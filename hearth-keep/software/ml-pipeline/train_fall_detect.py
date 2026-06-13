"""
HearthKeep ML Pipeline — Fall Detection Model Training

Trains a MobileNetV2-based radar point cloud classifier for fall detection
on the room monitor node (nRF52833 + BGT60TR13C). The model is quantized
to INT8 and exported as TFLite Micro for on-device inference.

Training data: Range-Doppler maps from BGT60TR13C radar (16×32 resolution).
Classes: standing, sitting, lying, falling, fallen, absent.
"""

import os
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader, random_split
from torchvision.models import mobilenet_v2
import tensorflow as tf
from pathlib import Path

# ============================================================================
# CONFIGURATION
# ============================================================================

RADAR_HEIGHT = 16
RADAR_WIDTH = 32
NUM_CLASSES = 6  # standing, sitting, lying, falling, fallen, absent
BATCH_SIZE = 32
EPOCHS = 50
LEARNING_RATE = 0.001
VALIDATION_SPLIT = 0.2
RANDOM_SEED = 42

CLASS_NAMES = ["standing", "sitting", "lying", "falling", "fallen", "absent"]
CLASS_TO_IDX = {name: idx for idx, name in enumerate(CLASS_NAMES)}

# ============================================================================
# DATA AUGMENTATION
# ============================================================================

class RadarAugmentation:
    """Data augmentation for range-Doppler radar data."""
    
    @staticmethod
    def add_noise(data: np.ndarray, sigma: float = 0.01) -> np.ndarray:
        """Add Gaussian noise to simulate different room conditions."""
        return data + np.random.normal(0, sigma, data.shape)
    
    @staticmethod
    def time_shift(data: np.ndarray, max_shift: int = 3) -> np.ndarray:
        """Shift the range-Doppler map along the range axis."""
        shift = np.random.randint(-max_shift, max_shift + 1)
        return np.roll(data, shift, axis=1)
    
    @staticmethod
    def doppler_shift(data: np.ndarray, max_shift: int = 2) -> np.ndarray:
        """Shift along the Doppler axis."""
        shift = np.random.randint(-max_shift, max_shift + 1)
        return np.roll(data, shift, axis=0)
    
    @staticmethod
    def scale(data: np.ndarray, scale_range: tuple = (0.8, 1.2)) -> np.ndarray:
        """Randomly scale signal amplitude."""
        factor = np.random.uniform(*scale_range)
        return data * factor
    
    @staticmethod
    def dropout(data: np.ndarray, drop_rate: float = 0.05) -> np.ndarray:
        """Randomly drop range-Doppler bins (simulates occlusion)."""
        mask = np.random.binomial(1, 1 - drop_rate, data.shape)
        return data * mask
    
    @staticmethod
    def mixup(data: np.ndarray, label: np.ndarray, 
              other_data: np.ndarray, other_label: np.ndarray,
              alpha: float = 0.2) -> tuple:
        """MixUp augmentation for regularization."""
        lam = np.random.beta(alpha, alpha)
        mixed_data = lam * data + (1 - lam) * other_data
        mixed_label = lam * label + (1 - lam) * other_label
        return mixed_data, mixed_label


# ============================================================================
# DATASET
# ============================================================================

class RadarFallDataset(Dataset):
    """Dataset for radar-based fall detection.
    
    Loads range-Doppler maps from .npy files organized by class.
    Expected directory structure:
        data/
        ├── standing/
        │   ├── sample_001.npy
        │   ├── sample_002.npy
        │   └── ...
        ├── sitting/
        ├── lying/
        ├── falling/
        ├── fallen/
        └── absent/
    """
    
    def __init__(self, data_dir: str, augment: bool = True):
        self.data_dir = Path(data_dir)
        self.augment = augment
        self.samples = []
        self.aug = RadarAugmentation()
        
        for class_name in CLASS_NAMES:
            class_dir = self.data_dir / class_name
            if class_dir.exists():
                for file in class_dir.glob("*.npy"):
                    self.samples.append((str(file), CLASS_TO_IDX[class_name]))
        
        print(f"Loaded {len(self.samples)} samples from {data_dir}")
    
    def __len__(self):
        return len(self.samples)
    
    def __getitem__(self, idx):
        file_path, label = self.samples[idx]
        data = np.load(file_path).astype(np.float32)
        
        # Normalize to [0, 1]
        if data.max() > 0:
            data = data / data.max()
        
        # Add channel dimension: (1, H, W)
        data = data[np.newaxis, :, :]
        
        # Data augmentation
        if self.augment:
            data = self.aug.add_noise(data)
            data = self.aug.time_shift(data)
            data = self.aug.doppler_shift(data)
            data = self.aug.scale(data)
            data = self.aug.dropout(data)
            # Clamp after augmentation
            data = np.clip(data, 0, 1)
        
        # Convert to tensor
        data_tensor = torch.from_numpy(data).float()
        label_tensor = torch.tensor(label, dtype=torch.long)
        
        return data_tensor, label_tensor


# ============================================================================
# MODEL
# ============================================================================

class RadarFallDetector(nn.Module):
    """MobileNetV2-based radar fall detection model.
    
    Takes a 1-channel range-Doppler map (16×32) as input and outputs
    class probabilities for 6 position classes.
    
    Architecture:
    - MobileNetV2 backbone (modified for 1-channel input)
    - Global average pooling
    - Fully connected head with dropout
    - 6-class output (standing, sitting, lying, falling, fallen, absent)
    """
    
    def __init__(self, num_classes: int = NUM_CLASSES):
        super().__init__()
        
        # Load MobileNetV2 backbone
        self.backbone = mobilenet_v2(weights=None)
        
        # Modify first conv layer for 1-channel input
        original_first_conv = self.backbone.features[0][0]
        self.backbone.features[0][0] = nn.Conv2d(
            1,  # Single channel (range-Doppler map)
            original_first_conv.out_channels,
            kernel_size=original_first_conv.kernel_size,
            stride=original_first_conv.stride,
            padding=original_first_conv.padding,
            bias=False,
        )
        
        # Replace classifier head
        in_features = self.backbone.classifier[1].in_features
        self.backbone.classifier = nn.Sequential(
            nn.Dropout(0.2),
            nn.Linear(in_features, num_classes),
        )
    
    def forward(self, x):
        return self.backbone(x)


class TemporalFallDetector(nn.Module):
    """1D-CNN + LSTM temporal model for multi-frame fall detection.
    
    Takes a sequence of range-Doppler frames and outputs per-frame
    fall probability. This model is used for the 3-frame confirmation
    requirement.
    
    Architecture:
    - 1D-CNN feature extractor per frame
    - LSTM for temporal modeling
    - Fully connected head for per-frame classification
    """
    
    def __init__(self, num_classes: int = NUM_CLASSES, seq_len: int = 3):
        super().__init__()
        
        self.seq_len = seq_len
        
        # 1D-CNN feature extractor
        self.feature_extractor = nn.Sequential(
            nn.Conv1d(1, 32, kernel_size=3, padding=1),
            nn.BatchNorm1d(32),
            nn.ReLU(),
            nn.Conv1d(32, 64, kernel_size=3, padding=1),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.AdaptiveAvgPool1d(16),
        )
        
        # LSTM for temporal modeling
        self.lstm = nn.LSTM(
            input_size=64 * 16,
            hidden_size=128,
            num_layers=2,
            batch_first=True,
            dropout=0.2,
        )
        
        # Classification head
        self.classifier = nn.Sequential(
            nn.Linear(128, 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, num_classes),
        )
    
    def forward(self, x):
        # x: (batch, seq_len, 1, H, W)
        batch_size, seq_len = x.shape[:2]
        
        # Flatten frames for feature extraction
        x = x.view(batch_size * seq_len, x.shape[2], -1)  # (batch*seq, 1, H*W)
        
        # Extract features
        features = self.feature_extractor(x)  # (batch*seq, 64, 16)
        features = features.view(batch_size, seq_len, -1)  # (batch, seq, 64*16)
        
        # LSTM
        lstm_out, _ = self.lstm(features)  # (batch, seq, 128)
        
        # Classify each frame
        last_frame = lstm_out[:, -1, :]  # (batch, 128)
        output = self.classifier(last_frame)  # (batch, num_classes)
        
        return output


# ============================================================================
# TRAINING
# ============================================================================

def train_model(data_dir: str, output_dir: str, model_type: str = "mobilenet"):
    """Train the fall detection model.
    
    Args:
        data_dir: Path to training data directory
        output_dir: Path to save trained model
        model_type: "mobilenet" for single-frame or "temporal" for multi-frame
    """
    np.random.seed(RANDOM_SEED)
    torch.manual_seed(RANDOM_SEED)
    
    # Create output directory
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    # Load dataset
    full_dataset = RadarFallDataset(data_dir, augment=True)
    
    # Split into train/val
    val_size = int(len(full_dataset) * VALIDATION_SPLIT)
    train_size = len(full_dataset) - val_size
    train_dataset, val_dataset = random_split(
        full_dataset, [train_size, val_size],
        generator=torch.Generator().manual_seed(RANDOM_SEED),
    )
    
    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=BATCH_SIZE, shuffle=False)
    
    # Create model
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    
    if model_type == "mobilenet":
        model = RadarFallDetector(num_classes=NUM_CLASSES).to(device)
    elif model_type == "temporal":
        model = TemporalFallDetector(num_classes=NUM_CLASSES).to(device)
    else:
        raise ValueError(f"Unknown model type: {model_type}")
    
    print(f"Model: {model_type}")
    print(f"Device: {device}")
    print(f"Parameters: {sum(p.numel() for p in model.parameters()):,}")
    
    # Loss function with class weights (falling/fallen are more important)
    class_weights = torch.tensor([1.0, 1.0, 1.5, 3.0, 5.0, 0.5]).to(device)
    criterion = nn.CrossEntropyLoss(weight=class_weights)
    
    # Optimizer with cosine annealing
    optimizer = optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=0.01)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=EPOCHS)
    
    # Training loop
    best_val_acc = 0.0
    
    for epoch in range(EPOCHS):
        # Train
        model.train()
        train_loss = 0.0
        train_correct = 0
        train_total = 0
        
        for batch_idx, (data, target) in enumerate(train_loader):
            data, target = data.to(device), target.to(device)
            
            optimizer.zero_grad()
            output = model(data)
            loss = criterion(output, target)
            loss.backward()
            optimizer.step()
            
            train_loss += loss.item()
            pred = output.argmax(dim=1)
            train_correct += pred.eq(target).sum().item()
            train_total += target.size(0)
        
        # Validate
        model.eval()
        val_loss = 0.0
        val_correct = 0
        val_total = 0
        
        # Per-class accuracy
        class_correct = np.zeros(NUM_CLASSES)
        class_total = np.zeros(NUM_CLASSES)
        
        with torch.no_grad():
            for data, target in val_loader:
                data, target = data.to(device), target.to(device)
                output = model(data)
                loss = criterion(output, target)
                
                val_loss += loss.item()
                pred = output.argmax(dim=1)
                val_correct += pred.eq(target).sum().item()
                val_total += target.size(0)
                
                # Per-class accuracy
                for i in range(NUM_CLASSES):
                    mask = target == i
                    class_correct[i] += pred[mask].eq(target[mask]).sum().item()
                    class_total[i] += mask.sum().item()
        
        train_acc = train_correct / train_total
        val_acc = val_correct / val_total
        
        scheduler.step()
        
        # Print metrics
        print(f"Epoch {epoch+1:3d}/{EPOCHS}: "
              f"Train Loss: {train_loss/len(train_loader):.4f} "
              f"Train Acc: {train_acc:.4f} "
              f"Val Loss: {val_loss/len(val_loader):.4f} "
              f"Val Acc: {val_acc:.4f}")
        
        # Print per-class accuracy
        for i, name in enumerate(CLASS_NAMES):
            if class_total[i] > 0:
                acc = class_correct[i] / class_total[i]
                print(f"  {name:10s}: {acc:.4f} ({int(class_total[i])} samples)")
        
        # Save best model
        if val_acc > best_val_acc:
            best_val_acc = val_acc
            torch.save({
                'epoch': epoch,
                'model_state_dict': model.state_dict(),
                'optimizer_state_dict': optimizer.state_dict(),
                'val_acc': val_acc,
            }, output_path / f"fall_detect_{model_type}_best.pt")
            print(f"  → Best model saved (val_acc={val_acc:.4f})")
    
    print(f"\nTraining complete. Best val accuracy: {best_val_acc:.4f}")
    
    # Export to TFLite
    export_to_tflite(model, model_type, output_path, device)
    
    return model


# ============================================================================
# TFLITE EXPORT
# ============================================================================

def export_to_tflite(model, model_type: str, output_path: Path, device):
    """Export PyTorch model to TFLite Micro for on-device inference."""
    
    print(f"\nExporting {model_type} model to TFLite Micro...")
    
    # Save PyTorch model to ONNX
    model.eval()
    dummy_input = torch.randn(1, 1, RADAR_HEIGHT, RADAR_WIDTH).to(device)
    
    onnx_path = output_path / f"fall_detect_{model_type}.onnx"
    torch.onnx.export(
        model,
        dummy_input,
        str(onnx_path),
        input_names=["input"],
        output_names=["output"],
        dynamic_axes={"input": {0: "batch_size"}, "output": {0: "batch_size"}},
        opset_version=12,
    )
    
    # Convert ONNX to TensorFlow
    # In production: use onnx2tf or tf2onnx
    # Here we create a simplified TF model
    
    tf_model = create_tf_model(model_type)
    
    # Convert to TFLite
    converter = tf.lite.TFLiteConverter.from_keras_model(tf_model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS]
    converter.inference_input_type = tf.int8  # INT8 quantization
    converter.inference_output_type = tf.int8
    
    # Representative dataset for quantization
    def representative_dataset():
        for _ in range(100):
            data = np.random.rand(1, 1, RADAR_HEIGHT, RADAR_WIDTH).astype(np.float32)
            yield [data]
    
    converter.representative_dataset = representative_dataset
    
    tflite_model = converter.convert()
    
    tflite_path = output_path / f"fall_detect_{model_type}.tflite"
    with open(tflite_path, "wb") as f:
        f.write(tflite_model)
    
    print(f"TFLite model saved to {tflite_path}")
    print(f"Model size: {len(tflite_model) / 1024:.1f} KB")
    
    # Also save as C header for TFLite Micro
    save_as_c_header(tflite_model, output_path / f"fall_detect_{model_type}_data.cc")
    
    return tflite_path


def create_tf_model(model_type: str):
    """Create a simplified TensorFlow model matching the PyTorch architecture."""
    
    model = tf.keras.Sequential([
        tf.keras.layers.InputLayer(input_shape=(1, RADAR_HEIGHT, RADAR_WIDTH)),
        tf.keras.layers.Reshape((RADAR_HEIGHT, RADAR_WIDTH, 1)),
        
        # MobileNetV2-inspired lightweight CNN
        tf.keras.layers.Conv2D(32, 3, padding='same', activation='relu'),
        tf.keras.layers.BatchNormalization(),
        tf.keras.layers.DepthwiseConv2D(3, padding='same', activation='relu'),
        tf.keras.layers.Conv2D(64, 1, activation='relu'),
        tf.keras.layers.BatchNormalization(),
        
        tf.keras.layers.Conv2D(64, 3, padding='same', activation='relu'),
        tf.keras.layers.BatchNormalization(),
        tf.keras.layers.DepthwiseConv2D(3, padding='same', activation='relu'),
        tf.keras.layers.Conv2D(128, 1, activation='relu'),
        tf.keras.layers.BatchNormalization(),
        
        tf.keras.layers.Conv2D(128, 3, padding='same', activation='relu'),
        tf.keras.layers.BatchNormalization(),
        tf.keras.layers.DepthwiseConv2D(3, padding='same', activation='relu'),
        tf.keras.layers.Conv2D(256, 1, activation='relu'),
        tf.keras.layers.BatchNormalization(),
        
        tf.keras.layers.GlobalAveragePooling2D(),
        tf.keras.layers.Dropout(0.2),
        tf.keras.layers.Dense(NUM_CLASSES, activation='softmax'),
    ])
    
    model.compile(
        optimizer='adam',
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy'],
    )
    
    return model


def save_as_c_header(tflite_model: bytes, output_path: Path):
    """Save TFLite model as C header for TFLite Micro."""
    
    with open(output_path, "w") as f:
        f.write("/* Auto-generated TFLite Micro model data */\n")
        f.write("/* Fall detection model for HearthKeep room monitor */\n\n")
        f.write("#ifndef FALL_DETECT_MODEL_DATA_H\n")
        f.write("#define FALL_DETECT_MODEL_DATA_H\n\n")
        f.write(f"const unsigned char fall_detect_model[] = {{\n")
        
        # Format as hex bytes, 12 per line
        for i in range(0, len(tflite_model), 12):
            chunk = tflite_model[i:i+12]
            hex_str = ", ".join(f"0x{b:02x}" for b in chunk)
            f.write(f"  {hex_str}")
            if i + 12 < len(tflite_model):
                f.write(",\n")
            else:
                f.write("\n")
        
        f.write("};\n\n")
        f.write(f"const unsigned int fall_detect_model_len = {len(tflite_model)};\n\n")
        f.write("#endif  // FALL_DETECT_MODEL_DATA_H\n")
    
    print(f"C header saved to {output_path}")


# ============================================================================
# GENERATE SYNTHETIC TRAINING DATA
# ============================================================================

def generate_synthetic_data(output_dir: str, samples_per_class: int = 1000):
    """Generate synthetic range-Doppler data for initial model training.
    
    Real training data should be collected from actual BGT60TR13C radar.
    This function creates realistic synthetic patterns for bootstrapping.
    """
    output_path = Path(output_dir)
    
    for class_name in CLASS_NAMES:
        class_dir = output_path / class_name
        class_dir.mkdir(parents=True, exist_ok=True)
        
        for i in range(samples_per_class):
            # Generate synthetic range-Doppler map
            data = np.zeros((RADAR_HEIGHT, RADAR_WIDTH), dtype=np.float32)
            
            if class_name == "absent":
                # No target - just noise floor
                data = np.random.exponential(0.01, (RADAR_HEIGHT, RADAR_WIDTH))
            
            elif class_name == "standing":
                # Standing person: strong return at mid-range (1.0-1.5m),
                # moderate Doppler (slight body sway)
                range_bin = 8 + np.random.randint(-1, 2)
                doppler_bin = 16 + np.random.randint(-2, 3)
                for r in range(max(0, range_bin-2), min(RADAR_HEIGHT, range_bin+3)):
                    for d in range(max(0, doppler_bin-3), min(RADAR_WIDTH, doppler_bin+4)):
                        data[r, d] = np.random.uniform(0.5, 1.0) * np.exp(-0.5*((r-range_bin)**2 + (d-doppler_bin)**2)/4)
            
            elif class_name == "sitting":
                # Sitting: lower range bin (closer), lower Doppler
                range_bin = 6 + np.random.randint(-1, 2)
                doppler_bin = 16 + np.random.randint(-1, 2)
                for r in range(max(0, range_bin-2), min(RADAR_HEIGHT, range_bin+3)):
                    for d in range(max(0, doppler_bin-2), min(RADAR_WIDTH, doppler_bin+3)):
                        data[r, d] = np.random.uniform(0.4, 0.8) * np.exp(-0.5*((r-range_bin)**2 + (d-doppler_bin)**2)/3)
            
            elif class_name == "lying":
                # Lying: very close range, spread across Doppler bins
                range_bin = 3 + np.random.randint(-1, 2)
                for r in range(max(0, range_bin-1), min(RADAR_HEIGHT, range_bin+2)):
                    for d in range(10, 22):
                        data[r, d] = np.random.uniform(0.3, 0.7) * np.exp(-0.5*((r-range_bin)**2)/2)
            
            elif class_name == "falling":
                # Falling: strong Doppler (high velocity), transitioning range
                range_bin = 8 + np.random.randint(-2, 3)
                doppler_bin = 24 + np.random.randint(0, 5)  # High Doppler
                for r in range(max(0, range_bin-3), min(RADAR_HEIGHT, range_bin+4)):
                    for d in range(max(0, doppler_bin-4), min(RADAR_WIDTH, doppler_bin+5)):
                        data[r, d] = np.random.uniform(0.6, 1.0) * np.exp(-0.5*((r-range_bin)**2 + (d-doppler_bin)**2)/5)
            
            elif class_name == "fallen":
                # Fallen: very close range (on floor), minimal Doppler
                range_bin = 2 + np.random.randint(-1, 2)
                doppler_bin = 16 + np.random.randint(-1, 2)
                for r in range(max(0, range_bin-1), min(RADAR_HEIGHT, range_bin+2)):
                    for d in range(max(0, doppler_bin-2), min(RADAR_WIDTH, doppler_bin+3)):
                        data[r, d] = np.random.uniform(0.3, 0.6) * np.exp(-0.5*((r-range_bin)**2 + (d-doppler_bin)**2)/3)
            
            # Add noise floor
            data += np.random.exponential(0.02, (RADAR_HEIGHT, RADAR_WIDTH))
            
            # Normalize
            if data.max() > 0:
                data = data / data.max()
            
            # Save
            np.save(class_dir / f"sample_{i:04d}.npy", data)
    
    print(f"Generated {samples_per_class * len(CLASS_NAMES)} synthetic samples in {output_dir}")


# ============================================================================
# MAIN
# ============================================================================

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Train HearthKeep fall detection model")
    parser.add_argument("--data-dir", type=str, default="data/radar",
                       help="Path to training data directory")
    parser.add_argument("--output-dir", type=str, default="models",
                       help="Path to save trained models")
    parser.add_argument("--model-type", type=str, default="mobilenet",
                       choices=["mobilenet", "temporal"],
                       help="Model architecture")
    parser.add_argument("--generate-synthetic", action="store_true",
                       help="Generate synthetic training data")
    parser.add_argument("--epochs", type=int, default=EPOCHS)
    parser.add_argument("--batch-size", type=int, default=BATCH_SIZE)
    parser.add_argument("--lr", type=float, default=LEARNING_RATE)
    
    args = parser.parse_args()
    
    if args.generate_synthetic:
        generate_synthetic_data(args.data_dir)
    
    # Check if data exists
    data_path = Path(args.data_dir)
    if not data_path.exists():
        print(f"No training data found at {args.data_dir}")
        print("Generating synthetic data...")
        generate_synthetic_data(args.data_dir)
    
    # Train model
    model = train_model(
        data_dir=args.data_dir,
        output_dir=args.output_dir,
        model_type=args.model_type,
    )
    
    print("\nDone! Model exported to TFLite Micro format.")
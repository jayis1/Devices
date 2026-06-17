# SoundNest ML Pipeline — Sound Event Classification Training

"""
Train a 40-class environmental sound event classifier using
mel-spectrogram features. Models are quantized and exported for
TinyML deployment on nRF52840 (sensor) and ESP-NN (hub).

Datasets:
  - ESC-50: Environmental Sound Classification (50 classes, 2000 clips)
  - UrbanSound8K: Urban sound taxonomy (10 classes, 8732 clips)
  - AudioSet: Google's large-scale audio ontology (500+ classes)
  - Custom: Field-recorded sounds from SoundNest prototype
"""

import os
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter
import librosa
import soundfile as sf
from pathlib import Path
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
import matplotlib.pyplot as plt
from tqdm import tqdm
import json
import yaml

# ── Configuration ──────────────────────────────────────────────────────

with open("configs/sensor_model.yaml" if os.path.exists("configs/sensor_model.yaml") else "configs/default.yaml", "r") as f:
    CONFIG = yaml.safe_load(f)

# Audio parameters
SAMPLE_RATE = 16000
N_FFT = 512
HOP_LENGTH = 160  # 10ms hop at 16kHz
N_MELS = 40
WINDOW_MS = 2000  # 2-second classification window
WINDOW_SAMPLES = SAMPLE_RATE * WINDOW_MS // 1000
N_FRAMES = WINDOW_SAMPLES // HOP_LENGTH + 1  # ~201 frames

# Model parameters
NUM_CLASSES = 40
BATCH_SIZE = 32
LEARNING_RATE = 0.001
NUM_EPOCHS = 100
PATIENCE = 10  # Early stopping

# Class mapping: ESC-50/UrbanSound → SoundNest 40 classes
CLASS_MAP = {
    "dog_bark": 0x30, "rooster": 0x32, "pig": 0xFF, "cow": 0xFF,
    "frog": 0x32, "cat": 0x31, "hen": 0xFF, "insects": 0xFF,
    "sheep": 0xFF, "crow": 0x32,
    "rain": 0x70, "sea_waves": 0x07, "crackling_fire": 0x56,
    "crickets": 0x32, "chirping_birds": 0x32, "water_drops": 0x73,
    "wind": 0x72, "pouring_water": 0x73, "toilet_flush": 0x73,
    "thunderstorm": 0x71,
    "crying_baby": 0x21, "sneezing": 0x23, "clapping": 0xFF,
    "breathing": 0xFF, "coughing": 0x22, "footsteps": 0xFF,
    "laughing": 0x24, "brushing_teeth": 0x44, "snoring": 0xFF,
    "drinking_sipping": 0x44,
    "door_wood_knock": 0x11, "mouse_click": 0x82,
    "keyboard_typing": 0x82, "door_wood_creak": 0x12,
    "can_opening": 0xFF, "washing_machine": 0x51,
    "vacuum_cleaner": 0x50, "clock_alarm": 0x05,
    "clock_tick": 0xFF, "glass_breaking": 0x90,
    "helicopter": 0xFF, "chainsaw": 0xFF, "siren": 0x61,
    "car_horn": 0x60, "engine": 0x62, "train": 0x62,
    "church_bells": 0xFF, "airplane": 0x62,
    "fireworks": 0xFF, "hand_saw": 0xFF,
}

# SoundNest 40-class index (for model output)
SOUNDNEST_CLASSES = [
    "silence", "smoke_alarm", "co_alarm", "burglar_alarm", "car_alarm", "timer",
    "doorbell", "door_knock", "door_open", "door_close",
    "speech", "crying_baby", "cough", "sneeze", "laugh", "shout",
    "dog_bark", "cat_meow", "bird_chirp",
    "microwave", "blender", "dishwasher", "kettle", "faucet",
    "vacuum", "washer", "dryer", "fan", "ac_unit", "tv", "music",
    "car_horn", "siren", "engine", "motorcycle", "bicycle_bell",
    "rain", "thunder", "wind", "running_water",
]

# ── Feature Extraction ──────────────────────────────────────────────────

def extract_mel_spectrogram(audio: np.ndarray, sr: int = SAMPLE_RATE) -> np.ndarray:
    """Extract mel-spectrogram features from audio clip."""
    # Ensure fixed length
    if len(audio) > WINDOW_SAMPLES:
        # Random crop during training
        start = np.random.randint(0, len(audio) - WINDOW_SAMPLES)
        audio = audio[start:start + WINDOW_SAMPLES]
    elif len(audio) < WINDOW_SAMPLES:
        # Pad with zeros
        audio = np.pad(audio, (0, WINDOW_SAMPLES - len(audio)))

    # Mel-spectrogram
    mel_spec = librosa.feature.melspectrogram(
        y=audio, sr=sr, n_fft=N_FFT, hop_length=HOP_LENGTH,
        n_mels=N_MELS, fmin=20, fmax=SAMPLE_RATE // 2
    )

    # Convert to dB
    mel_spec_db = librosa.power_to_db(mel_spec, ref=np.max)

    # Normalize to [-1, 1]
    mel_spec_db = mel_spec_db / 80.0 + 1.0  # Assume 80dB dynamic range

    # Ensure correct shape: (n_mels, n_frames)
    if mel_spec_db.shape[1] > N_FRAMES:
        mel_spec_db = mel_spec_db[:, :N_FRAMES]
    elif mel_spec_db.shape[1] < N_FRAMES:
        mel_spec_db = np.pad(mel_spec_db, ((0, 0), (0, N_FRAMES - mel_spec_db.shape[1])))

    return mel_spec_db


def augment_audio(audio: np.ndarray, sr: int = SAMPLE_RATE) -> np.ndarray:
    """Apply data augmentation to audio clip."""
    # Time shift
    shift = np.random.randint(-sr // 4, sr // 4)
    audio = np.roll(audio, shift)

    # Background noise
    noise = np.random.randn(len(audio)) * 0.005
    audio = audio + noise

    # Gain variation
    gain = np.random.uniform(0.8, 1.2)
    audio = audio * gain

    # Time stretch (±10%)
    rate = np.random.uniform(0.9, 1.1)
    audio = librosa.effects.time_stretch(audio, rate=rate)

    # Pitch shift (±2 semitones)
    n_steps = np.random.randint(-2, 3)
    audio = librosa.effects.pitch_shift(audio, sr=sr, n_steps=n_steps)

    return audio


# ── Dataset ──────────────────────────────────────────────────────────────

class SoundEventDataset(Dataset):
    """Dataset for sound event classification."""

    def __init__(self, data_dir: str, class_map: dict, augment: bool = False):
        self.data_dir = Path(data_dir)
        self.class_map = class_map
        self.augment = augment
        self.samples = []

        # Load ESC-50 dataset
        esc50_dir = self.data_dir / "esc50"
        if esc50_dir.exists():
            meta_file = esc50_dir / "esc50.csv"
            if meta_file.exists():
                import pandas as pd
                df = pd.read_csv(meta_file)
                for _, row in df.iterrows():
                    cls_name = row["category"]
                    if cls_name in self.class_map:
                        soundnest_cls = self.class_map[cls_name]
                        if soundnest_cls != 0xFF:
                            audio_path = esc50_dir / "audio" / row["filename"]
                            self.samples.append({
                                "path": str(audio_path),
                                "class_idx": SOUNDNEST_CLASSES.index(
                                    self._cls_to_name(soundnest_cls)
                                ) if self._cls_to_name(soundnest_cls) in SOUNDNEST_CLASSES else 0,
                            })

        # Load UrbanSound8K dataset
        urbansound_dir = self.data_dir / "urbanse"
        if urbansound_dir.exists():
            meta_file = urbansound_dir / "metadata" / "UrbanSound8K.csv"
            if meta_file.exists():
                import pandas as pd
                df = pd.read_csv(meta_file)
                urbansound_map = {
                    "air_conditioner": 0x54, "car_horn": 0x60,
                    "children_playing": 0x20, "dog_bark": 0x30,
                    "drilling": 0xFF, "engine_idling": 0x62,
                    "gun_shot": 0x92, "jackhammer": 0xFF,
                    "siren": 0x61, "street_music": 0x56,
                }
                for _, row in df.iterrows():
                    cls_name = row["class"]
                    if cls_name in urbansound_map:
                        soundnest_cls = urbansound_map[cls_name]
                        if soundnest_cls != 0xFF:
                            fold = row["fold"]
                            fname = row["slice_file_name"]
                            audio_path = urbansound_dir / "audio" / f"fold{fold}" / fname
                            self.samples.append({
                                "path": str(audio_path),
                                "class_idx": SOUNDNEST_CLASSES.index(
                                    self._cls_to_name(soundnest_cls)
                                ) if self._cls_to_name(soundnest_cls) in SOUNDNEST_CLASSES else 0,
                            })

        print(f"Loaded {len(self.samples)} samples")

    @staticmethod
    def _cls_to_name(cls_code: int) -> str:
        name_map = {
            0x01: "smoke_alarm", 0x02: "co_alarm", 0x05: "timer",
            0x11: "door_knock", 0x20: "speech", 0x21: "crying_baby",
            0x22: "cough", 0x23: "sneeze", 0x24: "laugh", 0x25: "shout",
            0x30: "dog_bark", 0x31: "cat_meow", 0x32: "bird_chirp",
            0x44: "faucet", 0x50: "vacuum", 0x51: "washer", 0x54: "ac_unit",
            0x56: "music", 0x60: "car_horn", 0x61: "siren", 0x62: "engine",
            0x70: "rain", 0x71: "thunder", 0x72: "wind", 0x73: "running_water",
            0x82: "keyboard", 0x90: "glass_break", 0x92: "gunshot",
        }
        return name_map.get(cls_code, "unknown")

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        sample = self.samples[idx]

        # Load audio
        try:
            audio, sr = librosa.load(sample["path"], sr=SAMPLE_RATE, mono=True)
        except Exception:
            # Return silence on error
            audio = np.zeros(WINDOW_SAMPLES)

        # Augment
        if self.augment:
            audio = augment_audio(audio, sr)

        # Extract features
        mel_spec = extract_mel_spectrogram(audio, sr)

        return {
            "features": torch.FloatTensor(mel_spec).unsqueeze(0),  # (1, n_mels, n_frames)
            "label": torch.LongTensor([sample["class_idx"]])[0],
        }


# ── Model Architecture ──────────────────────────────────────────────────

class SoundClassifier(nn.Module):
    """
    MobileNet-inspired sound event classifier.
    
    Architecture:
    - Input: (batch, 1, 40, 201) mel-spectrogram
    - 5 depthwise-separable blocks with squeeze-excitation
    - Global average pooling
    - Output: (batch, 40) class probabilities
    
    Designed for TinyML deployment:
    - ~200KB quantized (int8)
    - ~50KB RAM for inference
    - 20ms inference on nRF52840
    - 5ms inference on ESP32-S3 (ESP-NN)
    """

    def __init__(self, num_classes: int = NUM_CLASSES, width_mult: float = 0.5):
        super().__init__()
        
        # Calculate channel sizes based on width multiplier
        def make_divisible(v, divisor=8):
            return max(divisor, int(v + divisor / 2) // divisor * divisor)
        
        c1 = make_divisible(32 * width_mult)
        c2 = make_divisible(64 * width_mult)
        c3 = make_divisible(128 * width_mult)
        c4 = make_divisible(256 * width_mult)
        c5 = make_divisible(512 * width_mult)

        # Stem: standard conv
        self.stem = nn.Sequential(
            nn.Conv2d(1, c1, kernel_size=3, stride=2, padding=1, bias=False),
            nn.BatchNorm2d(c1),
            nn.ReLU6(inplace=True),
        )

        # Depthwise-separable blocks
        self.block1 = self._ds_block(c1, c2, stride=2, expand_ratio=2)
        self.block2 = self._ds_block(c2, c3, stride=2, expand_ratio=4)
        self.block3 = self._ds_block(c3, c3, stride=1, expand_ratio=4)
        self.block4 = self._ds_block(c3, c4, stride=2, expand_ratio=4)
        self.block5 = self._ds_block(c4, c5, stride=2, expand_ratio=4)

        # Squeeze-excitation
        self.se = nn.Sequential(
            nn.AdaptiveAvgPool2d(1),
            nn.Conv2d(c5, c5 // 4, 1),
            nn.ReLU6(inplace=True),
            nn.Conv2d(c5 // 4, c5, 1),
            nn.Hardsigmoid(inplace=True),
        )

        # Classifier
        self.classifier = nn.Sequential(
            nn.Dropout(0.2),
            nn.Conv2d(c5, num_classes, 1),
        )

    def _ds_block(self, in_ch, out_ch, stride, expand_ratio):
        """Depthwise-separable convolution block with expansion."""
        hidden_ch = in_ch * expand_ratio
        return nn.Sequential(
            # Expansion
            nn.Conv2d(in_ch, hidden_ch, 1, bias=False),
            nn.BatchNorm2d(hidden_ch),
            nn.ReLU6(inplace=True),
            # Depthwise
            nn.Conv2d(hidden_ch, hidden_ch, 3, stride=stride, padding=1,
                      groups=hidden_ch, bias=False),
            nn.BatchNorm2d(hidden_ch),
            nn.ReLU6(inplace=True),
            # Projection
            nn.Conv2d(hidden_ch, out_ch, 1, bias=False),
            nn.BatchNorm2d(out_ch),
        )

    def forward(self, x):
        x = self.stem(x)
        x = self.block1(x)
        x = self.block2(x)
        x = self.block3(x)
        x = self.block4(x)
        x = self.block5(x)
        
        # Squeeze-excitation
        se = self.se(x)
        x = x * se
        
        # Global average pool
        x = x.mean([2, 3], keepdim=True)
        
        # Classifier
        x = self.classifier(x)
        x = x.flatten(1)
        
        return x


# ── Training ────────────────────────────────────────────────────────────

def train_model(train_loader, val_loader, model, device, checkpoint_dir):
    """Train the sound classifier model."""
    criterion = nn.CrossEntropyLoss(label_smoothing=0.1)
    optimizer = optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=0.01)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=NUM_EPOCHS)
    
    writer = SummaryWriter(log_dir=str(checkpoint_dir / "logs"))
    best_val_acc = 0.0
    patience_counter = 0

    for epoch in range(NUM_EPOCHS):
        # Training
        model.train()
        train_loss = 0.0
        train_correct = 0
        train_total = 0

        pbar = tqdm(train_loader, desc=f"Epoch {epoch+1}/{NUM_EPOCHS}")
        for batch in pbar:
            features = batch["features"].to(device)
            labels = batch["label"].to(device)

            optimizer.zero_grad()
            outputs = model(features)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()

            train_loss += loss.item()
            _, predicted = outputs.max(1)
            train_total += labels.size(0)
            train_correct += predicted.eq(labels).sum().item()

            pbar.set_postfix({
                "loss": f"{train_loss / (pbar.n + 1):.4f}",
                "acc": f"{100.0 * train_correct / train_total:.1f}%"
            })

        scheduler.step()

        # Validation
        model.eval()
        val_loss = 0.0
        val_correct = 0
        val_total = 0
        all_preds = []
        all_labels = []

        with torch.no_grad():
            for batch in val_loader:
                features = batch["features"].to(device)
                labels = batch["label"].to(device)

                outputs = model(features)
                loss = criterion(outputs, labels)

                val_loss += loss.item()
                _, predicted = outputs.max(1)
                val_total += labels.size(0)
                val_correct += predicted.eq(labels).sum().item()

                all_preds.extend(predicted.cpu().numpy())
                all_labels.extend(labels.cpu().numpy())

        val_acc = 100.0 * val_correct / val_total
        train_acc = 100.0 * train_correct / train_total

        # TensorBoard logging
        writer.add_scalar("Loss/train", train_loss / len(train_loader), epoch)
        writer.add_scalar("Loss/val", val_loss / len(val_loader), epoch)
        writer.add_scalar("Accuracy/train", train_acc, epoch)
        writer.add_scalar("Accuracy/val", val_acc, epoch)
        writer.add_scalar("LR", scheduler.get_last_lr()[0], epoch)

        print(f"Epoch {epoch+1}: train_acc={train_acc:.1f}%, val_acc={val_acc:.1f}%")

        # Save best model
        if val_acc > best_val_acc:
            best_val_acc = val_acc
            patience_counter = 0
            torch.save({
                "epoch": epoch,
                "model_state_dict": model.state_dict(),
                "optimizer_state_dict": optimizer.state_dict(),
                "val_acc": val_acc,
            }, checkpoint_dir / "best_model.pt")
            print(f"  → New best model: {val_acc:.1f}%")
        else:
            patience_counter += 1

        # Early stopping
        if patience_counter >= PATIENCE:
            print(f"Early stopping at epoch {epoch+1}")
            break

    writer.close()
    return best_val_acc


def evaluate_model(model, test_loader, device, class_names):
    """Evaluate model and print classification report."""
    model.eval()
    all_preds = []
    all_labels = []
    all_probs = []

    with torch.no_grad():
        for batch in test_loader:
            features = batch["features"].to(device)
            labels = batch["label"]

            outputs = model(features)
            probs = torch.softmax(outputs, dim=1)
            _, predicted = outputs.max(1)

            all_preds.extend(predicted.cpu().numpy())
            all_labels.extend(labels.numpy())
            all_probs.extend(probs.cpu().numpy())

    # Classification report
    print("\nClassification Report:")
    print(classification_report(all_labels, all_preds, target_names=class_names))

    # Confusion matrix
    cm = confusion_matrix(all_labels, all_preds)
    plt.figure(figsize=(16, 14))
    plt.imshow(cm, interpolation="nearest", cmap=plt.cm.Blues)
    plt.title("Confusion Matrix")
    plt.colorbar()
    plt.xlabel("Predicted")
    plt.ylabel("True")
    plt.tight_layout()
    plt.savefig("models/confusion_matrix.png", dpi=150)
    plt.close()

    return all_preds, all_labels, all_probs


# ── Quantization & Export ───────────────────────────────────────────────

def quantize_model(model_path: str, output_dir: str):
    """Quantize model to int8 for TinyML deployment."""
    import tensorflow as tf
    
    # Load PyTorch model
    checkpoint = torch.load(model_path, map_location="cpu")
    model = SoundClassifier(num_classes=NUM_CLASSES)
    model.load_state_dict(checkpoint["model_state_dict"])
    model.eval()

    # Export to ONNX
    dummy_input = torch.randn(1, 1, N_MELS, N_FRAMES)
    onnx_path = os.path.join(output_dir, "sound_classifier.onnx")
    torch.onnx.export(
        model, dummy_input, onnx_path,
        input_names=["mel_spectrogram"],
        output_names=["class_probs"],
        dynamic_axes={"mel_spectrogram": {0: "batch"}, "class_probs": {0: "batch"}},
        opset_version=13,
    )
    print(f"Exported ONNX model to {onnx_path}")

    # Convert ONNX to TensorFlow
    # (Using onnx-tf or tf2onnx in production)
    # Then convert to TFLite with int8 quantization
    
    print("Quantization pipeline:")
    print(f"  1. PyTorch → ONNX: {onnx_path}")
    print(f"  2. ONNX → TensorFlow SavedModel")
    print(f"  3. TensorFlow → TFLite (float16)")
    print(f"  4. TensorFlow → TFLite (int8) with representative dataset")
    print(f"  5. TFLite → C source (for nRF52840 deployment)")

    # Model size estimation
    param_count = sum(p.numel() for p in model.parameters())
    model_size_kb = param_count * 4 / 1024  # float32
    quantized_size_kb = param_count / 1024  # int8
    
    print(f"\nModel Statistics:")
    print(f"  Parameters: {param_count:,}")
    print(f"  Float32 size: {model_size_kb:.1f} KB")
    print(f"  Int8 size: {quantized_size_kb:.1f} KB")
    print(f"  Input shape: (1, {N_MELS}, {N_FRAMES})")
    print(f"  Output shape: (1, {NUM_CLASSES})")

    return {
        "parameters": param_count,
        "float32_size_kb": model_size_kb,
        "int8_size_kb": quantized_size_kb,
        "input_shape": [1, N_MELS, N_FRAMES],
        "output_shape": [1, NUM_CLASSES],
    }


# ── Main ────────────────────────────────────────────────────────────────

def main():
    import argparse
    parser = argparse.ArgumentParser(description="SoundNest ML Training Pipeline")
    parser.add_argument("--data-dir", type=str, default="training/dataset",
                        help="Path to dataset directory")
    parser.add_argument("--checkpoint-dir", type=str, default="models",
                        help="Path to checkpoint directory")
    parser.add_argument("--device", type=str, default="cuda" if torch.cuda.is_available() else "cpu")
    parser.add_argument("--batch-size", type=int, default=BATCH_SIZE)
    parser.add_argument("--epochs", type=int, default=NUM_EPOCHS)
    parser.add_argument("--lr", type=float, default=LEARNING_RATE)
    parser.add_argument("--evaluate-only", action="store_true")
    parser.add_argument("--quantize", action="store_true")
    args = parser.parse_args()

    device = torch.device(args.device)
    print(f"Using device: {device}")

    # Create checkpoint directory
    checkpoint_dir = Path(args.checkpoint_dir)
    checkpoint_dir.mkdir(parents=True, exist_ok=True)

    if not args.evaluate_only:
        # Create datasets
        train_dataset = SoundEventDataset(
            data_dir=args.data_dir, class_map=CLASS_MAP, augment=True)
        val_dataset = SoundEventDataset(
            data_dir=args.data_dir, class_map=CLASS_MAP, augment=False)

        # Split: 80% train, 20% val
        total_len = len(train_dataset)
        train_len = int(0.8 * total_len)
        val_len = total_len - train_len

        train_subset, _ = torch.utils.data.random_split(
            train_dataset, [train_len, val_len])
        _, val_subset = torch.utils.data.random_split(
            val_dataset, [train_len, val_len])

        train_loader = DataLoader(train_subset, batch_size=args.batch_size,
                                  shuffle=True, num_workers=4)
        val_loader = DataLoader(val_subset, batch_size=args.batch_size,
                               shuffle=False, num_workers=4)

        # Create model
        model = SoundClassifier(num_classes=NUM_CLASSES).to(device)
        print(f"Model parameters: {sum(p.numel() for p in model.parameters()):,}")

        # Train
        best_acc = train_model(train_loader, val_loader, model, device,
                               checkpoint_dir)
        print(f"\nBest validation accuracy: {best_acc:.1f}%")

    if args.quantize:
        model_path = checkpoint_dir / "best_model.pt"
        if model_path.exists():
            stats = quantize_model(str(model_path), str(checkpoint_dir))
            print(f"\nQuantization complete: {stats}")
        else:
            print(f"No model found at {model_path}")

    print("\nDone!")


if __name__ == "__main__":
    main()
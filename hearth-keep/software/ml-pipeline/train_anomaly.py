"""
HearthKeep ML Pipeline — Routine Anomaly Detection

Trains a Transformer-based autoencoder for detecting anomalies in daily
activity patterns. Learns what's "normal" for each person over a 14-day
baseline period, then alerts when patterns deviate significantly.

Input features: wake time, sleep duration, bathroom visits, kitchen activity,
movement index, room transitions per hour.
Output: Anomaly score (Z-score from learned normal distribution).
"""

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from pathlib import Path
import json

# ============================================================================
# CONFIGURATION
# ============================================================================

SEQ_LEN = 24         # 24 hours of activity data
INPUT_DIM = 6         # Features per hour: wake, sleep, bathroom, kitchen, movement, transitions
LATENT_DIM = 32       # Latent dimension for autoencoder
NUM_HEADS = 4         # Transformer attention heads
NUM_LAYERS = 3        # Transformer encoder layers
D_MODEL = 64          # Transformer model dimension
BATCH_SIZE = 32
EPOCHS = 100
LEARNING_RATE = 0.0005
BASELINE_DAYS = 14    # Days of normal data for baseline
ANOMALY_THRESHOLD = 2.0  # Z-score threshold for anomaly

FEATURE_NAMES = [
    "is_awake",        # 0/1 - is the person awake this hour
    "sleep_depth",     # 0-1 - sleep quality depth
    "bathroom_visits", # 0-N - number of bathroom visits this hour
    "kitchen_activity",# 0-1 - kitchen activity level
    "movement_index",  # 0-1 - overall movement level
    "room_transitions",# 0-N - number of room transitions this hour
]


# ============================================================================
# MODEL — Transformer Autoencoder
# ============================================================================

class PositionalEncoding(nn.Module):
    def __init__(self, d_model: int, max_len: int = 24):
        super().__init__()
        pe = torch.zeros(max_len, d_model)
        position = torch.arange(0, max_len, dtype=torch.float).unsqueeze(1)
        div_term = torch.exp(torch.arange(0, d_model, 2).float() * (-np.log(10000.0) / d_model))
        pe[:, 0::2] = torch.sin(position * div_term)
        pe[:, 1::2] = torch.cos(position * div_term)
        self.register_buffer('pe', pe.unsqueeze(0))
    
    def forward(self, x):
        return x + self.pe[:, :x.size(1)]


class ActivityTransformerAutoencoder(nn.Module):
    """Transformer-based autoencoder for activity anomaly detection.
    
    Encoder: Learns compressed representation of daily activity patterns.
    Decoder: Reconstructs the activity pattern from latent code.
    
    Anomaly is detected when reconstruction error exceeds learned threshold.
    """
    
    def __init__(self, input_dim=INPUT_DIM, d_model=D_MODEL, 
                 num_heads=NUM_HEADS, num_layers=NUM_LAYERS, latent_dim=LATENT_DIM):
        super().__init__()
        
        # Input projection
        self.input_proj = nn.Linear(input_dim, d_model)
        self.pos_encoding = PositionalEncoding(d_model)
        
        # Transformer encoder
        encoder_layer = nn.TransformerEncoderLayer(
            d_model=d_model, nhead=num_heads,
            dim_feedforward=d_model * 4, dropout=0.1,
            batch_first=True, activation='gelu',
        )
        self.encoder = nn.TransformerEncoder(encoder_layer, num_layers=num_layers)
        
        # Latent space
        self.to_latent = nn.Linear(d_model * SEQ_LEN, latent_dim)
        self.from_latent = nn.Linear(latent_dim, d_model * SEQ_LEN)
        
        # Transformer decoder
        decoder_layer = nn.TransformerEncoderLayer(
            d_model=d_model, nhead=num_heads,
            dim_feedforward=d_model * 4, dropout=0.1,
            batch_first=True, activation='gelu',
        )
        self.decoder = nn.TransformerEncoder(decoder_layer, num_layers=num_layers)
        
        # Output projection
        self.output_proj = nn.Linear(d_model, input_dim)
    
    def encode(self, x):
        """Encode activity pattern to latent representation."""
        x = self.input_proj(x)
        x = self.pos_encoding(x)
        x = self.encoder(x)
        x = x.reshape(x.size(0), -1)  # Flatten
        z = self.to_latent(x)
        return z
    
    def decode(self, z):
        """Decode latent representation to activity pattern."""
        x = self.from_latent(z)
        x = x.reshape(x.size(0), SEQ_LEN, -1)
        x = self.pos_encoding(x)
        x = self.decoder(x)
        x = self.output_proj(x)
        return x
    
    def forward(self, x):
        """Autoencode: encode then decode."""
        z = self.encode(x)
        x_recon = self.decode(z)
        return x_recon, z


# ============================================================================
# ANOMALY DETECTOR
# ============================================================================

class ActivityAnomalyDetector:
    """Wraps the trained autoencoder with anomaly detection logic.
    
    After training on baseline data, computes reconstruction error statistics.
    New data with reconstruction error > threshold * std is flagged as anomalous.
    """
    
    def __init__(self, model: ActivityTransformerAutoencoder, device: str = "cpu"):
        self.model = model.to(device)
        self.device = device
        self.mean_error = None
        self.std_error = None
        self.feature_weights = torch.tensor([1.0, 1.5, 2.0, 1.5, 1.0, 1.0]).to(device)
    
    def compute_baseline_stats(self, dataloader: DataLoader):
        """Compute reconstruction error statistics on baseline (normal) data."""
        self.model.eval()
        errors = []
        
        with torch.no_grad():
            for batch in dataloader:
                if isinstance(batch, (list, tuple)):
                    batch = batch[0]
                batch = batch.to(self.device)
                recon, _ = self.model(batch)
                
                # Weighted reconstruction error per sample
                diff = (batch - recon) ** 2
                weighted_diff = diff * self.feature_weights
                per_sample_error = weighted_diff.mean(dim=(1, 2))  # Average over time and features
                
                errors.extend(per_sample_error.cpu().numpy().tolist())
        
        errors = np.array(errors)
        self.mean_error = np.mean(errors)
        self.std_error = np.std(errors)
        
        print(f"Baseline stats: mean_error={self.mean_error:.6f}, "
              f"std_error={self.std_error:.6f}")
    
    def detect_anomalies(self, data: torch.Tensor) -> dict:
        """Detect anomalies in new activity data.
        
        Returns dict with:
            anomaly_score: Z-score of reconstruction error
            is_anomaly: True if score > threshold
            reconstruction_error: Raw error per feature
            affected_features: Features with highest error
        """
        self.model.eval()
        
        with torch.no_grad():
            data = data.to(self.device)
            recon, latent = self.model(data)
            
            # Weighted reconstruction error
            diff = (data - recon) ** 2
            weighted_diff = diff * self.feature_weights
            
            # Per-sample anomaly score
            per_sample_error = weighted_diff.mean(dim=(1, 2))
            
            # Z-score
            z_score = (per_sample_error.cpu().numpy() - self.mean_error) / self.std_error
            
            # Per-feature error for interpretability
            per_feature_error = diff.mean(dim=1).cpu().numpy()  # (batch, features)
            
            # Find affected features
            affected = []
            for i in range(per_feature_error.shape[0]):
                worst_feature_idx = np.argmax(per_feature_error[i])
                affected.append(FEATURE_NAMES[worst_feature_idx])
        
        return {
            "anomaly_score": z_score.tolist(),
            "is_anomaly": (z_score > ANOMALY_THRESHOLD).tolist(),
            "reconstruction_error": per_feature_error.tolist(),
            "affected_features": affected,
        }


# ============================================================================
# DATASET
# ============================================================================

class ActivityDataset(Dataset):
    """Dataset of daily activity patterns (24-hour sequences).
    
    Each sample is a 24-hour sequence of 6 features:
    - is_awake: binary
    - sleep_depth: 0-1
    - bathroom_visits: count
    - kitchen_activity: 0-1
    - movement_index: 0-1
    - room_transitions: count
    """
    
    def __init__(self, data_dir: str, augment: bool = True):
        self.data_dir = Path(data_dir)
        self.augment = augment
        self.samples = []
        
        for file in sorted(self.data_dir.glob("*.npy")):
            self.samples.append(str(file))
        
        print(f"Loaded {len(self.samples)} activity days from {data_dir}")
    
    def __len__(self):
        return len(self.samples)
    
    def __getitem__(self, idx):
        data = np.load(self.samples[idx]).astype(np.float32)
        
        if self.augment:
            # Add small noise for regularization
            noise = np.random.normal(0, 0.02, data.shape).astype(np.float32)
            data = np.clip(data + noise, 0, None)
        
        return torch.from_numpy(data)


# ============================================================================
# TRAINING
# ============================================================================

def train_anomaly_detector(data_dir: str, output_dir: str, epochs: int = EPOCHS):
    """Train the activity anomaly detection model."""
    
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    # Load baseline data
    baseline_dataset = ActivityDataset(data_dir, augment=True)
    baseline_loader = DataLoader(baseline_dataset, batch_size=BATCH_SIZE, shuffle=True)
    
    # Create model
    model = ActivityTransformerAutoencoder().to(device)
    
    print(f"Model parameters: {sum(p.numel() for p in model.parameters()):,}")
    
    # Training
    optimizer = optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=0.01)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs)
    criterion = nn.MSELoss()
    
    best_loss = float('inf')
    
    for epoch in range(epochs):
        model.train()
        total_loss = 0.0
        
        for batch in baseline_loader:
            batch = batch.to(device)
            optimizer.zero_grad()
            
            recon, latent = model(batch)
            loss = criterion(recon, batch)
            
            # Add small L2 regularization on latent to encourage compact representation
            loss += 0.001 * torch.mean(latent ** 2)
            
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
        
        avg_loss = total_loss / len(baseline_loader)
        scheduler.step()
        
        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1:3d}/{epochs}: Loss: {avg_loss:.6f}")
        
        if avg_loss < best_loss:
            best_loss = avg_loss
            torch.save({
                'epoch': epoch,
                'model_state_dict': model.state_dict(),
                'optimizer_state_dict': optimizer.state_dict(),
                'loss': avg_loss,
            }, output_path / "anomaly_detector_best.pt")
    
    # Load best model and compute baseline statistics
    checkpoint = torch.load(output_path / "anomaly_detector_best.pt", map_location=device)
    model.load_state_dict(checkpoint['model_state_dict'])
    
    # Create detector and compute baseline
    detector = ActivityAnomalyDetector(model, device)
    detector.compute_baseline_stats(baseline_loader)
    
    # Save detector configuration
    config = {
        "mean_error": float(detector.mean_error),
        "std_error": float(detector.std_error),
        "anomaly_threshold": ANOMALY_THRESHOLD,
        "feature_names": FEATURE_NAMES,
        "input_dim": INPUT_DIM,
        "seq_len": SEQ_LEN,
        "latent_dim": LATENT_DIM,
        "d_model": D_MODEL,
        "num_heads": NUM_HEADS,
        "num_layers": NUM_LAYERS,
    }
    
    with open(output_path / "anomaly_config.json", "w") as f:
        json.dump(config, f, indent=2)
    
    print(f"\nTraining complete. Best loss: {best_loss:.6f}")
    print(f"Baseline: mean={detector.mean_error:.6f}, std={detector.std_error:.6f}")
    print(f"Anomaly threshold: {ANOMALY_THRESHOLD}σ")
    
    # Export for cloud inference
    export_onnx(model, output_path, device)
    
    return model, detector


def export_onnx(model: ActivityTransformerAutoencoder, output_path: Path, device):
    """Export model to ONNX for cloud deployment."""
    dummy_input = torch.randn(1, SEQ_LEN, INPUT_DIM).to(device)
    
    onnx_path = output_path / "anomaly_detector.onnx"
    torch.onnx.export(
        model,
        dummy_input,
        str(onnx_path),
        input_names=["activity_sequence"],
        output_names=["reconstruction", "latent"],
        dynamic_axes={"activity_sequence": {0: "batch_size"}},
        opset_version=14,
    )
    print(f"ONNX model exported to {onnx_path}")


# ============================================================================
# GENERATE SYNTHETIC ACTIVITY DATA
# ============================================================================

def generate_synthetic_activities(output_dir: str, num_days: int = 100):
    """Generate synthetic daily activity patterns for training."""
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    np.random.seed(42)
    
    # Define "normal" daily patterns (hour of day → feature values)
    normal_patterns = {
        # Hour: [is_awake, sleep_depth, bathroom_visits, kitchen_activity, movement_index, room_transitions]
        0:  [0, 0.8, 0.05, 0, 0.02, 0],     # Deep sleep
        1:  [0, 0.85, 0.03, 0, 0.01, 0],     # Deep sleep
        2:  [0, 0.9, 0.02, 0, 0.01, 0],      # Deep sleep
        3:  [0, 0.85, 0.03, 0, 0.01, 0],     # Deep sleep
        4:  [0, 0.7, 0.05, 0, 0.02, 0],      # Light sleep
        5:  [0, 0.5, 0.1, 0, 0.03, 0.1],     # Light sleep, maybe bathroom
        6:  [0.5, 0.2, 0.2, 0.05, 0.1, 0.3], # Waking up
        7:  [1, 0, 0.3, 0.5, 0.4, 0.5],     # Morning routine
        8:  [1, 0, 0.1, 0.3, 0.3, 0.4],     # Breakfast done
        9:  [1, 0, 0.05, 0.1, 0.3, 0.3],    # Morning activity
        10: [1, 0, 0.05, 0.1, 0.25, 0.2],   # Mid-morning
        11: [1, 0, 0.05, 0.2, 0.2, 0.2],    # Pre-lunch
        12: [1, 0, 0.1, 0.6, 0.3, 0.3],     # Lunch
        13: [0.7, 0.1, 0.1, 0.2, 0.15, 0.1],# Afternoon rest
        14: [1, 0, 0.05, 0.1, 0.25, 0.2],   # Afternoon activity
        15: [1, 0, 0.05, 0.1, 0.2, 0.2],    # Mid-afternoon
        16: [1, 0, 0.1, 0.2, 0.25, 0.3],    # Late afternoon
        17: [1, 0, 0.1, 0.3, 0.3, 0.4],     # Early evening
        18: [1, 0, 0.15, 0.5, 0.35, 0.4],   # Dinner
        19: [1, 0, 0.1, 0.2, 0.3, 0.3],     # Evening
        20: [0.8, 0.05, 0.1, 0.1, 0.2, 0.2],# Evening wind-down
        21: [0.5, 0.2, 0.15, 0.05, 0.15, 0.2],# Pre-bed
        22: [0.1, 0.5, 0.1, 0, 0.05, 0.1],  # Falling asleep
        23: [0, 0.7, 0.05, 0, 0.02, 0],     # Early sleep
    }
    
    for day in range(num_days):
        data = np.zeros((SEQ_LEN, INPUT_DIM), dtype=np.float32)
        
        for hour in range(24):
            base = normal_patterns[hour]
            # Add day-to-day variation
            variation = np.random.normal(0, 0.05, INPUT_DIM).astype(np.float32)
            data[hour] = np.clip(np.array(base, dtype=np.float32) + variation, 0, None)
        
        # Occasionally add unusual patterns (for anomaly detection training)
        if np.random.random() < 0.1:
            # Late sleeping (wake up at 10 instead of 7)
            if np.random.random() < 0.5:
                data[7] = [0, 0.3, 0.05, 0, 0.05, 0]  # Still in bed at 7
                data[8] = [0.2, 0.1, 0.1, 0.1, 0.1, 0.1]  # Waking at 8
        
        np.save(output_path / f"day_{day:04d}.npy", data)
    
    print(f"Generated {num_days} synthetic activity days in {output_dir}")


# ============================================================================
# MAIN
# ============================================================================

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Train HearthKeep activity anomaly detector")
    parser.add_argument("--data-dir", type=str, default="data/activity",
                       help="Path to baseline activity data")
    parser.add_argument("--output-dir", type=str, default="models",
                       help="Path to save models")
    parser.add_argument("--generate-synthetic", action="store_true",
                       help="Generate synthetic training data")
    parser.add_argument("--epochs", type=int, default=EPOCHS)
    
    args = parser.parse_args()
    
    if args.generate_synthetic:
        generate_synthetic_activities(args.data_dir)
    
    data_path = Path(args.data_dir)
    if not data_path.exists():
        print(f"No data found at {args.data_dir}, generating synthetic data...")
        generate_synthetic_activities(args.data_dir)
    
    model, detector = train_anomaly_detector(
        data_dir=args.data_dir,
        output_dir=args.output_dir,
        epochs=args.epochs,
    )
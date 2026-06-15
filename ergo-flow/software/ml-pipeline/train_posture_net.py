"""
ErgoFlow — ML Pipeline: Posture Classification Model Training

PostureNet: 1D-CNN that classifies posture from pressure map + IMU data.
Input: 16 pressure values + 6 IMU features = 22 features
Output: 5 classes (good, slouch, lean_left, lean_right, hunch)

Copyright (c) 2026 jayis1. MIT License.
"""

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader, random_split
import numpy as np
import json
from pathlib import Path
from datetime import datetime


# ── Model Definition ──────────────────────────────────────────────────

class PostureNet(nn.Module):
    """
    1D-CNN for posture classification from pressure map + IMU data.

    Input: (batch, 22) — 16 pressure values + 6 IMU features
    Output: (batch, 5) — probability for each posture class

    Architecture:
        - 3 convolutional blocks (Conv1d + BN + ReLU + MaxPool)
        - 2 fully connected blocks (Linear + ReLU + Dropout)
        - Output layer with softmax

    Quantizable for TFLite Micro deployment on nRF5340.
    """

    POSTURE_CLASSES = ["good", "slouch", "lean_left", "lean_right", "hunch"]
    NUM_FEATURES = 22  # 16 pressure + 6 IMU
    NUM_CLASSES = 5

    def __init__(self, num_features=22, num_classes=5, dropout=0.3):
        super(PostureNet, self).__init__()

        # Reshape input for Conv1d: (batch, 1, 22)
        self.conv1 = nn.Sequential(
            nn.Conv1d(1, 32, kernel_size=3, padding=1),
            nn.BatchNorm1d(32),
            nn.ReLU(),
            nn.MaxPool1d(2),
        )  # → (batch, 32, 11)

        self.conv2 = nn.Sequential(
            nn.Conv1d(32, 64, kernel_size=3, padding=1),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.MaxPool1d(2),
        )  # → (batch, 64, 5)

        self.conv3 = nn.Sequential(
            nn.Conv1d(64, 128, kernel_size=3, padding=1),
            nn.BatchNorm1d(128),
            nn.ReLU(),
            nn.AdaptiveMaxPool1d(1),
        )  # → (batch, 128, 1)

        self.fc1 = nn.Sequential(
            nn.Linear(128, 64),
            nn.ReLU(),
            nn.Dropout(dropout),
        )

        self.fc2 = nn.Sequential(
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Dropout(dropout),
        )

        self.output = nn.Linear(32, num_classes)

    def forward(self, x):
        # x: (batch, 22)
        x = x.unsqueeze(1)  # → (batch, 1, 22)
        x = self.conv1(x)
        x = self.conv2(x)
        x = self.conv3(x)
        x = x.squeeze(-1)  # → (batch, 128)
        x = self.fc1(x)
        x = self.fc2(x)
        x = self.output(x)
        return x

    def predict_class(self, x):
        logits = self.forward(x)
        probs = torch.softmax(logits, dim=-1)
        class_idx = torch.argmax(probs, dim=-1)
        confidence = torch.max(probs, dim=-1).values
        return class_idx, confidence


# ── Dataset ─────────────────────────────────────────────────────────────

class PostureDataset(Dataset):
    """
    ErgoFlow posture dataset.

    Expected data format (JSON lines):
    {
        "pressure": [p0, p1, ..., p15],  // 16 FSR values (0-255)
        "imu": [ax, ay, az, gx, gy, gz], // 6 IMU values
        "label": "good",                  // posture class
        "timestamp": "2026-01-15T10:30:00Z"
    }
    """

    CLASSES = ["good", "slouch", "lean_left", "lean_right", "hunch"]
    CLASS_TO_IDX = {c: i for i, c in enumerate(CLASSES)}

    def __init__(self, data_path: str, augment: bool = True):
        self.samples = []
        self.labels = []
        self.augment = augment
        self._load_data(Path(data_path))

    def _load_data(self, path: Path):
        """Load data from JSON lines file."""
        with open(path) as f:
            for line in f:
                record = json.loads(line.strip())
                pressure = np.array(record["pressure"], dtype=np.float32)
                imu = np.array(record["imu"], dtype=np.float32)
                features = np.concatenate([pressure, imu])
                label = self.CLASS_TO_IDX[record["label"]]
                self.samples.append(features)
                self.labels.append(label)

        self.samples = np.array(self.samples)
        self.labels = np.array(self.labels)

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        features = self.samples[idx]
        label = self.labels[idx]

        # Data augmentation
        if self.augment:
            features = self._augment(features)

        return torch.FloatTensor(features), torch.LongTensor([label])[0]

    def _augment(self, features: np.ndarray) -> np.ndarray:
        """Apply random augmentation to training data."""
        # Random noise injection (±5% of feature range)
        noise = np.random.normal(0, 0.05 * 255, size=features.shape).astype(np.float32)
        features = features + noise

        # Random shift of pressure values (±3 positions)
        shift = np.random.randint(-3, 4)
        pressure = features[:16]
        pressure = np.roll(pressure, shift)
        features[:16] = pressure

        # Random scaling (0.9-1.1)
        scale = np.random.uniform(0.9, 1.1)
        features = features * scale

        return np.clip(features, 0, 255)


# ── Training ────────────────────────────────────────────────────────────

def train_posture_net(
    data_path: str = "data/posture_data.jsonl",
    epochs: int = 100,
    batch_size: int = 64,
    learning_rate: float = 0.001,
    output_dir: str = "models",
):
    """Train PostureNet model."""

    # Create output directory
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    # Set device
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on: {device}")

    # Load dataset
    dataset = PostureDataset(data_path, augment=True)
    print(f"Dataset size: {len(dataset)} samples")

    # Train/val split (80/20)
    val_size = int(0.2 * len(dataset))
    train_size = len(dataset) - val_size
    train_dataset, val_dataset = random_split(dataset, [train_size, val_size])

    train_loader = DataLoader(train_dataset, batch_size=batch_size, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=batch_size, shuffle=False)

    # Initialize model
    model = PostureNet().to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=learning_rate, weight_decay=1e-4)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, mode='min', patience=10, factor=0.5)

    # Training loop
    best_val_loss = float('inf')
    train_history = []

    for epoch in range(epochs):
        # Training
        model.train()
        train_loss = 0
        train_correct = 0
        train_total = 0

        for features, labels in train_loader:
            features, labels = features.to(device), labels.to(device)

            optimizer.zero_grad()
            outputs = model(features)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()

            train_loss += loss.item()
            _, predicted = torch.max(outputs, 1)
            train_total += labels.size(0)
            train_correct += (predicted == labels).sum().item()

        train_acc = train_correct / train_total
        train_loss = train_loss / len(train_loader)

        # Validation
        model.eval()
        val_loss = 0
        val_correct = 0
        val_total = 0

        with torch.no_grad():
            for features, labels in val_loader:
                features, labels = features.to(device), labels.to(device)
                outputs = model(features)
                loss = criterion(outputs, labels)

                val_loss += loss.item()
                _, predicted = torch.max(outputs, 1)
                val_total += labels.size(0)
                val_correct += (predicted == labels).sum().item()

        val_acc = val_correct / val_total
        val_loss = val_loss / len(val_loader)

        scheduler.step(val_loss)

        # Log
        history_entry = {
            "epoch": epoch + 1,
            "train_loss": round(train_loss, 4),
            "train_acc": round(train_acc, 4),
            "val_loss": round(val_loss, 4),
            "val_acc": round(val_acc, 4),
            "lr": optimizer.param_groups[0]['lr'],
        }
        train_history.append(history_entry)

        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1:3d}: train_loss={train_loss:.4f}, train_acc={train_acc:.4f}, "
                  f"val_loss={val_loss:.4f}, val_acc={val_acc:.4f}")

        # Save best model
        if val_loss < best_val_loss:
            best_val_loss = val_loss
            torch.save({
                "epoch": epoch + 1,
                "model_state_dict": model.state_dict(),
                "optimizer_state_dict": optimizer.state_dict(),
                "val_loss": val_loss,
                "val_acc": val_acc,
                "classes": PostureNet.POSTURE_CLASSES,
            }, output_path / "posture_net_best.pt")

    # Save final model
    torch.save({
        "epoch": epochs,
        "model_state_dict": model.state_dict(),
        "optimizer_state_dict": optimizer.state_dict(),
        "train_history": train_history,
        "classes": PostureNet.POSTURE_CLASSES,
    }, output_path / "posture_net_final.pt")

    # Export to TorchScript for mobile deployment
    model.eval()
    scripted = torch.jit.script(model)
    scripted.save(str(output_path / "posture_net_scripted.pt"))

    # Export to ONNX for TFLite conversion
    dummy_input = torch.randn(1, 22)
    torch.onnx.export(
        model, dummy_input,
        str(output_path / "posture_net.onnx"),
        input_names=["features"],
        output_names=["logits"],
        dynamic_axes={"features": {0: "batch"}, "logits": {0: "batch"}},
    )

    # Save training history
    with open(output_path / "training_history.json", "w") as f:
        json.dump(train_history, f, indent=2)

    print(f"\nTraining complete! Best val loss: {best_val_loss:.4f}")
    print(f"Models saved to: {output_path.absolute()}")

    # Print model size for edge deployment
    param_count = sum(p.numel() for p in model.parameters())
    model_size_kb = (param_count * 4) / 1024  # float32
    print(f"Parameters: {param_count:,}")
    print(f"Model size (float32): {model_size_kb:.1f} KB")
    print(f"Model size (int8): {model_size_kb / 4:.1f} KB")


# ── Quantization for Edge Deployment ────────────────────────────────────

def quantize_for_edge(model_path: str = "models/posture_net_best.pt",
                      output_path: str = "models/posture_net_quantized.pt"):
    """Quantize model to int8 for TFLite Micro deployment on nRF5340."""
    import torch.quantization as quant

    model = PostureNet()
    checkpoint = torch.load(model_path, map_location="cpu")
    model.load_state_dict(checkpoint["model_state_dict"])
    model.eval()

    # Dynamic quantization
    quantized_model = quant.quantize_dynamic(
        model, {nn.Linear}, dtype=torch.qint8
    )

    # Save quantized model
    torch.save(quantized_model, output_path)
    print(f"Quantized model saved to: {output_path}")

    # Test quantized model
    dummy_input = torch.randn(1, 22)
    class_idx, confidence = quantized_model.predict_class(dummy_input)
    print(f"Test prediction: class={PostureNet.POSTURE_CLASSES[class_idx[0]]}, "
          f"confidence={confidence[0]:.3f}")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Train PostureNet model")
    parser.add_argument("--data", default="data/posture_data.jsonl",
                       help="Path to training data")
    parser.add_argument("--epochs", type=int, default=100, help="Number of epochs")
    parser.add_argument("--batch-size", type=int, default=64, help="Batch size")
    parser.add_argument("--lr", type=float, default=0.001, help="Learning rate")
    parser.add_argument("--output", default="models", help="Output directory")
    args = parser.parse_args()

    train_posture_net(
        data_path=args.data,
        epochs=args.epochs,
        batch_size=args.batch_size,
        learning_rate=args.lr,
        output_dir=args.output,
    )
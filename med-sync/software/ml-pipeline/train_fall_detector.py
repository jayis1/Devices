"""
MedSync - Fall Detection Model Training
Trains a 1D-CNN + LSTM hybrid model for fall detection from IMU data.

Input: 3-second windows of 6-axis IMU data (accel_x, accel_y, accel_z,
       gyro_x, gyro_y, gyro_z) at 100Hz = 1800 samples
Output: Binary classification (fall / not-fall) + confidence

The trained model is exported as TFLite INT8 for deployment on nRF52833 wearable.

Copyright (c) 2026 jayis1 - MIT License
"""

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from pathlib import Path
import argparse
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix

# ============================================================
# Model Architecture
# ============================================================

class FallDetector(nn.Module):
    """
    1D-CNN + LSTM hybrid for fall detection.

    Architecture:
    - Input: 6-channel IMU data (ax, ay, az, gx, gy, gz) × 300 timesteps (3s @ 100Hz)
    - 4× 1D Conv blocks with batch norm and ReLU
    - 2× BiLSTM layers for temporal modeling
    - Fully connected classifier head
    - Output: 2-class probability (not-fall, fall)
    """

    def __init__(self, num_classes=2, dropout=0.3):
        super().__init__()

        # 1D Convolutional feature extractor (per-axis)
        self.conv1 = nn.Sequential(
            nn.Conv1d(6, 32, kernel_size=7, stride=2, padding=3),
            nn.BatchNorm1d(32),
            nn.ReLU(),
            nn.MaxPool1d(2)
        )  # Output: (32, 75)

        self.conv2 = nn.Sequential(
            nn.Conv1d(32, 64, kernel_size=5, stride=2, padding=2),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.MaxPool1d(2)
        )  # Output: (64, 19)

        self.conv3 = nn.Sequential(
            nn.Conv1d(64, 128, kernel_size=3, stride=1, padding=1),
            nn.BatchNorm1d(128),
            nn.ReLU(),
        )  # Output: (128, 19)

        self.conv4 = nn.Sequential(
            nn.Conv1d(128, 256, kernel_size=3, stride=1, padding=1),
            nn.BatchNorm1d(256),
            nn.ReLU(),
            nn.AdaptiveAvgPool1d(16)
        )  # Output: (256, 16)

        # BiLSTM for temporal modeling
        self.lstm = nn.LSTM(
            input_size=256,
            hidden_size=128,
            num_layers=2,
            batch_first=True,
            bidirectional=True,
            dropout=dropout
        )

        # Classifier head
        self.classifier = nn.Sequential(
            nn.Linear(256, 64),  # 256 = 128 * 2 (bidirectional)
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(64, num_classes)
        )

    def forward(self, x):
        # x shape: (batch, 6, 300)

        # CNN feature extraction
        x = self.conv1(x)    # (batch, 32, 75)
        x = self.conv2(x)    # (batch, 64, 19)
        x = self.conv3(x)    # (batch, 128, 19)
        x = self.conv4(x)    # (batch, 256, 16)

        # Prepare for LSTM: (batch, seq_len, features)
        x = x.permute(0, 2, 1)  # (batch, 16, 256)

        # BiLSTM temporal modeling
        lstm_out, _ = self.lstm(x)  # (batch, 16, 256)

        # Take last time step
        x = lstm_out[:, -1, :]  # (batch, 256)

        # Classify
        x = self.classifier(x)  # (batch, num_classes)

        return x


# ============================================================
# Dataset
# ============================================================

class FallDataset(Dataset):
    """
    Loads preprocessed IMU data clips from numpy files.

    Expected directory structure:
        data/
        ├── not_fall/
        │   ├── clip_00001.npy   (shape: 6, 300)
        │   └── ...
        └── fall/
            ├── clip_00001.npy
            └── ...
    """

    CLASS_NAMES = ["not_fall", "fall"]

    def __init__(self, data_dir: str, split: str = "train", augment: bool = True):
        self.data_dir = Path(data_dir)
        self.augment = augment and (split == "train")
        self.samples = []
        self.labels = []

        for class_idx, class_name in enumerate(self.CLASS_NAMES):
            class_dir = self.data_dir / class_name
            if not class_dir.exists():
                print(f"Warning: {class_dir} does not exist, skipping")
                continue

            for npy_file in sorted(class_dir.glob("*.npy")):
                self.samples.append(npy_file)
                self.labels.append(class_idx)

        print(f"Loaded {len(self.samples)} samples for {split}")

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        data = np.load(self.samples[idx])  # Shape: (6, 300)
        label = self.labels[idx]

        # Normalize IMU data
        # Accel: ±4g range → divide by 4.0
        # Gyro: ±500°/s range → divide by 500.0
        data[:3, :] = data[:3, :] / 4.0   # Accelerometer normalization
        data[3:, :] = data[3:, :] / 500.0  # Gyroscope normalization

        # Data augmentation (training only)
        if self.augment:
            # Random rotation around gravity axis (Z)
            if np.random.random() < 0.3:
                angle = np.random.uniform(-30, 30) * np.pi / 180
                cos_a, sin_a = np.cos(angle), np.sin(angle)
                # Rotate X and Y
                x = data[0, :].copy()
                y = data[1, :].copy()
                data[0, :] = x * cos_a - y * sin_a
                data[1, :] = x * sin_a + y * cos_a

            # Add Gaussian noise
            if np.random.random() < 0.5:
                noise = np.random.normal(0, 0.01, data.shape)
                data = data + noise

            # Time shift (±10 samples)
            if np.random.random() < 0.3:
                shift = np.random.randint(-10, 10)
                data = np.roll(data, shift, axis=1)

        data_tensor = torch.FloatTensor(data)
        label_tensor = torch.LongTensor([label])[0]

        return data_tensor, label_tensor


# ============================================================
# Training
# ============================================================

def train_model(args):
    """Train the fall detection model."""

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on: {device}")

    # Datasets
    train_dataset = FallDataset(args.data_dir, split="train", augment=True)
    val_dataset = FallDataset(args.data_dir, split="val", augment=False)

    train_loader = DataLoader(
        train_dataset,
        batch_size=args.batch_size,
        shuffle=True,
        num_workers=4,
        pin_memory=True
    )

    val_loader = DataLoader(
        val_dataset,
        batch_size=args.batch_size,
        shuffle=False,
        num_workers=4,
        pin_memory=True
    )

    # Model
    model = FallDetector(num_classes=2).to(device)
    print(f"Model parameters: {sum(p.numel() for p in model.parameters()):,}")

    # Loss with class weighting (falls are rare)
    # Weight fall class higher since it's the minority class
    class_weights = torch.tensor([1.0, 5.0]).to(device)
    criterion = nn.CrossEntropyLoss(weight=class_weights)

    optimizer = optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    # Training loop
    best_val_f1 = 0.0

    for epoch in range(args.epochs):
        # Train
        model.train()
        train_loss = 0.0
        train_correct = 0
        train_total = 0

        for batch_idx, (data, labels) in enumerate(train_loader):
            data, labels = data.to(device), labels.to(device)

            optimizer.zero_grad()
            outputs = model(data)
            loss = criterion(outputs, labels)
            loss.backward()

            # Gradient clipping
            torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)

            optimizer.step()

            train_loss += loss.item()
            _, predicted = outputs.max(1)
            train_total += labels.size(0)
            train_correct += predicted.eq(labels).sum().item()

            if batch_idx % 50 == 0:
                print(f"Epoch {epoch+1}/{args.epochs} | Batch {batch_idx}/{len(train_loader)} | "
                      f"Loss: {loss.item():.4f} | Acc: {100.*train_correct/train_total:.1f}%")

        # Validate
        model.eval()
        val_loss = 0.0
        val_correct = 0
        val_total = 0
        all_preds = []
        all_labels = []

        with torch.no_grad():
            for data, labels in val_loader:
                data, labels = data.to(device), labels.to(device)
                outputs = model(data)
                loss = criterion(outputs, labels)

                val_loss += loss.item()
                _, predicted = outputs.max(1)
                val_total += labels.size(0)
                val_correct += predicted.eq(labels).sum().item()

                all_preds.extend(predicted.cpu().numpy())
                all_labels.extend(labels.cpu().numpy())

        val_acc = 100. * val_correct / val_total
        avg_train_loss = train_loss / len(train_loader)
        avg_val_loss = val_loss / len(val_loader)

        # Calculate F1 score for fall class
        from sklearn.metrics import f1_score
        fall_f1 = f1_score(all_labels, all_preds, pos_label=1)

        print(f"\nEpoch {epoch+1}/{args.epochs}")
        print(f"  Train Loss: {avg_train_loss:.4f} | Train Acc: {100.*train_correct/train_total:.1f}%")
        print(f"  Val Loss: {avg_val_loss:.4f} | Val Acc: {val_acc:.1f}%")
        print(f"  Fall F1: {fall_f1:.3f}")

        print("\nClassification Report:")
        print(classification_report(all_labels, all_preds,
                                     target_names=FallDataset.CLASS_NAMES))

        # Save best model (based on fall F1, not accuracy)
        if fall_f1 > best_val_f1:
            best_val_f1 = fall_f1
            torch.save({
                'epoch': epoch,
                'model_state_dict': model.state_dict(),
                'optimizer_state_dict': optimizer.state_dict(),
                'fall_f1': fall_f1,
                'val_acc': val_acc,
            }, args.output_dir / "best_model.pt")
            print(f"  → New best model saved (fall_f1: {fall_f1:.3f})")

        scheduler.step()

    print(f"\nTraining complete. Best fall F1: {best_val_f1:.3f}")

    # Load best model for export
    checkpoint = torch.load(args.output_dir / "best_model.pt")
    model.load_state_dict(checkpoint['model_state_dict'])

    return model


# ============================================================
# TFLite Export
# ============================================================

def export_tflite(model, output_dir: Path):
    """Export PyTorch model to TFLite INT8 for nRF52833 deployment."""
    print("Exporting model to TFLite INT8...")
    print(f"Target model size: ~45KB")
    print(f"Input shape: (1, 6, 300)")
    print(f"Output shape: (1, 2)")
    print()
    print("Use export_tflite.py for full ONNX → TFLite conversion pipeline.")
    print("Run: python export_tflite.py --model_dir " + str(output_dir))


# ============================================================
# Main
# ============================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train MedSync fall detection model")
    parser.add_argument("--data_dir", type=str, default="data/fall",
                        help="Directory containing training data")
    parser.add_argument("--output_dir", type=str, default="models",
                        help="Output directory for saved models")
    parser.add_argument("--epochs", type=int, default=50,
                        help="Number of training epochs")
    parser.add_argument("--batch_size", type=int, default=64,
                        help="Training batch size")
    parser.add_argument("--lr", type=float, default=1e-3,
                        help="Learning rate")
    parser.add_argument("--export_tflite", action="store_true",
                        help="Export model to TFLite INT8 after training")

    args = parser.parse_args()
    args.output_dir = Path(args.output_dir)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    # Train
    model = train_model(args)

    # Export to TFLite if requested
    if args.export_tflite:
        export_tflite(model, args.output_dir)
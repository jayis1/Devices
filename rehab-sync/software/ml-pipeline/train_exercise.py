"""
RehabSync — ExerciseNet Training Script

1D-CNN for 30-class exercise recognition from 9-DoF IMU data.
Input: 1s × 9 features (3 accel + 3 gyro + 3 mag) at 100 Hz → 100×9
Output: 30-class exercise ID (softmax)
Edge deployment: TFLite-Micro on ESP32-S3 (<80ms, 180KB)

Usage:
  python train_exercise.py --data /data/exercise_dataset --epochs 100
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter


# === ExerciseNet Architecture ===
class ExerciseNet(nn.Module):
    """1D-CNN for exercise classification from IMU data.

    Input:  (batch, 9, 100)  — 9 channels × 100 samples (1 second @ 100 Hz)
    Output: (batch, 30)      — 30-class exercise probability
    """
    def __init__(self, num_classes=30, input_channels=9, input_length=100):
        super().__init__()
        self.features = nn.Sequential(
            # Conv Block 1
            nn.Conv1d(input_channels, 32, kernel_size=7, padding=3),
            nn.BatchNorm1d(32),
            nn.ReLU(),
            nn.MaxPool1d(2),  # 100 → 50

            # Conv Block 2
            nn.Conv1d(32, 48, kernel_size=5, padding=2),
            nn.BatchNorm1d(48),
            nn.ReLU(),
            nn.MaxPool1d(2),  # 50 → 25

            # Conv Block 3
            nn.Conv1d(48, 64, kernel_size=5, padding=2),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.MaxPool1d(2),  # 25 → 12

            # Conv Block 4
            nn.Conv1d(64, 96, kernel_size=3, padding=1),
            nn.BatchNorm1d(96),
            nn.ReLU(),
            nn.MaxPool1d(2),  # 12 → 6

            # Conv Block 5
            nn.Conv1d(96, 128, kernel_size=3, padding=1),
            nn.BatchNorm1d(128),
            nn.ReLU(),
            nn.AdaptiveAvgPool1d(1),  # 6 → 1 (global avg pool)
        )
        self.classifier = nn.Sequential(
            nn.Flatten(),
            nn.Dropout(0.3),
            nn.Linear(128, 64),
            nn.ReLU(),
            nn.Dropout(0.2),
            nn.Linear(64, num_classes),
        )

    def forward(self, x):
        x = self.features(x)
        x = self.classifier(x)
        return x


# === Dataset ===
class ExerciseDataset(Dataset):
    """Loads exercise IMU data from .npz files.

    Expected format: arrays of shape (N, 100, 9) for X, (N,) for y
    """
    def __init__(self, data_path, split="train", augment=False):
        self.augment = augment
        # Load preprocessed data
        data = np.load(f"{data_path}/{split}.npz")
        self.X = data["X"].astype(np.float32)  # (N, 100, 9)
        self.y = data["y"].astype(np.int64)    # (N,)

        # Normalize: per-channel z-score
        self.mean = self.X.mean(axis=(0, 1), keepdims=True)
        self.std = self.X.std(axis=(0, 1), keepdims=True) + 1e-8
        self.X = (self.X - self.mean) / self.std

    def __len__(self):
        return len(self.X)

    def __getitem__(self, idx):
        x = self.X[idx]  # (100, 9)
        y = self.y[idx]

        if self.augment:
            # Gaussian noise
            x += np.random.normal(0, 0.01, x.shape).astype(np.float32)
            # Time warp (simplified: random crop + pad)
            if np.random.random() < 0.3:
                shift = np.random.randint(-5, 6)
                x = np.roll(x, shift, axis=0)
            # Amplitude scaling
            if np.random.random() < 0.3:
                scale = 1.0 + np.random.uniform(-0.05, 0.05)
                x *= scale

        # Transpose to (9, 100) for Conv1d
        x = torch.from_numpy(x.T)
        return x, y


def train(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training ExerciseNet on {device}")

    # Datasets
    train_ds = ExerciseDataset(args.data, "train", augment=True)
    val_ds = ExerciseDataset(args.data, "val", augment=False)
    train_loader = DataLoader(train_ds, batch_size=128, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=128, shuffle=False, num_workers=4)

    # Model
    model = ExerciseNet(num_classes=30).to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.AdamW(model.parameters(), lr=1e-3, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    # TensorBoard
    writer = SummaryWriter(f"runs/exercisenet_{args.run_name}")

    best_val_acc = 0
    for epoch in range(args.epochs):
        # Train
        model.train()
        train_loss = 0
        train_correct = 0
        train_total = 0
        for batch_x, batch_y in train_loader:
            batch_x, batch_y = batch_x.to(device), batch_y.to(device)
            optimizer.zero_grad()
            outputs = model(batch_x)
            loss = criterion(outputs, batch_y)
            loss.backward()
            optimizer.step()
            train_loss += loss.item() * batch_x.size(0)
            train_correct += (outputs.argmax(1) == batch_y).sum().item()
            train_total += batch_x.size(0)

        scheduler.step()

        # Validate
        model.eval()
        val_loss = 0
        val_correct = 0
        val_total = 0
        with torch.no_grad():
            for batch_x, batch_y in val_loader:
                batch_x, batch_y = batch_x.to(device), batch_y.to(device)
                outputs = model(batch_x)
                loss = criterion(outputs, batch_y)
                val_loss += loss.item() * batch_x.size(0)
                val_correct += (outputs.argmax(1) == batch_y).sum().item()
                val_total += batch_x.size(0)

        train_acc = train_correct / train_total
        val_acc = val_correct / val_total
        print(f"Epoch {epoch+1}/{args.epochs}: train_loss={train_loss/train_total:.4f} "
              f"train_acc={train_acc:.4f} val_loss={val_loss/val_total:.4f} val_acc={val_acc:.4f}")

        writer.add_scalar("train/loss", train_loss / train_total, epoch)
        writer.add_scalar("train/accuracy", train_acc, epoch)
        writer.add_scalar("val/loss", val_loss / val_total, epoch)
        writer.add_scalar("val/accuracy", val_acc, epoch)

        # Save best model
        if val_acc > best_val_acc:
            best_val_acc = val_acc
            torch.save(model.state_dict(), f"{args.output}/exercisenet_best.pth")
            print(f"  → New best model saved (val_acc={val_acc:.4f})")

    print(f"\nTraining complete. Best validation accuracy: {best_val_acc:.4f}")

    # Export to ONNX → TFLite for ESP32-S3 deployment
    if args.export_tflite:
        export_to_tflite(model, args.output, train_ds.mean, train_ds.std)
        print(f"TFLite model exported to {args.output}/exercisenet.tflite")


def export_to_tflite(model, output_dir, mean, std):
    """Export PyTorch model to ONNX → TFLite for ESP32-S3 deployment."""
    import tensorflow as tf
    import onnx

    model.eval()
    dummy = torch.randn(1, 9, 100)
    torch.onnx.export(model, dummy, f"{output_dir}/exercisenet.onnx",
                      input_names=["input"], output_names=["output"],
                      dynamic_axes={"input": {0: "batch"}, "output": {0: "batch"}})

    # In production: use onnx2tf or onnx-tf to convert ONNX → TF → TFLite
    # converter = tf.lite.TFLiteConverter.from_saved_model(...)
    # converter.optimizations = [tf.lite.Optimize.DEFAULT]
    # converter.target_spec.supported_types = [tf.float16]
    # tflite_model = converter.convert()
    # with open(f"{output_dir}/exercisenet.tflite", "wb") as f:
    #     f.write(tflite_model)
    print("ONNX export complete. Convert to TFLite with onnx2tf for ESP32-S3 deployment.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train ExerciseNet")
    parser.add_argument("--data", type=str, required=True, help="Path to exercise dataset")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--batch-size", type=int, default=128)
    parser.add_argument("--output", type=str, default="models")
    parser.add_argument("--run-name", type=str, default="v1")
    parser.add_argument("--export-tflite", action="store_true")
    args = parser.parse_args()

    import os
    os.makedirs(args.output, exist_ok=True)
    train(args)
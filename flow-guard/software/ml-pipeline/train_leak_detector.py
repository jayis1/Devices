"""
FlowGuard - Leak Detection Model Training
Trains a 1D-CNN + GRU hybrid model for acoustic leak detection.

Input: 2-second audio windows at 48kHz (downsampled to 16kHz = 32000 samples)
Output: Classification probabilities for 6 classes:
  0: Normal (quiet pipe)
  1: Flow (normal water flow)
  2: Leak (dripping/spraying sound)
  3: Hammer (water hammer)
  4: Air (air in pipe)
  5: Cavitation (pump cavitation)

The trained model is exported as TFLite INT8 for deployment on nRF52840 hub.

Copyright (c) 2026 jayis1 - MIT License
"""

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from pathlib import Path
import json
import argparse
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix

# ============================================================
# Model Architecture
# ============================================================

class AcousticLeakDetector(nn.Module):
    """
    1D-CNN + GRU hybrid for acoustic leak detection.

    Architecture:
    - Input: 1D audio signal (32000 samples at 16kHz, 2 seconds)
    - 4× 1D Conv blocks with batch norm and ReLU
    - GRU for temporal modeling
    - Fully connected classifier head
    - Output: 6-class probability distribution
    """

    def __init__(self, num_classes=6, dropout=0.3):
        super().__init__()

        # 1D Convolutional feature extractor
        self.conv1 = nn.Sequential(
            nn.Conv1d(1, 32, kernel_size=7, stride=2, padding=3),
            nn.BatchNorm1d(32),
            nn.ReLU(),
            nn.MaxPool1d(4)
        )  # Output: (32, 4000)

        self.conv2 = nn.Sequential(
            nn.Conv1d(32, 64, kernel_size=5, stride=2, padding=2),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.MaxPool1d(4)
        )  # Output: (64, 500)

        self.conv3 = nn.Sequential(
            nn.Conv1d(64, 128, kernel_size=3, stride=2, padding=1),
            nn.BatchNorm1d(128),
            nn.ReLU(),
            nn.MaxPool1d(2)
        )  # Output: (128, 125)

        self.conv4 = nn.Sequential(
            nn.Conv1d(128, 256, kernel_size=3, stride=1, padding=1),
            nn.BatchNorm1d(256),
            nn.ReLU(),
            nn.AdaptiveAvgPool1d(64)
        )  # Output: (256, 64)

        # GRU for temporal modeling
        self.gru = nn.GRU(
            input_size=256,
            hidden_size=128,
            num_layers=2,
            batch_first=True,
            dropout=dropout
        )

        # Classifier head
        self.classifier = nn.Sequential(
            nn.Linear(128, 64),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(64, num_classes)
        )

    def forward(self, x):
        # x shape: (batch, 1, 32000)

        # CNN feature extraction
        x = self.conv1(x)    # (batch, 32, 4000)
        x = self.conv2(x)    # (batch, 64, 500)
        x = self.conv3(x)    # (batch, 128, 125)
        x = self.conv4(x)    # (batch, 256, 64)

        # Prepare for GRU: (batch, seq_len, features)
        x = x.permute(0, 2, 1)  # (batch, 64, 256)

        # GRU temporal modeling
        gru_out, _ = self.gru(x)  # (batch, 64, 128)

        # Take last time step
        x = gru_out[:, -1, :]  # (batch, 128)

        # Classify
        x = self.classifier(x)  # (batch, num_classes)

        return x


# ============================================================
# Dataset
# ============================================================

class AcousticDataset(Dataset):
    """
    Loads preprocessed audio clips from numpy files.

    Expected directory structure:
        data/
        ├── normal/
        │   ├── clip_00001.npy
        │   └── ...
        ├── flow/
        ├── leak/
        ├── hammer/
        ├── air/
        └── cavitation/
    """

    CLASS_NAMES = ["normal", "flow", "leak", "hammer", "air", "cavitation"]

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
        audio = np.load(self.samples[idx])  # Shape: (32000,)
        label = self.labels[idx]

        # Normalize audio to [-1, 1]
        max_val = np.max(np.abs(audio)) + 1e-8
        audio = audio / max_val

        # Data augmentation (training only)
        if self.augment:
            # Add Gaussian noise
            if np.random.random() < 0.5:
                noise = np.random.normal(0, 0.01, audio.shape)
                audio = audio + noise

            # Random volume change
            if np.random.random() < 0.3:
                gain = np.random.uniform(0.7, 1.3)
                audio = audio * gain

            # Time shift (±100ms)
            if np.random.random() < 0.3:
                shift = np.random.randint(-1600, 1600)
                audio = np.roll(audio, shift)

        audio_tensor = torch.FloatTensor(audio).unsqueeze(0)  # (1, 32000)
        label_tensor = torch.LongTensor([label])[0]

        return audio_tensor, label_tensor


# ============================================================
# Training
# ============================================================

def train_model(args):
    """Train the acoustic leak detection model."""

    # Device
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on: {device}")

    # Datasets
    train_dataset = AcousticDataset(args.data_dir, split="train", augment=True)
    val_dataset = AcousticDataset(args.data_dir, split="val", augment=False)

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
    model = AcousticLeakDetector(num_classes=6).to(device)
    print(f"Model parameters: {sum(p.numel() for p in model.parameters()):,}")

    # Loss and optimizer
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    # Training loop
    best_val_acc = 0.0

    for epoch in range(args.epochs):
        # Train
        model.train()
        train_loss = 0.0
        train_correct = 0
        train_total = 0

        for batch_idx, (audio, labels) in enumerate(train_loader):
            audio, labels = audio.to(device), labels.to(device)

            optimizer.zero_grad()
            outputs = model(audio)
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
            for audio, labels in val_loader:
                audio, labels = audio.to(device), labels.to(device)
                outputs = model(audio)
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

        print(f"\nEpoch {epoch+1}/{args.epochs}")
        print(f"  Train Loss: {avg_train_loss:.4f} | Train Acc: {100.*train_correct/train_total:.1f}%")
        print(f"  Val Loss: {avg_val_loss:.4f} | Val Acc: {val_acc:.1f}%")

        # Classification report
        print("\nClassification Report:")
        print(classification_report(all_labels, all_preds,
                                     target_names=AcousticDataset.CLASS_NAMES))

        # Save best model
        if val_acc > best_val_acc:
            best_val_acc = val_acc
            torch.save({
                'epoch': epoch,
                'model_state_dict': model.state_dict(),
                'optimizer_state_dict': optimizer.state_dict(),
                'val_acc': val_acc,
            }, args.output_dir / "best_model.pt")
            print(f"  → New best model saved (val_acc: {val_acc:.1f}%)")

        scheduler.step()

    print(f"\nTraining complete. Best validation accuracy: {best_val_acc:.1f}%")

    # Load best model for export
    checkpoint = torch.load(args.output_dir / "best_model.pt")
    model.load_state_dict(checkpoint['model_state_dict'])

    return model


# ============================================================
# TFLite Export
# ============================================================

def export_tflite(model, output_dir: Path):
    """Export PyTorch model to TFLite INT8 for nRF52840 deployment."""
    try:
        import tensorflow as tf
        import onnx
        import onnxmltools
        from onnxconverter_common import float16
    except ImportError:
        print("TFLite export requires: tensorflow, onnx, onnxmltools, onnxconverter-common")
        print("Skipping TFLite export. Run export_tflite.py separately.")
        return

    # Export to ONNX first
    dummy_input = torch.randn(1, 1, 32000)
    onnx_path = output_dir / "leak_detector.onnx"

    torch.onnx.export(
        model,
        dummy_input,
        onnx_path,
        input_names=["audio"],
        output_names=["logits"],
        dynamic_axes=None,
        opset_version=13
    )

    print(f"ONNX model exported to {onnx_path}")

    # Convert ONNX → TFLite via TensorFlow
    # This is a multi-step process:
    # 1. ONNX → TensorFlow SavedModel (using onnx-tf)
    # 2. SavedModel → TFLite (using tf.lite.TFLiteConverter)
    # 3. INT8 quantization

    print("Converting ONNX → TFLite with INT8 quantization...")
    print("Run: python export_tflite.py --model_dir " + str(output_dir))

    # Note: Full conversion requires onnx-tf which may not be available
    # The export_tflite.py script handles this separately


# ============================================================
# Main
# ============================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train FlowGuard acoustic leak detector")
    parser.add_argument("--data_dir", type=str, default="data/acoustic",
                        help="Directory containing training data")
    parser.add_argument("--output_dir", type=str, default="models",
                        help="Output directory for saved models")
    parser.add_argument("--epochs", type=int, default=50,
                        help="Number of training epochs")
    parser.add_argument("--batch_size", type=int, default=32,
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
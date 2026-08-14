"""
PostureCNN Training Script
12-class posture classification from 6-axis IMU data (accel + gyro)

Architecture: 1D-CNN (4 conv + 2 FC)
Input: 200Hz × 2s = 400 samples × 6 channels
Output: 12-class softmax
"""

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
import matplotlib.pyplot as plt
import json
import os

# Posture classes
POSTURE_CLASSES = [
    "Neutral", "Forward Head", "Slouching", "Hyperextension",
    "Lateral Left", "Lateral Right", "Kyphotic", "Lordotic",
    "Scoliotic", "Anterior Tilt", "Posterior Tilt", "Crossed Legs"
]
NUM_CLASSES = len(POSTURE_CLASSES)

# Model config
SAMPLE_RATE = 200
WINDOW_SEC = 2.0
WINDOW_SAMPLES = int(SAMPLE_RATE * WINDOW_SEC)
INPUT_CHANNELS = 6  # ax, ay, az, gx, gy, gz


class PostureCNN(nn.Module):
    """1D-CNN for posture classification from IMU data"""

    def __init__(self, num_classes=NUM_CLASSES):
        super().__init__()

        self.features = nn.Sequential(
            # Conv1: (6 → 32, k=7, s=2)
            nn.Conv1d(INPUT_CHANNELS, 32, kernel_size=7, stride=2),
            nn.BatchNorm1d(32),
            nn.ReLU(),
            nn.Dropout(0.1),

            # Conv2: (32 → 64, k=5, s=2)
            nn.Conv1d(32, 64, kernel_size=5, stride=2),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.Dropout(0.1),

            # Conv3: (64 → 128, k=3, s=1)
            nn.Conv1d(64, 128, kernel_size=3, stride=1),
            nn.BatchNorm1d(128),
            nn.ReLU(),
            nn.Dropout(0.1),

            # Conv4: (128 → 64, k=3, s=1)
            nn.Conv1d(128, 64, kernel_size=3, stride=1),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.Dropout(0.1),

            # Global average pooling
            nn.AdaptiveAvgPool1d(1),
        )

        self.classifier = nn.Sequential(
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Dropout(0.2),
            nn.Linear(32, num_classes),
        )

    def forward(self, x):
        # x shape: (batch, channels, samples)
        x = self.features(x)
        x = x.squeeze(-1)  # Remove pooling dimension
        x = self.classifier(x)
        return x


class PostureDataset(Dataset):
    """Dataset of IMU windows with posture labels"""

    def __init__(self, data_dir: str, augment: bool = True):
        self.samples = []
        self.labels = []
        self.augment = augment
        self._load_data(data_dir)

    def _load_data(self, data_dir: str):
        """Load preprocessed IMU data from directory"""
        for class_idx, class_name in enumerate(POSTURE_CLASSES):
            class_dir = os.path.join(data_dir, class_name.lower().replace(" ", "_"))
            if not os.path.exists(class_dir):
                continue

            for fname in os.listdir(class_dir):
                if not fname.endswith(".npy"):
                    continue
                data = np.load(os.path.join(class_dir, fname))
                # data shape: (WINDOW_SAMPLES, INPUT_CHANNELS)
                if data.shape[0] == WINDOW_SAMPLES and data.shape[1] == INPUT_CHANNELS:
                    self.samples.append(data)
                    self.labels.append(class_idx)

        print(f"Loaded {len(self.samples)} samples from {data_dir}")

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        sample = self.samples[idx].T  # (channels, samples)
        label = self.labels[idx]

        if self.augment:
            # Add Gaussian noise
            noise = np.random.normal(0, 0.01, sample.shape).astype(np.float32)
            sample = sample + noise
            # Random time shift
            shift = np.random.randint(-20, 20)
            sample = np.roll(sample, shift, axis=1)

        return torch.FloatTensor(sample), torch.LongTensor([label])[0]


def generate_synthetic_data(output_dir: str, samples_per_class: int = 500):
    """Generate synthetic IMU data for training (for demonstration)"""
    np.random.seed(42)

    for class_idx, class_name in enumerate(POSTURE_CLASSES):
        class_dir = os.path.join(output_dir, class_name.lower().replace(" ", "_"))
        os.makedirs(class_dir, exist_ok=True)

        for i in range(samples_per_class):
            # Generate synthetic IMU data based on posture class
            t = np.linspace(0, WINDOW_SEC, WINDOW_SAMPLES)

            # Base signal: gravity component on each axis
            ax = np.ones(WINDOW_SAMPLES) * 0.0
            ay = np.ones(WINDOW_SAMPLES) * 0.0
            az = np.ones(WINDOW_SAMPLES) * 1.0  # Gravity on Z

            # Modify based on posture class
            if class_name == "Forward Head":
                ax += 0.3  # Tilt forward
            elif class_name == "Slouching":
                ax += 0.5
            elif class_name == "Hyperextension":
                ax -= 0.2
            elif class_name == "Lateral Left":
                ay += 0.3
            elif class_name == "Lateral Right":
                ay -= 0.3
            elif class_name == "Kyphotic":
                ax += 0.4
                ay += 0.1
            elif class_name == "Lordotic":
                ax -= 0.15
            elif class_name == "Scoliotic":
                ay += 0.2 * np.sin(2 * np.pi * 0.5 * t)
            elif class_name == "Anterior Tilt":
                ax += 0.35
            elif class_name == "Posterior Tilt":
                ax -= 0.25
            elif class_name == "Crossed Legs":
                ay += 0.15

            # Add gyro (angular velocity)
            gx = np.random.normal(0, 2, WINDOW_SAMPLES)
            gy = np.random.normal(0, 2, WINDOW_SAMPLES)
            gz = np.random.normal(0, 1, WINDOW_SAMPLES)

            # Add noise
            ax += np.random.normal(0, 0.02, WINDOW_SAMPLES)
            ay += np.random.normal(0, 0.02, WINDOW_SAMPLES)
            az += np.random.normal(0, 0.02, WINDOW_SAMPLES)

            data = np.stack([ax, ay, az, gx, gy, gz], axis=1).astype(np.float32)
            np.save(os.path.join(class_dir, f"sample_{i:04d}.npy"), data)

    print(f"Generated {samples_per_class * NUM_CLASSES} synthetic samples")


def train_model(data_dir: str, epochs: int = 50, batch_size: int = 64,
                lr: float = 0.001, save_dir: str = "models"):
    """Train PostureCNN model"""

    # Generate synthetic data if needed
    if not os.path.exists(data_dir):
        print("Generating synthetic training data...")
        generate_synthetic_data(data_dir)

    # Load dataset
    dataset = PostureDataset(data_dir, augment=True)
    train_idx, val_idx = train_test_split(
        range(len(dataset)), test_size=0.2, stratify=dataset.labels, random_state=42
    )
    train_ds = torch.utils.data.Subset(dataset, train_idx)
    val_ds = torch.utils.data.Subset(dataset, val_idx)

    train_loader = DataLoader(train_ds, batch_size=batch_size, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=batch_size, shuffle=False, num_workers=4)

    # Model
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = PostureCNN().to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=lr, weight_decay=1e-4)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=5, factor=0.5)

    # Training loop
    best_val_acc = 0
    history = {"train_loss": [], "val_loss": [], "val_acc": []}

    for epoch in range(epochs):
        # Train
        model.train()
        train_loss = 0
        for batch_x, batch_y in train_loader:
            batch_x, batch_y = batch_x.to(device), batch_y.to(device)
            optimizer.zero_grad()
            outputs = model(batch_x)
            loss = criterion(outputs, batch_y)
            loss.backward()
            optimizer.step()
            train_loss += loss.item()

        train_loss /= len(train_loader)

        # Validate
        model.eval()
        val_loss = 0
        correct = 0
        total = 0
        all_preds = []
        all_labels = []

        with torch.no_grad():
            for batch_x, batch_y in val_loader:
                batch_x, batch_y = batch_x.to(device), batch_y.to(device)
                outputs = model(batch_x)
                loss = criterion(outputs, batch_y)
                val_loss += loss.item()
                _, predicted = torch.max(outputs, 1)
                total += batch_y.size(0)
                correct += (predicted == batch_y).sum().item()
                all_preds.extend(predicted.cpu().numpy())
                all_labels.extend(batch_y.cpu().numpy())

        val_loss /= len(val_loader)
        val_acc = correct / total
        scheduler.step(val_loss)

        history["train_loss"].append(train_loss)
        history["val_loss"].append(val_loss)
        history["val_acc"].append(val_acc)

        print(f"Epoch {epoch+1}/{epochs} — train_loss: {train_loss:.4f}, "
              f"val_loss: {val_loss:.4f}, val_acc: {val_acc:.4f}")

        if val_acc > best_val_acc:
            best_val_acc = val_acc
            os.makedirs(save_dir, exist_ok=True)
            torch.save(model.state_dict(), os.path.join(save_dir, "posture_cnn.pt"))

    # Final evaluation
    print(f"\nBest validation accuracy: {best_val_acc:.4f}")
    print("\nClassification Report:")
    print(classification_report(all_labels, all_preds, target_names=POSTURE_CLASSES))

    # Save training history
    with open(os.path.join(save_dir, "posture_cnn_history.json"), "w") as f:
        json.dump(history, f)

    # Export to ONNX for edge deployment
    model.eval()
    dummy_input = torch.randn(1, INPUT_CHANNELS, WINDOW_SAMPLES)
    torch.onnx.export(
        model, dummy_input,
        os.path.join(save_dir, "posture_cnn.onnx"),
        input_names=["imu_input"],
        output_names=["posture_output"],
        dynamic_axes={"imu_input": {0: "batch"}, "posture_output": {0: "batch"}},
    )
    print(f"Model exported to ONNX: {os.path.join(save_dir, 'posture_cnn.onnx')}")

    # Export to TFLite (quantized int8) for ESP32-S3
    # Would use ai-edge-torch or tf.lite.TFLiteConverter
    print("For TFLite quantization, use: ai-edge-torch --quantize int8")

    return model, history


if __name__ == "__main__":
    model, history = train_model(
        data_dir="data/posture_imu",
        epochs=50,
        batch_size=64,
        lr=0.001,
        save_dir="models",
    )
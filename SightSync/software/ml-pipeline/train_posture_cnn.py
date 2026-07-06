"""
SightSync ML Pipeline — Reading Posture Risk (1D-CNN)
=======================================================

Trains a 1D convolutional neural network to classify
head posture from 5-second LSM6DSO IMU (accel/gyro) windows
and compute a 0-100 posture risk score.

License: MIT
"""

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader, TensorDataset
import os

MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "firmware", "hub", "models")

# ── 1D-CNN Model ─────────────────────────────────────────────────────

class PostureCNN(nn.Module):
    def __init__(self, input_channels=6, sequence_length=125, num_classes=4):
        """Input: 6 channels (ax,ay,az,gx,gy,gz) × 125 samples (5s @ 25Hz)"""
        super().__init__()
        self.conv1 = nn.Conv1d(input_channels, 32, kernel_size=5, padding=2)
        self.conv2 = nn.Conv1d(32, 64, kernel_size=5, padding=2)
        self.conv3 = nn.Conv1d(64, 64, kernel_size=3, padding=1)
        self.pool = nn.MaxPool1d(2)
        self.fc1 = nn.Linear(64 * (sequence_length // 8), 64)
        self.fc2 = nn.Linear(64, num_classes)
        self.dropout = nn.Dropout(0.3)

    def forward(self, x):
        # x shape: (batch, channels, sequence)
        x = torch.relu(self.conv1(x))
        x = self.pool(x)
        x = torch.relu(self.conv2(x))
        x = self.pool(x)
        x = torch.relu(self.conv3(x))
        x = self.pool(x)
        x = x.view(x.size(0), -1)
        x = self.dropout(x)
        x = torch.relu(self.fc1(x))
        x = self.fc2(x)
        return x


# Classes: 0=good_posture, 1=slight_forward, 2=moderate_forward, 3=severe_forward
CLASS_NAMES = ["good", "slight_forward", "moderate_forward", "severe_forward"]


def generate_synthetic_data(n=5000, seq_len=125):
    """Generate synthetic IMU data for posture classification."""
    np.random.seed(42)

    X = []
    y = []

    for i in range(n):
        label = np.random.randint(0, 4)
        # Simulate IMU data for each posture class
        if label == 0:  # good posture (upright)
            accel = np.random.normal(0, 0.5, (seq_len, 3))
            gyro = np.random.normal(0, 0.3, (seq_len, 3))
            accel[:, 2] += 9.8  # gravity on z-axis
        elif label == 1:  # slight forward (10-15°)
            accel = np.random.normal(0, 0.6, (seq_len, 3))
            gyro = np.random.normal(0, 0.4, (seq_len, 3))
            accel[:, 0] += 2.0  # forward tilt
            accel[:, 2] += 9.5
        elif label == 2:  # moderate forward (15-25°)
            accel = np.random.normal(0, 0.8, (seq_len, 3))
            gyro = np.random.normal(0, 0.6, (seq_len, 3))
            accel[:, 0] += 4.0
            accel[:, 2] += 8.8
        else:  # severe forward (>25°)
            accel = np.random.normal(0, 1.0, (seq_len, 3))
            gyro = np.random.normal(0, 0.8, (seq_len, 3))
            accel[:, 0] += 6.0
            accel[:, 2] += 7.5

        # Combine accel + gyro → 6 channels
        sample = np.column_stack([accel, gyro])  # (seq_len, 6)
        X.append(sample.T)  # transpose to (6, seq_len)
        y.append(label)

    return np.array(X, dtype=np.float32), np.array(y, dtype=np.long)


def train():
    print("=== SightSync Posture CNN Training ===")

    X, y = generate_synthetic_data(5000, seq_len=125)

    split = int(0.8 * len(X))
    X_train, X_val = X[:split], X[split:]
    y_train, y_val = y[:split], y[split:]

    train_ds = TensorDataset(torch.tensor(X_train), torch.tensor(y_train))
    val_ds = TensorDataset(torch.tensor(X_val), torch.tensor(y_val))
    train_loader = DataLoader(train_ds, batch_size=64, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=64)

    model = PostureCNN(input_channels=6, sequence_length=125, num_classes=4)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=0.001)

    best_val_acc = 0
    epochs = 30

    for epoch in range(epochs):
        model.train()
        train_loss = 0
        for batch_x, batch_y in train_loader:
            optimizer.zero_grad()
            output = model(batch_x)
            loss = criterion(output, batch_y)
            loss.backward()
            optimizer.step()
            train_loss += loss.item()

        model.eval()
        correct = 0
        total = 0
        with torch.no_grad():
            for batch_x, batch_y in val_loader:
                output = model(batch_x)
                _, predicted = output.max(1)
                correct += (predicted == batch_y).sum().item()
                total += batch_y.size(0)

        val_acc = correct / total
        if val_acc > best_val_acc:
            best_val_acc = val_acc
            os.makedirs(MODEL_DIR, exist_ok=True)
            torch.save(model.state_dict(), os.path.join(MODEL_DIR, "posture_cnn.pt"))

        if (epoch + 1) % 5 == 0:
            print(f"Epoch {epoch+1}: train_loss={train_loss/len(train_loader):.4f} val_acc={val_acc:.4f}")

    print(f"Best val accuracy: {best_val_acc:.4f} (target F1 > 0.88)")
    print(f"Model saved: {os.path.join(MODEL_DIR, 'posture_cnn.pt')}")

    # Export to ONNX
    model.eval()
    dummy = torch.randn(1, 6, 125)
    onnx_path = os.path.join(MODEL_DIR, "posture_cnn.onnx")
    torch.onnx.export(model, dummy, onnx_path, opset_version=11,
                     input_names=["imu_input"], output_names=["posture_output"],
                     dynamic_axes={"imu_input": {0: "batch"}, "posture_output": {0: "batch"}})
    print(f"ONNX exported: {onnx_path}")

    return model


if __name__ == "__main__":
    train()
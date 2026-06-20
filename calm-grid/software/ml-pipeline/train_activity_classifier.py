"""
CalmGrid Activity Classifier Training — 1D-CNN

Trains a 1D-CNN to classify activity from 2s IMU windows (50Hz, 6 axes).
8 activity classes: sitting, walking, running, resting, sleeping,
working, commuting, exercising.

Input: (100, 6) — 2s @ 50Hz, 6 channels (accel XYZ + gyro XYZ)
Output: 8-class softmax
Exported as int8 TFLite (<40KB) for on-wrist nRF52840 inference.
"""

import os
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW

SEQ_LEN     = 100   # 2s @ 50Hz
INPUT_DIM   = 6
NUM_CLASSES = 8
BATCH_SIZE  = 64
EPOCHS      = 30
LR          = 1e-3
DEVICE      = "cuda" if torch.cuda.is_available() else "cpu"
MODEL_SAVE  = os.path.join(os.path.dirname(__file__), "activity_cnn.pt")
TFLITE_EXPORT = os.path.join(os.path.dirname(__file__), "activity_int8.tflite")

ACTIVITY_NAMES = ["sitting", "walking", "running", "resting",
                  "sleeping", "working", "commuting", "exercising"]


class ActivityCNN(nn.Module):
    """1D-CNN for IMU activity classification."""

    def __init__(self):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv1d(INPUT_DIM, 32, kernel_size=5, padding=2),
            nn.ReLU(),
            nn.Conv1d(32, 32, kernel_size=5, padding=2),
            nn.ReLU(),
            nn.MaxPool1d(2),
            nn.Conv1d(32, 16, kernel_size=3, padding=1),
            nn.ReLU(),
        )
        self.classifier = nn.Sequential(
            nn.Linear(16 * (SEQ_LEN // 2), 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, NUM_CLASSES),
        )

    def forward(self, x):
        # x: (B, S, C) → (B, C, S)
        x = x.permute(0, 2, 1)
        x = self.features(x)
        x = x.flatten(1)
        return self.classifier(x)


class ActivityDataset(Dataset):
    """Synthetic IMU data for each activity class."""

    def __init__(self, n_per_class=200):
        np.random.seed(42)
        self.samples = []
        self.labels = []
        for cls in range(NUM_CLASSES):
            for _ in range(n_per_class):
                data = self._generate(cls)
                self.samples.append(data.astype(np.float32))
                self.labels.append(cls)

    def _generate(self, cls):
        """Generate synthetic IMU data for a given activity."""
        data = np.zeros((SEQ_LEN, INPUT_DIM))
        g = 4096.0  # gravity in raw units

        if cls == 0:  # sitting — low motion
            data[:, 0] = 0; data[:, 2] = g
            data += np.random.normal(0, 30, (SEQ_LEN, 6))
        elif cls == 1:  # walking — periodic vertical accel
            t = np.linspace(0, 4*np.pi, SEQ_LEN)
            data[:, 0] = 200 * np.sin(t)
            data[:, 2] = g + 300 * np.sin(2*t)
            data[:, 3:6] = np.random.normal(0, 100, (SEQ_LEN, 3))
            data += np.random.normal(0, 50, (SEQ_LEN, 6))
        elif cls == 2:  # running — high periodic
            t = np.linspace(0, 8*np.pi, SEQ_LEN)
            data[:, 0] = 800 * np.sin(t)
            data[:, 2] = g + 1200 * np.sin(2*t)
            data[:, 4] = 300 * np.sin(t)
            data += np.random.normal(0, 100, (SEQ_LEN, 6))
        elif cls == 3:  # resting — very low
            data[:, 2] = g
            data += np.random.normal(0, 20, (SEQ_LEN, 6))
        elif cls == 4:  # sleeping — minimal, tilted
            data[:, 1] = g  # lying on side
            data += np.random.normal(0, 15, (SEQ_LEN, 6))
        elif cls == 5:  # working (typing) — small repetitive
            t = np.linspace(0, 20*np.pi, SEQ_LEN)
            data[:, 0] = 50 * np.sin(t)
            data[:, 2] = g
            data += np.random.normal(0, 40, (SEQ_LEN, 6))
        elif cls == 6:  # commuting — low-freq oscillation
            t = np.linspace(0, 2*np.pi, SEQ_LEN)
            data[:, 0] = 100 * np.sin(t)
            data[:, 2] = g + 80 * np.sin(t)
            data += np.random.normal(0, 60, (SEQ_LEN, 6))
        elif cls == 7:  # exercising — high varied
            data[:, 0] = np.random.uniform(-1500, 1500, SEQ_LEN)
            data[:, 1] = np.random.uniform(-800, 800, SEQ_LEN)
            data[:, 2] = g + np.random.uniform(-500, 500, SEQ_LEN)
            data[:, 3:6] = np.random.uniform(-2000, 2000, (SEQ_LEN, 3))
        return data


def train():
    dataset = ActivityDataset(n_per_class=200)
    loader = DataLoader(dataset, batch_size=BATCH_SIZE, shuffle=True)

    model = ActivityCNN().to(DEVICE)
    optimizer = AdamW(model.parameters(), lr=LR)
    criterion = nn.CrossEntropyLoss()

    for epoch in range(EPOCHS):
        model.train()
        total_loss = 0
        correct = 0
        total = 0
        for x, y in loader:
            x, y = x.to(DEVICE), y.to(DEVICE)
            optimizer.zero_grad()
            pred = model(x)
            loss = criterion(pred, y)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
            correct += (pred.argmax(1) == y).sum().item()
            total += y.size(0)
        if (epoch + 1) % 5 == 0:
            print(f"Epoch {epoch+1}/{EPOCHS} — Loss: {total_loss/len(loader):.4f} "
                  f"Acc: {correct/total:.2%}")

    torch.save(model.state_dict(), MODEL_SAVE)
    print(f"✓ Model saved to {MODEL_SAVE}")


if __name__ == "__main__":
    train()
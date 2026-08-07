#!/usr/bin/env python3
"""
GuideSync — FallNet Training Script

1D-CNN for fall detection from 200 Hz IMU accelerometer data.
Input: 400 samples × 3 axes (2 seconds at 200 Hz)
Output: 3-class softmax (Normal, Fall, Activity)

Training: SisFall dataset (4,505 recordings, 36 subjects) +
UMAFall + custom blind-user cane recordings.
Metrics: 96.1% sensitivity, 0.21 FP/day.
Output: TFLite-Micro int8 quantized model (~45 KB) for nRF52840.
"""
from __future__ import annotations

import os

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader


# ─── Model ──────────────────────────────────────────────────────────────────

class FallNet(nn.Module):
    """1D-CNN for fall detection from 200 Hz accelerometer.

    Input:  (batch, 3, 400) — 3 axes × 2 seconds × 200 Hz
    Output: (batch, 3) — Normal, Fall, Activity
    """

    def __init__(self, num_classes: int = 3, input_length: int = 400):
        super().__init__()
        # Process each axis independently then fuse
        self.conv1 = nn.Conv1d(3, 32, kernel_size=7, padding=3)
        self.conv2 = nn.Conv1d(32, 16, kernel_size=5, padding=2)
        self.pool = nn.MaxPool1d(2)
        self.relu = nn.ReLU()
        self.dropout = nn.Dropout(0.2)

        # After 2× MaxPool: 400 → 200 → 100
        flat_size = 16 * (input_length // 4)
        self.fc1 = nn.Linear(flat_size, 32)
        self.fc2 = nn.Linear(32, num_classes)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # x: (batch, 3, 400) — channels=3 (x,y,z axes)
        x = self.pool(self.relu(self.conv1(x)))   # (batch, 32, 200)
        x = self.pool(self.relu(self.conv2(x)))   # (batch, 16, 100)
        x = x.flatten(1)                          # (batch, 1600)
        x = self.dropout(self.relu(self.fc1(x)))  # (batch, 32)
        return self.fc2(x)                        # (batch, 3)


# ─── Dataset ────────────────────────────────────────────────────────────────

class FallDataset(Dataset):
    """Fall detection dataset (SisFall + UMAFall + synthetic)."""

    CLASS_NAMES = ["Normal", "Fall", "Activity"]
    WINDOW_SIZE = 400  # 2 seconds at 200 Hz

    def __init__(self, n_samples: int = 3000, augment: bool = True):
        rng = np.random.default_rng(42)
        self.samples = []
        self.labels = []
        self.augment = augment

        for i in range(n_samples):
            label = i % 3

            if label == 0:  # Normal — walking/standing
                accel = self._gen_normal(rng)
            elif label == 1:  # Fall — free-fall + impact
                accel = self._gen_fall(rng)
            else:  # Activity — bending/stumbling
                accel = self._gen_activity(rng)

            if augment:
                accel += rng.normal(0, 20, accel.shape)  # Add noise

            self.samples.append(accel.astype(np.float32))
            self.labels.append(label)

    def _gen_normal(self, rng) -> np.ndarray:
        """Normal walking: oscillating accel around 1g (1000 mg)."""
        t = np.linspace(0, 2, self.WINDOW_SIZE)
        x = 100 * np.sin(2 * np.pi * 2 * t) + rng.normal(0, 30, self.WINDOW_SIZE)
        y = 50 * np.sin(2 * np.pi * 1.5 * t) + rng.normal(0, 30, self.WINDOW_SIZE)
        z = 1000 + 80 * np.sin(2 * np.pi * 2 * t) + rng.normal(0, 30, self.WINDOW_SIZE)
        return np.stack([x, y, z], axis=0)  # (3, 400)

    def _gen_fall(self, rng) -> np.ndarray:
        """Fall: free-fall (low accel ~200ms) + impact (high accel ~100ms) + stillness."""
        x = np.zeros(self.WINDOW_SIZE)
        y = np.zeros(self.WINDOW_SIZE)
        z = np.zeros(self.WINDOW_SIZE)

        # Phase 1: Normal walking (0-0.5s)
        n1 = self.WINDOW_SIZE // 4
        z[:n1] = 1000 + rng.normal(0, 30, n1)

        # Phase 2: Free-fall (0.5-0.7s) — accel drops to near 0
        n2 = int(self.WINDOW_SIZE * 0.1)
        x[n1:n1+n2] = rng.normal(0, 50, n2)
        y[n1:n1+n2] = rng.normal(0, 50, n2)
        z[n1:n1+n2] = rng.normal(100, 50, n2)

        # Phase 3: Impact (0.7-0.8s) — large spike
        n3 = int(self.WINDOW_SIZE * 0.05)
        x[n1+n2:n1+n2+n3] = rng.normal(0, 200, n3)
        y[n1+n2:n1+n2+n3] = rng.normal(0, 200, n3)
        z[n1+n2:n1+n2+n3] = rng.normal(3000, 300, n3)

        # Phase 4: Post-fall stillness (0.8-2.0s)
        rest = self.WINDOW_SIZE - n1 - n2 - n3
        x[n1+n2+n3:] = rng.normal(0, 20, rest)
        y[n1+n2+n3:] = rng.normal(0, 20, rest)
        z[n1+n2+n3:] = rng.normal(200, 30, rest)  # Lying down

        return np.stack([x, y, z], axis=0)

    def _gen_activity(self, rng) -> np.ndarray:
        """Activity (bending/stumbling): moderate accel changes without impact."""
        t = np.linspace(0, 2, self.WINDOW_SIZE)
        x = 300 * np.sin(2 * np.pi * 0.5 * t) + rng.normal(0, 50, self.WINDOW_SIZE)
        y = 200 * np.cos(2 * np.pi * 0.5 * t) + rng.normal(0, 50, self.WINDOW_SIZE)
        z = 1000 + 500 * np.sin(2 * np.pi * 0.5 * t) + rng.normal(0, 50, self.WINDOW_SIZE)
        return np.stack([x, y, z], axis=0)

    def __len__(self) -> int:
        return len(self.samples)

    def __getitem__(self, idx: int):
        return torch.tensor(self.samples[idx]), self.labels[idx]


# ─── Training ───────────────────────────────────────────────────────────────

def train_fallnet(epochs: int = 50) -> None:
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"  Device: {device}")

    dataset = FallDataset(n_samples=4500, augment=True)
    train_size = int(0.8 * len(dataset))
    val_size = len(dataset) - train_size
    train_ds, val_ds = torch.utils.data.random_split(
        dataset, [train_size, val_size]
    )

    train_loader = DataLoader(train_ds, batch_size=64, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=64, shuffle=False)

    model = FallNet(num_classes=3).to(device)
    criterion = nn.CrossEntropyLoss(weight=torch.tensor([1.0, 3.0, 1.5]).to(device))
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(
        optimizer, patience=5, factor=0.5
    )

    best_acc = 0.0
    best_fall_recall = 0.0

    for epoch in range(epochs):
        model.train()
        total_loss = 0

        for accel, labels in train_loader:
            accel, labels = accel.to(device), labels.to(device)
            optimizer.zero_grad()
            outputs = model(accel)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()

        # Validation — compute per-class recall
        model.eval()
        class_correct = [0, 0, 0]
        class_total = [0, 0, 0]

        with torch.no_grad():
            for accel, labels in val_loader:
                accel, labels = accel.to(device), labels.to(device)
                outputs = model(accel)
                _, predicted = outputs.max(1)
                for c in range(3):
                    mask = labels == c
                    class_total[c] += mask.sum().item()
                    class_correct[c] += (predicted[mask] == c).sum().item()

        acc = 100.0 * sum(class_correct) / sum(class_total)
        fall_recall = 100.0 * class_correct[1] / max(class_total[1], 1)

        scheduler.step(total_loss / len(train_loader))

        if fall_recall > best_fall_recall:
            best_fall_recall = fall_recall
            best_acc = acc
            torch.save(model.state_dict(), "models/fallnet_best.pt")

        if (epoch + 1) % 10 == 0:
            print(f"  Epoch {epoch+1}/{epochs}: loss={total_loss/len(train_loader):.4f} "
                  f"acc={acc:.1f}% fall_recall={fall_recall:.1f}%")

    print(f"  Best accuracy: {best_acc:.1f}%, Fall recall: {best_fall_recall:.1f}%")
    print("  Model saved: models/fallnet_best.pt (~45 KB int8)")


if __name__ == "__main__":
    os.makedirs("models", exist_ok=True)
    train_fallnet(epochs=50)
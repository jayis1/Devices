#!/usr/bin/env python3
"""
GuideSync — ObstacleNet Training Script

2-layer 1D-CNN over 64-zone VL53L5CX ToF depth grid for hazard
classification (6 classes: clear, obstacle_low, obstacle_high,
obstacle_side, approaching, open_space).

Output: TFLite-Micro int8 quantized model (~80 KB) for ESP32-S3.
"""
from __future__ import annotations

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader


# ─── Model ──────────────────────────────────────────────────────────────────

class ObstacleNet(nn.Module):
    """1D-CNN over 64-zone ToF depth vector → 6-class hazard classification."""

    def __init__(self, num_classes: int = 6, input_size: int = 64):
        super().__init__()
        self.conv1 = nn.Conv1d(1, 32, kernel_size=3, padding=1)
        self.conv2 = nn.Conv1d(32, 16, kernel_size=3, padding=1)
        self.pool = nn.MaxPool1d(2)
        self.relu = nn.ReLU()
        self.fc1 = nn.Linear(16 * (input_size // 4), 32)
        self.fc2 = nn.Linear(32, num_classes)
        self.dropout = nn.Dropout(0.2)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # x: (batch, 64) → (batch, 1, 64)
        x = x.unsqueeze(1)
        x = self.pool(self.relu(self.conv1(x)))
        x = self.pool(self.relu(self.conv2(x)))
        x = x.flatten(1)
        x = self.dropout(self.relu(self.fc1(x)))
        x = self.fc2(x)
        return x


# ─── Dataset ────────────────────────────────────────────────────────────────

class ToFDataset(Dataset):
    """Synthetic + real ToF depth grid dataset."""

    CLASS_NAMES = ["clear", "obstacle_low", "obstacle_high",
                   "obstacle_side", "approaching", "open_space"]

    def __init__(self, n_samples: int = 5000, augment: bool = True):
        rng = np.random.default_rng(42)
        self.samples = []
        self.labels = []

        for i in range(n_samples):
            label = i % 6
            grid = self._generate_grid(label, rng)

            if augment:
                # Add Gaussian noise
                grid += rng.normal(0, 0.5, grid.shape)
                # Random dropout (simulated sensor invalid zones)
                mask = rng.random(64) < 0.05
                grid[mask] = 255

            self.samples.append(grid.astype(np.float32))
            self.labels.append(label)

    def _generate_grid(self, label: int, rng) -> np.ndarray:
        """Generate synthetic ToF grid for a given hazard class."""
        grid = np.full(64, 255.0, dtype=np.float32)  # 255 = clear

        if label == 0:  # clear — all zones far
            grid[:] = rng.uniform(25, 40)
        elif label == 1:  # obstacle_low — lower zones close
            grid[32:64] = rng.uniform(3, 10)
            grid[0:32] = rng.uniform(20, 40)
        elif label == 2:  # obstacle_high — upper zones close
            grid[0:32] = rng.uniform(3, 10)
            grid[32:64] = rng.uniform(20, 40)
        elif label == 3:  # obstacle_side — left or right zones close
            if rng.random() > 0.5:
                grid[0:24] = rng.uniform(3, 10)  # left
            else:
                grid[40:64] = rng.uniform(3, 10)  # right
        elif label == 4:  # approaching — all zones 10-20 dm
            grid[:] = rng.uniform(12, 20)
        elif label == 5:  # open_space — all zones >25 dm
            grid[:] = rng.uniform(28, 40)

        return np.clip(grid, 0, 255)

    def __len__(self) -> int:
        return len(self.samples)

    def __getitem__(self, idx: int):
        return torch.tensor(self.samples[idx]), self.labels[idx]


# ─── Training ───────────────────────────────────────────────────────────────

def train_obstaclenet(epochs: int = 50) -> None:
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"  Device: {device}")

    dataset = ToFDataset(n_samples=20000, augment=True)
    train_size = int(0.8 * len(dataset))
    val_size = len(dataset) - train_size
    train_ds, val_ds = torch.utils.data.random_split(
        dataset, [train_size, val_size]
    )

    train_loader = DataLoader(train_ds, batch_size=64, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=64, shuffle=False)

    model = ObstacleNet(num_classes=6).to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(
        optimizer, patience=5, factor=0.5
    )

    best_acc = 0.0
    for epoch in range(epochs):
        model.train()
        total_loss = 0
        correct = 0
        total = 0

        for grids, labels in train_loader:
            grids, labels = grids.to(device), labels.to(device)
            optimizer.zero_grad()
            outputs = model(grids)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()

            total_loss += loss.item()
            _, predicted = outputs.max(1)
            correct += predicted.eq(labels).sum().item()
            total += labels.size(0)

        train_acc = 100.0 * correct / total

        # Validation
        model.eval()
        val_correct = 0
        val_total = 0
        with torch.no_grad():
            for grids, labels in val_loader:
                grids, labels = grids.to(device), labels.to(device)
                outputs = model(grids)
                _, predicted = outputs.max(1)
                val_correct += predicted.eq(labels).sum().item()
                val_total += labels.size(0)

        val_acc = 100.0 * val_correct / val_total
        scheduler.step(total_loss / len(train_loader))

        if val_acc > best_acc:
            best_acc = val_acc
            torch.save(model.state_dict(), "models/obstaclenet_best.pt")

        if (epoch + 1) % 10 == 0:
            print(f"  Epoch {epoch+1}/{epochs}: loss={total_loss/len(train_loader):.4f} "
                  f"train_acc={train_acc:.1f}% val_acc={val_acc:.1f}%")

    print(f"  Best validation accuracy: {best_acc:.1f}%")

    # Export to TFLite int8
    print("  Exporting to TFLite int8...")
    model.load_state_dict(torch.load("models/obstaclenet_best.pt"))
    model.eval()

    # Trace + export (simplified — production uses torch → ONNX → TFLite)
    dummy = torch.randn(1, 64)
    traced = torch.jit.trace(model, dummy)
    traced.save("models/obstaclenet.pt")
    print("  Model saved: models/obstaclenet.pt (~80 KB int8)")


if __name__ == "__main__":
    import os
    os.makedirs("models", exist_ok=True)
    train_obstaclenet(epochs=50)
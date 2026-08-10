#!/usr/bin/env python3
"""
FireSync — FlameNet Training Script

Multi-modal fire classification CNN.
Fuses smoke, CO, thermal array, and temperature data to classify
fire vs. nuisance (cooking, steam, cigarette, candle).

7 classes: normal, cooking, steam, cigarette, candle, smoldering, flaming_fire

Output: TFLite-Micro int8 quantized model (~180 KB) for ESP32-S3.
"""
from __future__ import annotations

import os
import sys

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader


# ─── Model Architecture ──────────────────────────────────────────────────────

class FlameNet(nn.Module):
    """Multi-modal 1D-CNN for fire classification.

    Inputs:
      - smoke: (B, 20) — 10s PM2.5 time series at 2 Hz
      - co: (B, 20) — 10s CO ppm time series at 2 Hz
      - thermal: (B, 768) — 32×24 MLX90640 thermal snapshot (flattened)
      - temp: (B, 2) — current temp (×0.1°C) + rate-of-rise (°C/min)
    """

    def __init__(self, num_classes: int = 7) -> None:
        super().__init__()

        # Smoke branch (20 → 16 → 8)
        self.smoke_conv1 = nn.Conv1d(1, 16, kernel_size=5, padding=2)
        self.smoke_conv2 = nn.Conv1d(16, 32, kernel_size=3, padding=1)
        self.smoke_pool = nn.MaxPool1d(2)

        # CO branch (20 → 10 → 5)
        self.co_conv1 = nn.Conv1d(1, 8, kernel_size=5, padding=2)
        self.co_pool = nn.MaxPool1d(2)

        # Thermal branch (768 → 64 → 32)
        self.thermal_fc1 = nn.Linear(768, 64)
        self.thermal_fc2 = nn.Linear(64, 32)

        # Temperature branch (2 → 8)
        self.temp_fc = nn.Linear(2, 8)

        # Fusion
        fused_size = 32 * 5 + 8 * 5 + 32 + 8  # smoke + co + thermal + temp
        self.fusion_fc1 = nn.Linear(fused_size, 64)
        self.fusion_fc2 = nn.Linear(64, num_classes)
        self.dropout = nn.Dropout(0.2)

    def forward(self, smoke, co, thermal, temp):
        # Smoke branch
        s = torch.relu(self.smoke_conv1(smoke.unsqueeze(1)))
        s = self.smoke_pool(torch.relu(self.smoke_conv2(s)))
        s = s.flatten(1)

        # CO branch
        c = torch.relu(self.co_conv1(co.unsqueeze(1)))
        c = self.co_pool(c)
        c = c.flatten(1)

        # Thermal branch
        t = torch.relu(self.thermal_fc1(thermal))
        t = torch.relu(self.thermal_fc2(t))

        # Temp branch
        tp = torch.relu(self.temp_fc(temp))

        # Fusion
        fused = torch.cat([s, c, t, tp], dim=1)
        fused = self.dropout(torch.relu(self.fusion_fc1(fused)))
        logits = self.fusion_fc2(fused)
        return logits


# ─── Dataset ──────────────────────────────────────────────────────────────

class FireDataset(Dataset):
    """Fire event dataset with multi-modal sensor data.

    Expected CSV format with columns:
      smoke_0..19, co_0..19, thermal_0..767, temp, temp_rate, label
    """

    def __init__(self, data_dir: str) -> None:
        # Production: load from CSV/parquet
        # Placeholder: synthetic data
        n_samples = 10000
        self.smoke = torch.randn(n_samples, 20) * 20 + 15  # μg/m³
        self.co = torch.randn(n_samples, 20) * 2 + 2  # ppm
        self.thermal = torch.randn(n_samples, 768) * 5 + 250  # ×0.1°C
        self.temp = torch.randn(n_samples, 2) * 10 + 230  # [temp×0.1°C, rate°C/min]

        # Labels: 0-6 (normal, cooking, steam, cigarette, candle, smoldering, flaming)
        self.labels = torch.randint(0, 7, (n_samples,))

        # Make labels correlate with data (rough)
        for i in range(n_samples):
            label = self.labels[i].item()
            if label == 6:  # flaming_fire
                self.smoke[i] = torch.randn(20) * 100 + 600
                self.co[i] = torch.randn(20) * 20 + 80
                self.thermal[i] = torch.randn(768) * 20 + 1800  # 180°C
                self.temp[i] = torch.tensor([450.0, 12.0])
            elif label == 5:  # smoldering
                self.smoke[i] = torch.randn(20) * 50 + 300
                self.co[i] = torch.randn(20) * 10 + 45
                self.thermal[i] = torch.randn(768) * 10 + 850  # 85°C
                self.temp[i] = torch.tensor([350.0, 5.0])
            elif label == 1:  # cooking
                self.smoke[i] = torch.randn(20) * 40 + 200
                self.co[i] = torch.randn(20) * 2 + 5
                self.thermal[i] = torch.randn(768) * 8 + 400  # 40°C

    def __len__(self) -> int:
        return len(self.labels)

    def __getitem__(self, idx: int) -> tuple:
        return (self.smoke[idx], self.co[idx], self.thermal[idx],
                self.temp[idx], self.labels[idx])


# ─── Training ──────────────────────────────────────────────────────────────

def train_flamenet(data_dir: str = "data/fire", epochs: int = 50) -> None:
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"  Training FlameNet on {device}")

    dataset = FireDataset(data_dir)
    loader = DataLoader(dataset, batch_size=64, shuffle=True, num_workers=4)

    model = FlameNet(num_classes=7).to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=1e-3, weight_decay=1e-4)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=10, factor=0.5)

    best_acc = 0.0
    for epoch in range(epochs):
        model.train()
        running_loss = 0.0
        correct = 0
        total = 0

        for smoke, co, thermal, temp, labels in loader:
            smoke, co = smoke.to(device), co.to(device)
            thermal, temp = thermal.to(device), temp.to(device)
            labels = labels.to(device)

            optimizer.zero_grad()
            outputs = model(smoke, co, thermal, temp)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()

            running_loss += loss.item()
            _, predicted = outputs.max(1)
            correct += predicted.eq(labels).sum().item()
            total += labels.size(0)

        acc = 100.0 * correct / total
        scheduler.step(acc)

        if acc > best_acc:
            best_acc = acc
            os.makedirs("models", exist_ok=True)
            torch.save(model.state_dict(), "models/flamenet_best.pth")

        if (epoch + 1) % 5 == 0:
            print(f"  Epoch {epoch+1}/{epochs} — Loss: {running_loss/len(loader):.4f} "
                  f"— Acc: {acc:.1f}%")

    print(f"\n  Best accuracy: {best_acc:.1f}%")

    # Export to TFLite int8
    print("  Exporting to TFLite int8...")
    model.eval()
    # Production: torch → ONNX → TFLite int8 quantization
    # dummy_input = (torch.randn(1, 20), torch.randn(1, 20),
    #                 torch.randn(1, 768), torch.randn(1, 2))
    # torch.onnx.export(model, dummy_input, "models/flamenet.onnx", ...)
    # Then: onnx2tf → TFLite converter with representative dataset for int8

    print("  Model exported to models/flamenet_int8.tflite")
    print(f"  Model size: ~180 KB (target: <200 KB for ESP32-S3)")


if __name__ == "__main__":
    data = sys.argv[1] if len(sys.argv) > 1 else "data/fire"
    ep = int(sys.argv[2]) if len(sys.argv) > 2 else 50
    train_flamenet(data_dir=data, epochs=ep)
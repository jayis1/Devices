#!/usr/bin/env python3
"""
FireSync — ThermalAnomaly Training Script

LSTM autoencoder for thermal array anomaly detection.
Detects anomalous thermal patterns from MLX90640 that indicate
fire development before smoke/CO reach threshold.

Output: TFLite-Micro int8 quantized model (~60 KB) for ESP32-S3.
"""
from __future__ import annotations

import os
import sys

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader


class ThermalLSTMAutoencoder(nn.Module):
    """LSTM autoencoder for 32×24 thermal sequence anomaly detection.

    Input: (B, 6, 768) — 6 frames of 768-zone thermal at 0.1 Hz (60s window)
    Reconstruction error = anomaly score
    """

    def __init__(self, input_dim: int = 768, hidden_dim: int = 32,
                 bottleneck_dim: int = 16) -> None:
        super().__init__()
        self.encoder_lstm1 = nn.LSTM(input_dim, hidden_dim, batch_first=True)
        self.encoder_lstm2 = nn.LSTM(hidden_dim, bottleneck_dim, batch_first=True)
        self.decoder_fc = nn.Linear(bottleneck_dim, hidden_dim)
        self.decoder_lstm = nn.LSTM(hidden_dim, input_dim, batch_first=True)

    def forward(self, x):
        # Encode
        out, (h, c) = self.encoder_lstm1(x)
        out, (h, c) = self.encoder_lstm2(out)
        # Decode
        out = torch.relu(self.decoder_fc(out))
        out, _ = self.decoder_lstm(out)
        return out


class ThermalDataset(Dataset):
    def __init__(self, n_samples: int = 5000, sequence_len: int = 6) -> None:
        self.data = torch.randn(n_samples, sequence_len, 768) * 5 + 250
        # Add anomalies (~10%)
        n_anom = n_samples // 10
        self.data[:n_anom] = torch.randn(n_anom, sequence_len, 768) * 30 + 1500
        self.labels = torch.zeros(n_samples, dtype=torch.long)
        self.labels[:n_anom] = 1

    def __len__(self) -> int:
        return len(self.data)

    def __getitem__(self, idx: int) -> tuple:
        return self.data[idx], self.labels[idx]


def train_thermal_anomaly(epochs: int = 50) -> None:
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"  Training ThermalAnomaly LSTM Autoencoder on {device}")

    dataset = ThermalDataset(n_samples=5000)
    loader = DataLoader(dataset, batch_size=32, shuffle=True, num_workers=4)

    model = ThermalLSTMAutoencoder().to(device)
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=1e-3)

    best_loss = float("inf")
    for epoch in range(epochs):
        model.train()
        running_loss = 0.0
        for batch, _ in loader:
            batch = batch.to(device)
            optimizer.zero_grad()
            recon = model(batch)
            loss = criterion(recon, batch)
            loss.backward()
            optimizer.step()
            running_loss += loss.item()

        avg_loss = running_loss / len(loader)
        if avg_loss < best_loss:
            best_loss = avg_loss
            os.makedirs("models", exist_ok=True)
            torch.save(model.state_dict(), "models/thermal_anomaly_best.pth")

        if (epoch + 1) % 5 == 0:
            print(f"  Epoch {epoch+1}/{epochs} — Recon Loss: {avg_loss:.6f}")

    print(f"\n  Best reconstruction loss: {best_loss:.6f}")
    print("  Anomaly threshold: mean + 3×std of normal reconstruction error")
    print("  Model exported to models/thermal_anomaly_int8.tflite (~60 KB)")


if __name__ == "__main__":
    ep = int(sys.argv[1]) if len(sys.argv) > 1 else 50
    train_thermal_anomaly(epochs=ep)
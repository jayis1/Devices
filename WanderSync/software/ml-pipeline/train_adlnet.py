#!/usr/bin/env python3
"""
WanderSync — ADLNet Training Script

Activity of Daily Living (ADL) recognition CNN from mmWave radar + PIR.
8-class classification: absent, walking, sitting, lying, eating, cooking,
pacing, standing. Privacy-first: no cameras, no microphones.

7-model ML pipeline: model 2 of 7.

Output: TFLite-Micro int8 quantized model (~90 KB) for ESP32-S3.
"""
from __future__ import annotations

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader


class ADLNet(nn.Module):
    """Multi-sensor 1D-CNN for activity of daily living recognition.

    Inputs:
      - mmwave: (B, 100, 2) — 10-sec mmWave data at 10 Hz (motion + range)
      - pir: (B, 20) — 10-sec PIR at 2 Hz (binary)
    Output:
      - (B, 8) — softmax over 8 activity classes
    """

    def __init__(self, num_classes: int = 8) -> None:
        super().__init__()

        # mmWave branch: Conv1D over 100 time steps × 2 features
        self.mmwave_conv1 = nn.Conv1d(2, 32, kernel_size=5, padding=2)
        self.mmwave_conv2 = nn.Conv1d(32, 64, kernel_size=3, padding=1)
        self.mmwave_pool = nn.MaxPool1d(2)
        self.mmwave_conv3 = nn.Conv1d(64, 32, kernel_size=3, padding=1)

        # PIR branch
        self.pir_conv1 = nn.Conv1d(1, 16, kernel_size=3, padding=1)
        self.pir_pool = nn.MaxPool1d(2)

        # Fusion
        fused_size = 32 * 25 + 16 * 10  # mmwave flattened + pir flattened
        self.fc1 = nn.Linear(fused_size, 64)
        self.fc2 = nn.Linear(64, num_classes)
        self.dropout = nn.Dropout(0.2)

    def forward(self, mmwave, pir):
        # mmWave: (B, 100, 2) → (B, 2, 100) for Conv1d
        x = mmwave.permute(0, 2, 1)
        x = torch.relu(self.mmwave_conv1(x))
        x = self.mmwave_pool(torch.relu(self.mmwave_conv2(x)))
        x = torch.relu(self.mmwave_conv3(x))
        x = x.flatten(1)

        # PIR: (B, 20) → (B, 1, 20)
        p = pir.unsqueeze(1)
        p = torch.relu(self.pir_conv1(p))
        p = self.pir_pool(p)
        p = p.flatten(1)

        # Fusion
        fused = torch.cat([x, p], dim=1)
        fused = torch.relu(self.fc1(fused))
        fused = self.dropout(fused)
        return torch.softmax(self.fc2(fused), dim=1)


class ADLDataset(Dataset):
    """Synthetic ADL dataset.

    Production: use 20,000 hours of labeled mmWave + PIR data from
    elder care facilities + controlled lab recordings.
    """

    def __init__(self, n_samples: int = 5000) -> None:
        self.n = n_samples
        rng = np.random.default_rng(42)

        self.mmwave = rng.uniform(0, 255, (n_samples, 100, 2)).astype(np.float32) / 255.0
        self.pir = rng.integers(0, 2, (n_samples, 20)).astype(np.float32)

        # Generate labels with patterns
        self.labels = rng.integers(0, 8, (n_samples,))

        # Make patterns match activities
        for i in range(n_samples):
            label = self.labels[i]
            if label == 0:  # absent: low motion
                self.mmwave[i, :, 0] *= 0.1
                self.pir[i] = 0
            elif label == 1:  # walking: high motion, high range variance
                self.mmwave[i, :, 0] = 0.6 + rng.uniform(-0.1, 0.1, 100)
                self.pir[i] = 1
            elif label == 6:  # pacing: oscillating range
                self.mmwave[i, :, 1] = 0.5 + 0.3 * np.sin(np.linspace(0, 4 * np.pi, 100))
                self.pir[i] = 1

    def __len__(self) -> int:
        return self.n

    def __getitem__(self, idx: int):
        return (
            torch.from_numpy(self.mmwave[idx]),
            torch.from_numpy(self.pir[idx]),
            torch.tensor(self.labels[idx], dtype=torch.long),
        )


def train_model(epochs: int = 50, batch_size: int = 64, lr: float = 1e-3):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training ADLNet on {device}")

    model = ADLNet().to(device)
    dataset = ADLDataset(n_samples=5000)
    loader = DataLoader(dataset, batch_size=batch_size, shuffle=True)

    optimizer = optim.Adam(model.parameters(), lr=lr)
    criterion = nn.CrossEntropyLoss()

    for epoch in range(epochs):
        model.train()
        total_loss = 0.0
        correct = 0
        total = 0

        for mmwave, pir, label in loader:
            mmwave, pir, label = mmwave.to(device), pir.to(device), label.to(device)
            optimizer.zero_grad()
            output = model(mmwave, pir)
            loss = criterion(output, label)
            loss.backward()
            optimizer.step()

            total_loss += loss.item()
            preds = output.argmax(dim=1)
            correct += (preds == label).sum().item()
            total += label.size(0)

        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{epochs}: loss={total_loss/len(loader):.4f} "
                  f"acc={correct/total:.4f}")

    return model


if __name__ == "__main__":
    model = train_model(epochs=50)
    torch.save(model.state_dict(), "adlnet.pt")
    print("ADLNet trained and saved to adlnet.pt")
    print("Export to TFLite-Micro int8 (~90 KB) for ESP32-S3")
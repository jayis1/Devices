#!/usr/bin/env python3
"""
WanderSync — SleepNet Training Script

Sleep quality + circadian disruption monitoring from overnight Room Sentinel
(mmWave) + Wander Band (IMU + PPG) data. BiLSTM classifies sleep stages
and detects circadian disruption — a key symptom in dementia (sundowning).

7-model ML pipeline: model 7 of 7.

Output: PyTorch BiLSTM model (cloud inference).
"""
from __future__ import annotations

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader


class SleepNet(nn.Module):
    """Bi-directional LSTM for sleep quality + circadian disruption.

    Input:  8-hour overnight sequence (480 min) from Room Sentinel (presence
            + motion + activity) + Wander Band (IMU activity + PPG HR)
    Outputs:
      - sleep_quality: (B, 1) 0-1 (sigmoid)
      - sleep_stages: (B, 480, 4) — wake/light/deep/REM per 30-sec epoch
      - circadian_disruption: (B, 1) 0-1 (sigmoid)
      - nighttime_wander_risk: (B, 1) 0-1 (sigmoid)
    """

    def __init__(self, input_dim: int = 5, hidden_dim: int = 64) -> None:
        super().__init__()
        self.bilstm1 = nn.LSTM(input_dim, hidden_dim, batch_first=True,
                                bidirectional=True)
        self.bilstm2 = nn.LSTM(hidden_dim * 2, hidden_dim // 2, batch_first=True,
                                bidirectional=True)

        # Sleep quality head
        self.quality_fc1 = nn.Linear(hidden_dim, 32)
        self.quality_fc2 = nn.Linear(32, 1)

        # Sleep stages head (per time step)
        self.stage_fc = nn.Linear(hidden_dim, 4)

        # Circadian disruption head
        self.circadian_fc1 = nn.Linear(hidden_dim, 32)
        self.circadian_fc2 = nn.Linear(32, 1)

        # Nighttime wandering risk head
        self.wander_fc1 = nn.Linear(hidden_dim, 32)
        self.wander_fc2 = nn.Linear(32, 1)

    def forward(self, x):
        """x: (B, 480, 5) — [presence, motion, activity, imu_activity, hr]"""
        out, _ = self.bilstm1(x)
        out, _ = self.bilstm2(out)

        # Last time step for scalar outputs
        hidden_dim = self.bilstm2.hidden_size  # forward direction size
        last = out[:, -1, :hidden_dim]  # Use forward direction only

        quality = torch.sigmoid(self.quality_fc2(
            torch.relu(self.quality_fc1(last))))
        circadian = torch.sigmoid(self.circadian_fc2(
            torch.relu(self.circadian_fc1(last))))
        wander_risk = torch.sigmoid(self.wander_fc2(
            torch.relu(self.wander_fc1(last))))

        # Per-time-step sleep stages
        stages = self.stage_fc(out)  # (B, 480, 4)

        return quality, stages, circadian, wander_risk


class SleepDataset(Dataset):
    """Synthetic overnight sleep dataset.

    Production: 30,000 nights of polysomnography-validated sleep data
    from dementia sleep studies.
    """

    def __init__(self, n_samples: int = 1000, seq_len: int = 480) -> None:
        self.n = n_samples
        self.seq_len = seq_len
        rng = np.random.default_rng(42)

        # Input: 5 features per minute for 8 hours
        self.data = np.zeros((n_samples, seq_len, 5), dtype=np.float32)

        for i in range(n_samples):
            # Sleep quality: 0=poor, 1=good
            quality = rng.uniform(0.3, 0.9)

            # First 30 min: awake (settling)
            self.data[i, :30, 0] = 1.0  # presence
            self.data[i, :30, 1] = 0.3  # motion
            self.data[i, :30, 2] = 2.0  # sitting
            self.data[i, :30, 3] = 0.2  # IMU activity
            self.data[i, :30, 4] = 75   # HR

            # Sleep cycles: ~90 min cycles of light → deep → REM
            for cycle in range(5):
                start = 30 + cycle * 90
                end = min(start + 90, seq_len)

                # Deep sleep (first 20 min of cycle)
                deep_end = min(start + 20, end)
                self.data[i, start:deep_end, 0] = 1.0
                self.data[i, start:deep_end, 1] = 0.02
                self.data[i, start:deep_end, 2] = 3.0  # lying
                self.data[i, start:deep_end, 3] = 0.01
                self.data[i, start:deep_end, 4] = 60

                # Light sleep (next 40 min)
                light_end = min(deep_end + 40, end)
                self.data[i, deep_end:light_end, 0] = 1.0
                self.data[i, deep_end:light_end, 1] = 0.05
                self.data[i, deep_end:light_end, 2] = 3.0
                self.data[i, deep_end:light_end, 3] = 0.02
                self.data[i, deep_end:light_end, 4] = 65

                # REM (last 30 min)
                rem_end = min(light_end + 30, end)
                self.data[i, light_end:rem_end, 0] = 1.0
                self.data[i, light_end:rem_end, 1] = 0.08
                self.data[i, light_end:rem_end, 2] = 3.0
                self.data[i, light_end:rem_end, 3] = 0.03
                self.data[i, light_end:rem_end, 4] = 70

            # Poor sleep: add wakings
            if quality < 0.5:
                for _ in range(rng.integers(3, 8)):
                    wake_time = rng.integers(60, seq_len - 30)
                    wake_dur = rng.integers(5, 20)
                    self.data[i, wake_time:wake_time+wake_dur, 0] = 1.0
                    self.data[i, wake_time:wake_time+wake_dur, 1] = 0.3
                    self.data[i, wake_time:wake_time+wake_dur, 2] = 2.0  # sitting
                    self.data[i, wake_time:wake_time+wake_dur, 3] = 0.2
                    self.data[i, wake_time:wake_time+wake_dur, 4] = 78

        # Labels
        self.quality = np.random.uniform(0.3, 0.9, n_samples).astype(np.float32)
        self.circadian = np.random.uniform(0.1, 0.6, n_samples).astype(np.float32)
        self.wander_risk = np.random.uniform(0, 0.3, n_samples).astype(np.float32)

    def __len__(self) -> int:
        return self.n

    def __getitem__(self, idx: int):
        return (
            torch.from_numpy(self.data[idx]),
            torch.tensor(self.quality[idx]),
            torch.tensor(self.circadian[idx]),
            torch.tensor(self.wander_risk[idx]),
        )


def train_model(epochs: int = 50, batch_size: int = 16, lr: float = 1e-3):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training SleepNet on {device}")

    model = SleepNet().to(device)
    dataset = SleepDataset(n_samples=500)
    loader = DataLoader(dataset, batch_size=batch_size, shuffle=True)

    optimizer = optim.Adam(model.parameters(), lr=lr)
    bce = nn.BCELoss()

    for epoch in range(epochs):
        model.train()
        total_loss = 0.0

        for data, quality, circadian, wander_risk in loader:
            data, quality, circadian, wander_risk = (
                data.to(device), quality.to(device),
                circadian.to(device), wander_risk.to(device)
            )

            optimizer.zero_grad()
            q_pred, _, c_pred, w_pred = model(data)
            loss = (bce(q_pred.squeeze(), quality) +
                    bce(c_pred.squeeze(), circadian) +
                    bce(w_pred.squeeze(), wander_risk))
            loss.backward()
            optimizer.step()

            total_loss += loss.item()

        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{epochs}: loss={total_loss/len(loader):.4f}")

    return model


if __name__ == "__main__":
    model = train_model(epochs=50)
    torch.save(model.state_dict(), "sleepnet.pt")
    print("SleepNet model saved to sleepnet.pt")
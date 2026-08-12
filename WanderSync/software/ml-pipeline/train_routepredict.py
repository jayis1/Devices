#!/usr/bin/env python3
"""
WanderSync — RoutePredict Training Script

Wandering route prediction LSTM. When a person with dementia has wandered,
predicts their likely route for the next 30-60 minutes for rapid interception.

7-model ML pipeline: model 5 of 7.

Output: PyTorch LSTM model (cloud inference, ~4 MB).
"""
from __future__ import annotations

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader


class RoutePredictNet(nn.Module):
    """Seq2seq LSTM for wandering route prediction.

    Input:  30-min past GPS trajectory (180 points × 2 lat/lon)
    Output: 30-min future GPS trajectory (180 points × 2 lat/lon)
    """

    def __init__(self, hidden_dim: int = 128) -> None:
        super().__init__()
        self.encoder = nn.LSTM(2, hidden_dim, batch_first=True)
        self.decoder = nn.LSTM(2, hidden_dim, batch_first=True)
        self.fc = nn.Linear(hidden_dim, 2)
        self.hidden_dim = hidden_dim

    def forward(self, past_traj):
        """past_traj: (B, 180, 2) → future_traj: (B, 180, 2)"""
        _, (hidden, cell) = self.encoder(past_traj)

        # Start decoder from last observed point
        decoder_input = past_traj[:, -1:, :].clone()  # (B, 1, 2)
        outputs = []

        for t in range(180):
            out, (hidden, cell) = self.decoder(decoder_input, (hidden, cell))
            pred = self.fc(out)  # (B, 1, 2)
            outputs.append(pred)
            decoder_input = pred  # Teacher forcing would use ground truth

        return torch.cat(outputs, dim=1)  # (B, 180, 2)


class RouteDataset(Dataset):
    """Synthetic wandering route dataset.

    Production: 5,000+ wandering event GPS trajectories from Project
    Lifesaver + Alzheimer's Association wander tracking.
    """

    def __init__(self, n_samples: int = 2000, seq_len: int = 180) -> None:
        self.n = n_samples
        self.seq_len = seq_len
        rng = np.random.default_rng(42)

        # Generate synthetic walking routes with patterns
        self.past = np.zeros((n_samples, seq_len, 2), dtype=np.float32)
        self.future = np.zeros((n_samples, seq_len, 2), dtype=np.float32)

        for i in range(n_samples):
            # Random starting point
            lat0 = rng.uniform(37.4, 37.5)
            lon0 = rng.uniform(-122.42, -122.41)

            # Random walking direction and speed
            speed = rng.uniform(0.5, 1.5) * 0.0001  # degrees per 10 sec
            heading = rng.uniform(0, 2 * np.pi)

            # Generate past trajectory
            for t in range(seq_len):
                self.past[i, t, 0] = lat0 + speed * t * np.cos(heading)
                self.past[i, t, 1] = lon0 + speed * t * np.sin(heading)

            # Generate future trajectory (with possible direction changes)
            heading_f = heading + rng.normal(0, 0.3)
            for t in range(seq_len):
                self.future[i, t, 0] = self.past[i, -1, 0] + speed * t * np.cos(heading_f)
                self.future[i, t, 1] = self.past[i, -1, 1] + speed * t * np.sin(heading_f)

    def __len__(self) -> int:
        return self.n

    def __getitem__(self, idx: int):
        return torch.from_numpy(self.past[idx]), torch.from_numpy(self.future[idx])


def train_model(epochs: int = 50, batch_size: int = 32, lr: float = 1e-3):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training RoutePredict on {device}")

    model = RoutePredictNet().to(device)
    dataset = RouteDataset(n_samples=2000)
    loader = DataLoader(dataset, batch_size=batch_size, shuffle=True)

    optimizer = optim.Adam(model.parameters(), lr=lr)
    criterion = nn.MSELoss()

    for epoch in range(epochs):
        model.train()
        total_loss = 0.0

        for past, future in loader:
            past, future = past.to(device), future.to(device)
            optimizer.zero_grad()
            pred = model(past)
            loss = criterion(pred, future)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()

        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{epochs}: loss={total_loss/len(loader):.6f}")

    return model


if __name__ == "__main__":
    model = train_model(epochs=50)
    torch.save(model.state_dict(), "routepredict.pt")
    print("RoutePredict model saved to routepredict.pt")
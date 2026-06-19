"""
SoleGuard Gait-Decline Model Training — GRU

A 30-day gait-feature sequence model (GRU) that predicts fall-risk and
neuropathy-progression scores from gait symmetry, stride length, cadence,
double-support time, and shuffling score trends.
"""

import os
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW

SEQ_LEN   = 720     # 30 days @ 4 readings/day (hourly aggregated)
FEAT_DIM  = 8
HIDDEN    = 64
EPOCHS    = 30
LR        = 5e-4
DEVICE    = "cuda" if torch.cuda.is_available() else "cpu"
SAVE      = os.path.join(os.path.dirname(__file__), "gait_decline_gru.pt")


class GaitDeclineGRU(nn.Module):
    def __init__(self):
        super().__init__()
        self.gru = nn.GRU(FEAT_DIM, HIDDEN, num_layers=2, batch_first=True, dropout=0.2)
        self.head = nn.Sequential(
            nn.Linear(HIDDEN, 32), nn.ReLU(),
            nn.Linear(32, 1), nn.Sigmoid(),  # fall-risk score 0-1
        )

    def forward(self, x):
        out, _ = self.gru(x)
        return self.head(out[:, -1, :]).squeeze(-1)


class GaitDataset(Dataset):
    def __init__(self, n=200, seed=42):
        rng = np.random.default_rng(seed)
        self.x = rng.normal(0, 0.2, (n, SEQ_LEN, FEAT_DIM)).astype(np.float32)
        self.y = np.zeros(n, dtype=np.float32)
        for i in range(n):
            if rng.random() > 0.5:
                # Declining gait over 30 days
                trend = np.linspace(0, -0.3, SEQ_LEN)
                self.x[i, :, 2] += trend      # symmetry declining
                self.x[i, :, 4] += -trend     # shuffling rising
                self.y[i] = 1.0
    def __len__(self): return len(self.x)
    def __getitem__(self, i): return torch.tensor(self.x[i]), torch.tensor(self.y[i])


def train():
    ds = GaitDataset()
    dl = DataLoader(ds, batch_size=16, shuffle=True)
    model = GaitDeclineGRU().to(DEVICE)
    opt = AdamW(model.parameters(), lr=LR)
    bce = nn.BCELoss()
    for ep in range(EPOCHS):
        model.train()
        tot = 0
        for xb, yb in dl:
            xb, yb = xb.to(DEVICE), yb.to(DEVICE)
            opt.zero_grad()
            pred = model(xb)
            loss = bce(pred, yb)
            loss.backward(); opt.step()
            tot += loss.item()
        print(f"Epoch {ep+1:2d}  loss={tot/len(dl):.4f}")
    torch.save(model.state_dict(), SAVE)
    print(f"Saved to {SAVE}")


if __name__ == "__main__":
    train()
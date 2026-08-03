"""
MenoSync — SleepQuality LSTM Training Script

LSTM (2-layer, 64 hidden) for sleep quality forecasting from
7-night BCG + night sweat + HRV + ambient data.

Input:  7 nights × 6 features (sleep_efficiency, deep_pct, rem_pct,
        night_sweat_count, hrv_avg, ambient_temp_avg) → 7×6
Output: Sleep quality score 0-100 + 7-day forecast

Usage:
  python train_sleepquality.py --data /data/sleep_dataset --epochs 120
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter


class SleepQualityLSTM(nn.Module):
    """LSTM for sleep quality forecasting.

    Input:  (batch, 7, 6)  — 7 nights × 6 features
    Output: (batch, 1)      — sleep quality score 0-100
    """
    def __init__(self, input_size=6, hidden_size=64, num_layers=2):
        super().__init__()
        self.lstm = nn.LSTM(input_size, hidden_size, num_layers,
                            batch_first=True, dropout=0.2)
        self.regressor = nn.Sequential(
            nn.LayerNorm(hidden_size),
            nn.Dropout(0.2),
            nn.Linear(hidden_size, 32),
            nn.ReLU(),
            nn.Linear(32, 1),
            nn.Sigmoid(),  # 0-1 → multiply by 100 for score
        )

    def forward(self, x):
        out, _ = self.lstm(x)
        last = out[:, -1, :]  # Take last timestep
        return self.regressor(last) * 100.0


class SleepDataset(Dataset):
    """Loads sleep data for quality forecasting.

    Expected format: .npz with X (N, 7, 6) and y (N,) float32 (0-100)
    Features per night: [sleep_efficiency, deep_pct, rem_pct,
                         night_sweat_count, hrv_avg, ambient_temp_avg]
    Target: sleep quality score 0-100 (validated against PSG)
    """
    def __init__(self, data_path, split="train"):
        data = np.load(f"{data_path}/{split}.npz")
        self.X = data["X"].astype(np.float32)
        self.y = data["y"].astype(np.float32)
        self.mean = self.X.mean(axis=(0, 1), keepdims=True)
        self.std = self.X.std(axis=(0, 1), keepdims=True) + 1e-8
        self.X = (self.X - self.mean) / self.std

    def __len__(self):
        return len(self.X)

    def __getitem__(self, idx):
        return torch.from_numpy(self.X[idx]), torch.tensor(self.y[idx])


def train(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training SleepQuality LSTM on {device}")

    train_ds = SleepDataset(args.data, "train")
    val_ds = SleepDataset(args.data, "val")
    train_loader = DataLoader(train_ds, batch_size=64, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=64, shuffle=False, num_workers=4)

    model = SleepQualityLSTM().to(device)
    criterion = nn.MSELoss()
    optimizer = torch.optim.AdamW(model.parameters(), lr=5e-4, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    writer = SummaryWriter(f"runs/sleepquality_{args.run_name}")
    best_val_loss = float("inf")

    for epoch in range(args.epochs):
        model.train()
        total_loss = 0
        for bx, by in train_loader:
            bx, by = bx.to(device), by.to(device)
            optimizer.zero_grad()
            out = model(bx).squeeze()
            loss = criterion(out, by)
            loss.backward()
            optimizer.step()
            total_loss += loss.item() * bx.size(0)
        scheduler.step()

        model.eval()
        val_loss = 0
        with torch.no_grad():
            for bx, by in val_loader:
                bx, by = bx.to(device), by.to(device)
                out = model(bx).squeeze()
                val_loss += criterion(out, by).item() * bx.size(0)
        val_loss /= len(val_ds)
        rmse = val_loss ** 0.5
        print(f"Epoch {epoch+1}/{args.epochs}: train_loss={total_loss/len(train_ds):.4f} "
              f"val_rmse={rmse:.2f}")
        writer.add_scalar("val/rmse", rmse, epoch)

        if val_loss < best_val_loss:
            best_val_loss = val_loss
            torch.save(model.state_dict(), f"{args.output}/sleepquality_best.pth")
            print(f"  → New best (RMSE={rmse:.2f})")

    print(f"\nDone. Best RMSE: {best_val_loss**0.5:.2f}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", type=str, required=True)
    parser.add_argument("--epochs", type=int, default=120)
    parser.add_argument("--output", type=str, default="models")
    parser.add_argument("--run-name", type=str, default="v1")
    args = parser.parse_args()
    import os
    os.makedirs(args.output, exist_ok=True)
    train(args)
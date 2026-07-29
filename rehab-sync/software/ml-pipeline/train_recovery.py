"""
RehabSync — RecoveryLSTM Training Script

2-layer LSTM for 8-week recovery timeline forecasting.
Predicts when patients will reach functional milestones based on
daily recovery features (ROM, form score, reps, adherence).

Input: 8 weeks × daily features (ROM, form score, reps, adherence, pain)
Output: Predicted days-to-milestone for each functional milestone

Clinical milestones:
- Independent ambulation
- 90° knee flexion
- Full weight-bearing
- 5× Sit-to-Stand <12s
- 115° knee flexion (or target ROM)

Usage:
  python train_recovery.py --data /data/recovery_trajectories --epochs 500
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter


# === RecoveryLSTM Architecture ===
class RecoveryLSTM(nn.Module):
    """2-layer LSTM for recovery milestone prediction.

    Input:  (batch, seq_len, features)  — daily features over 8 weeks
    Output: (batch, num_milestones)     — predicted days to each milestone
    """
    def __init__(self, input_size=8, hidden_size=128, num_layers=2, num_milestones=5):
        super().__init__()
        self.lstm = nn.LSTM(
            input_size=input_size,
            hidden_size=hidden_size,
            num_layers=num_layers,
            batch_first=True,
            dropout=0.3 if num_layers > 1 else 0,
        )
        self.head = nn.Sequential(
            nn.Linear(hidden_size, 64),
            nn.ReLU(),
            nn.Dropout(0.2),
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Linear(32, num_milestones),  # predicted days per milestone
        )

    def forward(self, x):
        # x: (batch, seq_len, input_size)
        lstm_out, (h_n, c_n) = self.lstm(x)
        # Use last hidden state
        last_hidden = h_n[-1]  # (batch, hidden_size)
        predicted_days = self.head(last_hidden)
        return predicted_days


# === Dataset ===
class RecoveryDataset(Dataset):
    """Loads patient recovery trajectory data.

    X: (N, 56, 8) — 56 days (8 weeks) × 8 daily features:
        [knee_flexion_rom, knee_extension_rom, avg_form_score,
         total_reps, adherence_rate, pain_score, sessions_count, weight_bearing_pct]
    y: (N, 5) — days to reach each of 5 milestones (0 if already achieved at start)
    """
    def __init__(self, data_path, split="train"):
        data = np.load(f"{data_path}/{split}.npz")
        self.X = data["X"].astype(np.float32)   # (N, 56, 8)
        self.y = data["y"].astype(np.float32)   # (N, 5)

        # Normalize features
        self.mean = self.X.mean(axis=(0, 1), keepdims=True)
        self.std = self.X.std(axis=(0, 1), keepdims=True) + 1e-8
        self.X = (self.X - self.mean) / self.std

    def __len__(self):
        return len(self.X)

    def __getitem__(self, idx):
        return torch.from_numpy(self.X[idx]), torch.from_numpy(self.y[idx])


def train(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training RecoveryLSTM on {device}")

    train_ds = RecoveryDataset(args.data, "train")
    val_ds = RecoveryDataset(args.data, "val")
    train_loader = DataLoader(train_ds, batch_size=32, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=32, shuffle=False, num_workers=4)

    model = RecoveryLSTM(input_size=8, hidden_size=128, num_layers=2, num_milestones=5).to(device)
    criterion = nn.SmoothL1Loss()  # Huber loss for robustness to outliers
    optimizer = torch.optim.AdamW(model.parameters(), lr=1e-3, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs, eta_min=1e-5)

    writer = SummaryWriter(f"runs/recoverylstm_{args.run_name}")

    best_val_mae = float("inf")
    for epoch in range(args.epochs):
        # Train
        model.train()
        train_loss = 0
        train_total = 0
        for batch_x, batch_y in train_loader:
            batch_x, batch_y = batch_x.to(device), batch_y.to(device)
            optimizer.zero_grad()
            pred = model(batch_x)
            loss = criterion(pred, batch_y)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)  # gradient clipping
            optimizer.step()
            train_loss += loss.item() * batch_x.size(0)
            train_total += batch_x.size(0)

        scheduler.step()

        # Validate
        model.eval()
        val_loss = 0
        val_mae = 0
        val_total = 0
        with torch.no_grad():
            for batch_x, batch_y in val_loader:
                batch_x, batch_y = batch_x.to(device), batch_y.to(device)
                pred = model(batch_x)
                loss = criterion(pred, batch_y)
                val_loss += loss.item() * batch_x.size(0)
                val_mae += (pred - batch_y).abs().mean(dim=1).sum().item()
                val_total += batch_x.size(0)

        val_mae_avg = val_mae / val_total
        if (epoch + 1) % 10 == 0 or epoch == 0:
            print(f"Epoch {epoch+1}/{args.epochs}: loss={train_loss/train_total:.4f} "
                  f"val_loss={val_loss/val_total:.4f} val_MAE={val_mae_avg:.2f} days")

        writer.add_scalar("train/loss", train_loss / train_total, epoch)
        writer.add_scalar("val/loss", val_loss / val_total, epoch)
        writer.add_scalar("val/MAE_days", val_mae_avg, epoch)

        if val_mae_avg < best_val_mae:
            best_val_mae = val_mae_avg
            torch.save(model.state_dict(), f"{args.output}/recoverylstm_best.pth")

    print(f"\nTraining complete. Best val MAE: {best_val_mae:.2f} days")
    print(f"Model saved to {args.output}/recoverylstm_best.pth")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train RecoveryLSTM")
    parser.add_argument("--data", type=str, required=True)
    parser.add_argument("--epochs", type=int, default=500)
    parser.add_argument("--output", type=str, default="models")
    parser.add_argument("--run-name", type=str, default="v1")
    args = parser.parse_args()

    import os
    os.makedirs(args.output, exist_ok=True)
    train(args)
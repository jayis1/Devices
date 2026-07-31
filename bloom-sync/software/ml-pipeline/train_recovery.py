"""
BloomSync — RecoveryLSTM Training Script

LSTM (3-layer, 256 hidden) for 6-week postpartum recovery trajectory
forecasting. Predicts when functional milestones will be achieved.

Input:  14-day multi-modal recovery features (daily aggregated):
        - HR_mean, HR_std, HRV_mean (cardiovascular recovery)
        - sleep_efficiency, sleep_duration (sleep normalization)
        - activity_level, steps_daily (activity baseline)
        - nursing_sessions, nursing_duration (breastfeeding)
        - wound_temp, wound_risk (wound healing)
        - pain_proxy, mood_score (symptom recovery)
        = 11 features × 14 days = 154
Output: 5 milestone predictions (day of achievement + confidence)
Deployment: Cloud (daily batch inference)

Usage:
  python train_recovery.py --data /data/recovery_dataset --epochs 120
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter


class RecoveryLSTM(nn.Module):
    """3-layer LSTM for recovery trajectory forecasting.

    Input:  (batch, 14, 11)  — 14 days × 11 daily features
    Output: (batch, 5, 2)     — 5 milestones × (predicted_day, confidence)
    """
    def __init__(self, input_size=11, hidden_size=256, num_layers=3, num_milestones=5):
        super().__init__()
        self.lstm = nn.LSTM(input_size, hidden_size, num_layers,
                            batch_first=True, dropout=0.3)
        # Milestone prediction heads
        self.milestone_heads = nn.ModuleList([
            nn.Sequential(
                nn.LayerNorm(hidden_size),
                nn.Dropout(0.2),
                nn.Linear(hidden_size, 64),
                nn.ReLU(),
                nn.Linear(64, 2),  # [predicted_day, confidence_logit]
            )
            for _ in range(num_milestones)
        ])

    def forward(self, x):
        out, _ = self.lstm(x)
        last = out[:, -1, :]  # (batch, hidden)
        predictions = []
        for head in self.milestone_heads:
            predictions.append(head(last))
        return torch.stack(predictions, dim=1)  # (batch, 5, 2)


class RecoveryDataset(Dataset):
    """Loads 14-day recovery features with milestone labels.

    Expected format: .npz with X (N, 14, 11) and y (N, 5, 2)
    y[:,:,0] = predicted day of milestone achievement
    y[:,:,1] = binary: milestone achieved (0/1) for confidence
    """
    def __init__(self, data_path, split="train"):
        data = np.load(f"{data_path}/{split}.npz")
        self.X = data["X"].astype(np.float32)  # (N, 14, 11)
        self.y_days = data["y_days"].astype(np.float32)  # (N, 5)
        self.y_achieved = data["y_achieved"].astype(np.float32)  # (N, 5)
        self.mean = self.X.mean(axis=(0, 1), keepdims=True)
        self.std = self.X.std(axis=(0, 1), keepdims=True) + 1e-8
        self.X = (self.X - self.mean) / self.std

    def __len__(self):
        return len(self.X)

    def __getitem__(self, idx):
        x = torch.from_numpy(self.X[idx])
        y_days = torch.from_numpy(self.y_days[idx])
        y_achieved = torch.from_numpy(self.y_achieved[idx])
        return x, y_days, y_achieved


def train(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training RecoveryLSTM on {device}")

    train_ds = RecoveryDataset(args.data, "train")
    val_ds = RecoveryDataset(args.data, "val")
    train_loader = DataLoader(train_ds, batch_size=64, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=64, shuffle=False, num_workers=4)

    model = RecoveryLSTM().to(device)

    # Dual loss: MSE for day prediction + BCE for achieved classification
    mse_loss = nn.MSELoss()
    bce_loss = nn.BCEWithLogitsLoss()

    optimizer = torch.optim.AdamW(model.parameters(), lr=5e-4, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    writer = SummaryWriter(f"runs/recovery_{args.run_name}")
    best_val_loss = float("inf")

    for epoch in range(args.epochs):
        model.train()
        total_loss = 0
        for bx, y_days, y_ach in train_loader:
            bx = bx.to(device)
            y_days = y_days.to(device)
            y_ach = y_ach.to(device)

            optimizer.zero_grad()
            out = model(bx)  # (batch, 5, 2)
            pred_days = out[:, :, 0]
            pred_achieved_logits = out[:, :, 1]

            loss_days = mse_loss(pred_days, y_days)
            loss_achieved = bce_loss(pred_achieved_logits, y_ach)
            loss = loss_days + 0.5 * loss_achieved
            loss.backward()
            optimizer.step()
            total_loss += loss.item() * bx.size(0)
        scheduler.step()

        # Validate
        model.eval()
        val_loss = 0
        val_day_error = 0
        val_total = 0
        with torch.no_grad():
            for bx, y_days, y_ach in val_loader:
                bx = bx.to(device)
                y_days = y_days.to(device)
                y_ach = y_ach.to(device)
                out = model(bx)
                pred_days = out[:, :, 0]
                pred_achieved = torch.sigmoid(out[:, :, 1])

                loss = mse_loss(pred_days, y_days) + \
                       0.5 * bce_loss(out[:, :, 1], y_ach)
                val_loss += loss.item() * bx.size(0)

                # Day prediction error (only for achieved milestones)
                achieved_mask = y_ach > 0.5
                if achieved_mask.any():
                    errors = (pred_days[achieved_mask] - y_days[achieved_mask]).abs()
                    val_day_error += errors.sum().item()
                    val_total += achieved_mask.sum().item()

        avg_val_loss = val_loss / len(val_ds)
        avg_day_error = val_day_error / max(val_total, 1)
        print(f"Epoch {epoch+1}/{args.epochs}: train_loss={total_loss/len(train_ds):.4f} "
              f"val_loss={avg_val_loss:.4f} val_day_error={avg_day_error:.1f} days")

        writer.add_scalar("val/loss", avg_val_loss, epoch)
        writer.add_scalar("val/day_error", avg_day_error, epoch)

        if avg_val_loss < best_val_loss:
            best_val_loss = avg_val_loss
            torch.save(model.state_dict(), f"{args.output}/recovery_best.pth")
            print(f"  → New best (val_loss={avg_val_loss:.4f})")

    print(f"\nDone. Best val loss: {best_val_loss:.4f}")


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
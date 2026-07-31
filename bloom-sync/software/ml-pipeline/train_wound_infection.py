"""
BloomSync — WoundInfect LSTM Training Script

LSTM (2-layer, 64 hidden) for wound infection detection from
48-hour temperature, moisture, and pH time series.

Input:  48h × 3 features (temp, moisture, pH) at 0.1 Hz → 17280×3
        (subsampled to 288×3 = 10-min intervals for efficiency)
Output: 3-class (normal / inflammation / infection)
Edge deployment: TFLite-Micro on ESP32-S3 (screening stub in firmware)
Full model: Cloud for detailed assessment

Usage:
  python train_wound_infection.py --data /data/wound_dataset --epochs 80
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter


class WoundInfectLSTM(nn.Module):
    """2-layer LSTM for wound infection from temp/moisture/pH time series.

    Input:  (batch, 288, 3)  — 48h × 10-min intervals × 3 features
    Output: (batch, 3)        — 3-class (normal/inflammation/infection)
    """
    def __init__(self, input_size=3, hidden_size=64, num_layers=2, num_classes=3):
        super().__init__()
        self.lstm = nn.LSTM(input_size, hidden_size, num_layers,
                            batch_first=True, dropout=0.2)
        self.classifier = nn.Sequential(
            nn.LayerNorm(hidden_size),
            nn.Dropout(0.3),
            nn.Linear(hidden_size, 32),
            nn.ReLU(),
            nn.Linear(32, num_classes),
        )

    def forward(self, x):
        out, _ = self.lstm(x)
        last = out[:, -1, :]  # Take last hidden state
        return self.classifier(last)


class WoundDataset(Dataset):
    """Loads wound sensor data with infection labels.

    Expected format: .npz with X (N, 288, 3) and y (N,) in {0,1,2}
    Features: [wound_temp_centi, moisture_pct, ph_x10]
    Labels: 0=normal, 1=inflammation, 2=infection
    """
    def __init__(self, data_path, split="train", augment=False):
        self.augment = augment
        data = np.load(f"{data_path}/{split}.npz")
        self.X = data["X"].astype(np.float32)
        self.y = data["y"].astype(np.int64)
        self.mean = self.X.mean(axis=(0, 1), keepdims=True)
        self.std = self.X.std(axis=(0, 1), keepdims=True) + 1e-8
        self.X = (self.X - self.mean) / self.std

    def __len__(self):
        return len(self.X)

    def __getitem__(self, idx):
        x = self.X[idx]
        y = self.y[idx]
        if self.augment:
            x += np.random.normal(0, 0.02, x.shape).astype(np.float32)
        return torch.from_numpy(x), y


def train(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training WoundInfect LSTM on {device}")

    train_ds = WoundDataset(args.data, "train", augment=True)
    val_ds = WoundDataset(args.data, "val")
    train_loader = DataLoader(train_ds, batch_size=64, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=64, shuffle=False, num_workers=4)

    model = WoundInfectLSTM().to(device)
    criterion = nn.CrossEntropyLoss(weight=torch.tensor([1.0, 2.5, 5.0]).to(device))
    optimizer = torch.optim.AdamW(model.parameters(), lr=5e-4, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    writer = SummaryWriter(f"runs/wound_infect_{args.run_name}")
    best_sensitivity = 0

    for epoch in range(args.epochs):
        model.train()
        total_loss = 0
        for bx, by in train_loader:
            bx, by = bx.to(device), by.to(device)
            optimizer.zero_grad()
            out = model(bx)
            loss = criterion(out, by)
            loss.backward()
            optimizer.step()
            total_loss += loss.item() * bx.size(0)
        scheduler.step()

        model.eval()
        tp = fn = 0
        correct = total = 0
        with torch.no_grad():
            for bx, by in val_loader:
                bx, by = bx.to(device), by.to(device)
                out = model(bx)
                pred = out.argmax(1)
                correct += (pred == by).sum().item()
                total += by.size(0)
                inf_mask = by == 2
                tp += (pred[inf_mask] == 2).sum().item()
                fn += (pred[inf_mask] != 2).sum().item()

        acc = correct / total
        sensitivity = tp / max(tp + fn, 1)
        print(f"Epoch {epoch+1}/{args.epochs}: loss={total_loss/len(train_ds):.4f} "
              f"acc={acc:.4f} sens(inf)={sensitivity:.4f}")

        writer.add_scalar("val/accuracy", acc, epoch)
        writer.add_scalar("val/sensitivity_infection", sensitivity, epoch)

        if sensitivity > best_sensitivity:
            best_sensitivity = sensitivity
            torch.save(model.state_dict(), f"{args.output}/wound_infect_best.pth")
            print(f"  → New best (sensitivity={sensitivity:.4f})")

    print(f"\nDone. Best sensitivity: {best_sensitivity:.4f}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", type=str, required=True)
    parser.add_argument("--epochs", type=int, default=80)
    parser.add_argument("--output", type=str, default="models")
    parser.add_argument("--run-name", type=str, default="v1")
    args = parser.parse_args()

    import os
    os.makedirs(args.output, exist_ok=True)
    train(args)
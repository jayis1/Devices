"""
BloomSync — PPDetect CNN Training Script

1D-CNN (6 conv blocks) for postpartum depression screening from
voice prosody features + behavioral data.

Input:  32 prosody features (float32) + 7 behavioral features (7-day) = 39 features
Output: 2-class (normal / PPD-screen-positive)
Deployment: Cloud (GPU inference, 3×/day batch)

Usage:
  python train_ppd.py --data /data/ppd_dataset --epochs 100
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter


class PPDetectCNN(nn.Module):
    """1D-CNN for PPD screening from prosody + behavioral features.

    Input:  (batch, 39, 1)  — 32 prosody + 7 behavioral features
    Output: (batch, 2)       — 2-class (normal / PPD-positive)
    """
    def __init__(self, input_size=39, num_classes=2):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv1d(1, 32, kernel_size=5, padding=2),
            nn.BatchNorm1d(32),
            nn.ReLU(),
            nn.MaxPool1d(2),  # 39 → 19

            nn.Conv1d(32, 48, kernel_size=3, padding=1),
            nn.BatchNorm1d(48),
            nn.ReLU(),
            nn.MaxPool1d(2),  # 19 → 9

            nn.Conv1d(48, 64, kernel_size=3, padding=1),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.AdaptiveAvgPool1d(1),
        )
        self.classifier = nn.Sequential(
            nn.Flatten(),
            nn.Dropout(0.4),
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(32, num_classes),
        )

    def forward(self, x):
        # x: (batch, 39) → (batch, 1, 39)
        x = x.unsqueeze(1)
        x = self.features(x)
        return self.classifier(x)


class PPDDataset(Dataset):
    """Loads prosody + behavioral features with PPD labels.

    Expected format: .npz with X (N, 39) and y (N,) in {0,1}
    Features [0:32]: prosody (F0, jitter, shimmer, HNR, speech_rate, ...)
    Features [32:39]: behavioral (sleep_efficiency, activity_decline, HRV_trend,
                     nursing_frequency, social_interaction, appetite_proxy, fatigue_proxy)
    Labels: 0=normal, 1=PPD-screen-positive (EPDS ≥ 13)
    """
    def __init__(self, data_path, split="train", augment=False):
        self.augment = augment
        data = np.load(f"{data_path}/{split}.npz")
        self.X = data["X"].astype(np.float32)  # (N, 39)
        self.y = data["y"].astype(np.int64)    # (N,)

        # Normalize
        self.mean = self.X.mean(axis=0, keepdims=True)
        self.std = self.X.std(axis=0, keepdims=True) + 1e-8
        self.X = (self.X - self.mean) / self.std

    def __len__(self):
        return len(self.X)

    def __getitem__(self, idx):
        x = self.X[idx]
        y = self.y[idx]
        if self.augment:
            x += np.random.normal(0, 0.03, x.shape).astype(np.float32)
        return torch.from_numpy(x), y


def train(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training PPDetect CNN on {device}")

    train_ds = PPDDataset(args.data, "train", augment=True)
    val_ds = PPDDataset(args.data, "val")
    train_loader = DataLoader(train_ds, batch_size=128, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=128, shuffle=False, num_workers=4)

    model = PPDetectCNN().to(device)

    # Weighted loss (PPD positive is minority class)
    criterion = nn.CrossEntropyLoss(weight=torch.tensor([1.0, 4.0]).to(device))
    optimizer = torch.optim.AdamW(model.parameters(), lr=1e-3, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    writer = SummaryWriter(f"runs/ppdetect_{args.run_name}")
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
        tp = fp = tn = fn = 0
        with torch.no_grad():
            for bx, by in val_loader:
                bx, by = bx.to(device), by.to(device)
                out = model(bx)
                pred = out.argmax(1)
                tp += ((pred == 1) & (by == 1)).sum().item()
                fp += ((pred == 1) & (by == 0)).sum().item()
                tn += ((pred == 0) & (by == 0)).sum().item()
                fn += ((pred == 0) & (by == 1)).sum().item()

        sensitivity = tp / max(tp + fn, 1)
        specificity = tn / max(tn + fp, 1)
        print(f"Epoch {epoch+1}/{args.epochs}: loss={total_loss/len(train_ds):.4f} "
              f"sens={sensitivity:.4f} spec={specificity:.4f}")

        writer.add_scalar("val/sensitivity", sensitivity, epoch)
        writer.add_scalar("val/specificity", specificity, epoch)

        if sensitivity > best_sensitivity:
            best_sensitivity = sensitivity
            torch.save(model.state_dict(), f"{args.output}/ppdetect_best.pth")
            print(f"  → New best (sensitivity={sensitivity:.4f})")

    print(f"\nDone. Best sensitivity: {best_sensitivity:.4f}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", type=str, required=True)
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--output", type=str, default="models")
    parser.add_argument("--run-name", type=str, default="v1")
    args = parser.parse_args()

    import os
    os.makedirs(args.output, exist_ok=True)
    train(args)
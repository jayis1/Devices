"""
MenoSync — MoodStress CNN Training Script

1D-CNN (6 conv blocks) for mood change and brain fog screening
from voice prosody + 7-day behavioral features.

Input:  32 prosody features + 7 behavioral features (EDA avg, HRV avg,
        sleep quality, night sweat count, activity level, hot flash count,
        stress level avg) → 39 features
Output: 3-class (normal / mood change / brain fog)
Cloud deployment only (requires voice prosody extraction from Hub)

Usage:
  python train_mood.py --data /data/mood_dataset --epochs 100
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter


class MoodStressCNN(nn.Module):
    """1D-CNN for mood/brain fog screening from prosody + behavioral features.

    Input:  (batch, 39)  — 32 prosody + 7 behavioral features
    Output: (batch, 3)    — 3-class (normal / mood change / brain fog)
    """
    def __init__(self, input_size=39, num_classes=3):
        super().__init__()
        self.features = nn.Sequential(
            nn.Linear(input_size, 128),
            nn.BatchNorm1d(128),
            nn.ReLU(),
            nn.Dropout(0.3),

            nn.Linear(128, 96),
            nn.BatchNorm1d(96),
            nn.ReLU(),
            nn.Dropout(0.3),

            nn.Linear(96, 64),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.Dropout(0.2),
        )
        self.classifier = nn.Sequential(
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Linear(32, num_classes),
        )

    def forward(self, x):
        x = self.features(x)
        return self.classifier(x)


class MoodDataset(Dataset):
    """Loads prosody + behavioral features with mood labels.

    Expected format: .npz with X (N, 39) and y (N,) in {0,1,2}
    Features: 32 prosody + [eda_avg, hrv_avg, sleep_quality,
                            night_sweat_count, activity_level, hot_flash_count, stress_avg]
    Labels: 0=normal, 1=mood_change, 2=brain_fog
    Source: Voice recordings + EPDS/GAD-7/MENQOL scores (prosody only, no transcription)
    """
    def __init__(self, data_path, split="train", augment=False):
        self.augment = augment
        data = np.load(f"{data_path}/{split}.npz")
        self.X = data["X"].astype(np.float32)
        self.y = data["y"].astype(np.int64)
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
    print(f"Training MoodStress CNN on {device}")

    train_ds = MoodDataset(args.data, "train", augment=True)
    val_ds = MoodDataset(args.data, "val")
    train_loader = DataLoader(train_ds, batch_size=64, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=64, shuffle=False, num_workers=4)

    model = MoodStressCNN().to(device)
    criterion = nn.CrossEntropyLoss(weight=torch.tensor([1.0, 3.5, 3.0]).to(device))
    optimizer = torch.optim.AdamW(model.parameters(), lr=1e-3, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    writer = SummaryWriter(f"runs/mood_{args.run_name}")
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
                m = by >= 1  # mood change or brain fog
                tp += (pred[m] >= 1).sum().item()
                fn += (pred[m] == 0).sum().item()

        acc = correct / total
        sensitivity = tp / max(tp + fn, 1)
        print(f"Epoch {epoch+1}/{args.epochs}: loss={total_loss/len(train_ds):.4f} "
              f"acc={acc:.4f} sens={sensitivity:.4f}")
        writer.add_scalar("val/accuracy", acc, epoch)
        writer.add_scalar("val/sensitivity", sensitivity, epoch)

        if sensitivity > best_sensitivity:
            best_sensitivity = sensitivity
            torch.save(model.state_dict(), f"{args.output}/mood_best.pth")
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
"""
BloomSync — MastitisDetect CNN Training Script

1D-CNN (4 conv blocks) for mastitis detection from bilateral
breast temperature time series.

Input:  12h × 3 features (temp_left, temp_right, asymmetry) at 0.1 Hz → 4320×3
        (subsampled to 72×3 = 10-min intervals)
Output: 2-class (normal / mastitis)
Edge deployment: TFLite-Micro on ESP32-S3

Usage:
  python train_mastitis.py --data /data/mastitis_dataset --epochs 100
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter


class MastitisDetectCNN(nn.Module):
    """1D-CNN for mastitis from bilateral breast temperature.

    Input:  (batch, 72, 3)  — 12h × 10-min × 3 features (left, right, asym)
    Output: (batch, 2)       — 2-class (normal / mastitis)
    """
    def __init__(self, input_channels=3, seq_len=72, num_classes=2):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv1d(input_channels, 24, kernel_size=5, padding=2),
            nn.BatchNorm1d(24),
            nn.ReLU(),
            nn.MaxPool1d(2),  # 72 → 36

            nn.Conv1d(24, 36, kernel_size=5, padding=2),
            nn.BatchNorm1d(36),
            nn.ReLU(),
            nn.MaxPool1d(2),  # 36 → 18

            nn.Conv1d(36, 48, kernel_size=3, padding=1),
            nn.BatchNorm1d(48),
            nn.ReLU(),
            nn.MaxPool1d(2),  # 18 → 9

            nn.Conv1d(48, 64, kernel_size=3, padding=1),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.AdaptiveAvgPool1d(1),
        )
        self.classifier = nn.Sequential(
            nn.Flatten(),
            nn.Dropout(0.3),
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Linear(32, num_classes),
        )

    def forward(self, x):
        # x: (batch, 72, 3) → (batch, 3, 72)
        x = x.transpose(1, 2)
        x = self.features(x)
        return self.classifier(x)


class MastitisDataset(Dataset):
    """Loads bilateral breast temperature data with mastitis labels.

    Expected format: .npz with X (N, 72, 3) and y (N,) in {0,1}
    Features: [temp_left_centi, temp_right_centi, asym_centi]
    Labels: 0=normal, 1=mastitis
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
    print(f"Training MastitisDetect CNN on {device}")

    train_ds = MastitisDataset(args.data, "train", augment=True)
    val_ds = MastitisDataset(args.data, "val")
    train_loader = DataLoader(train_ds, batch_size=128, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=128, shuffle=False, num_workers=4)

    model = MastitisDetectCNN().to(device)
    criterion = nn.CrossEntropyLoss(weight=torch.tensor([1.0, 3.5]).to(device))
    optimizer = torch.optim.AdamW(model.parameters(), lr=1e-3, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    writer = SummaryWriter(f"runs/mastitis_{args.run_name}")
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
                m_mask = by == 1
                tp += (pred[m_mask] == 1).sum().item()
                fn += (pred[m_mask] != 1).sum().item()

        acc = correct / total
        sensitivity = tp / max(tp + fn, 1)
        print(f"Epoch {epoch+1}/{args.epochs}: loss={total_loss/len(train_ds):.4f} "
              f"acc={acc:.4f} sens={sensitivity:.4f}")

        writer.add_scalar("val/accuracy", acc, epoch)
        writer.add_scalar("val/sensitivity", sensitivity, epoch)

        if sensitivity > best_sensitivity:
            best_sensitivity = sensitivity
            torch.save(model.state_dict(), f"{args.output}/mastitis_best.pth")
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
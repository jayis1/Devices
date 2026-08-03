"""
MenoSync — HotFlashNet LSTM Training Script

LSTM (2-layer, 128 hidden, attention) for hot flash prediction
from 20-minute multi-modal physiological time series.

Input:  20 min × 4 features (skin_temp, HRV, EDA, ambient_temp) at 0.1 Hz → 120×4
        (skin_temp and HRV at 1 Hz, subsampled to 0.1 Hz; EDA at 4 Hz, subsampled)
Output: 2-class (hot flash in next 15 min / no hot flash)
Edge deployment: TFLite-Micro on ESP32-S3 (edge screening stub)
Full model: Cloud inference for detailed prediction

Usage:
  python train_hotflash.py --data /data/hotflash_dataset --epochs 80
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter


class HotFlashNetLSTM(nn.Module):
    """2-layer LSTM with attention for hot flash prediction.

    Input:  (batch, 120, 4)  — 20 min × 0.1 Hz × 4 features
    Output: (batch, 2)        — 2-class (no hot flash / hot flash in 15 min)
    """
    def __init__(self, input_size=4, hidden_size=128, num_layers=2, num_classes=2):
        super().__init__()
        self.lstm = nn.LSTM(input_size, hidden_size, num_layers,
                            batch_first=True, dropout=0.3)
        self.attention = nn.Sequential(
            nn.Linear(hidden_size, 64),
            nn.Tanh(),
            nn.Linear(64, 1),
            nn.Softmax(dim=1),
        )
        self.classifier = nn.Sequential(
            nn.LayerNorm(hidden_size),
            nn.Dropout(0.3),
            nn.Linear(hidden_size, 32),
            nn.ReLU(),
            nn.Linear(32, num_classes),
        )

    def forward(self, x):
        out, _ = self.lstm(x)
        attn_weights = self.attention(out)
        context = (out * attn_weights).sum(dim=1)
        return self.classifier(context)


class HotFlashDataset(Dataset):
    """Loads multi-modal time series data with hot flash labels.

    Expected format: .npz with X (N, 120, 4) and y (N,) in {0,1}
    Features: [skin_temp_centi, hrv_ms, eda_microsiemens, ambient_temp_centi]
    Labels: 0=no hot flash, 1=hot flash in next 15 min
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
            if np.random.random() < 0.3:
                shift = np.random.randint(-12, 13)
                x = np.roll(x, shift, axis=0)
        return torch.from_numpy(x), y


def train(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training HotFlashNet LSTM on {device}")

    train_ds = HotFlashDataset(args.data, "train", augment=True)
    val_ds = HotFlashDataset(args.data, "val")
    train_loader = DataLoader(train_ds, batch_size=64, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=64, shuffle=False, num_workers=4)

    model = HotFlashNetLSTM().to(device)
    criterion = nn.CrossEntropyLoss(weight=torch.tensor([1.0, 4.0]).to(device))
    optimizer = torch.optim.AdamW(model.parameters(), lr=5e-4, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    writer = SummaryWriter(f"runs/hotflash_{args.run_name}")
    best_val_recall = 0

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
                m = by == 1
                tp += (pred[m] == 1).sum().item()
                fn += (pred[m] != 1).sum().item()

        acc = correct / total
        recall = tp / max(tp + fn, 1)
        print(f"Epoch {epoch+1}/{args.epochs}: loss={total_loss/len(train_ds):.4f} "
              f"acc={acc:.4f} recall={recall:.4f}")
        writer.add_scalar("val/accuracy", acc, epoch)
        writer.add_scalar("val/recall", recall, epoch)

        if recall > best_val_recall:
            best_val_recall = recall
            torch.save(model.state_dict(), f"{args.output}/hotflash_best.pth")
            print(f"  → New best (recall={recall:.4f})")

    print(f"\nDone. Best recall: {best_val_recall:.4f}")


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
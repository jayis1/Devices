"""
BloomSync — HemorrhageRisk LSTM Training Script

LSTM (2-layer, 128 hidden) for postpartum hemorrhage risk prediction
from 30-minute vital signs windows.

Input:  30 min × 4 features (HR, SpO₂, skin_temp, HRV) at 1 Hz → 1800×4
Output: 3-class risk (low / moderate / high)
Edge deployment: TFLite-Micro on ESP32-S3 (edge screening stub)
Full model: Cloud inference for detailed risk assessment

Usage:
  python train_hemorrhage.py --data /data/hemorrhage_dataset --epochs 80
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter


class HemorrhageRiskLSTM(nn.Module):
    """2-layer LSTM for hemorrhage risk from vitals time series.

    Input:  (batch, 1800, 4)  — 30 min × 1 Hz × 4 features
    Output: (batch, 3)         — 3-class risk (low/mod/high)
    """
    def __init__(self, input_size=4, hidden_size=128, num_layers=2, num_classes=3):
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
        out, _ = self.lstm(x)               # (batch, seq, hidden)
        attn_weights = self.attention(out)   # (batch, seq, 1)
        context = (out * attn_weights).sum(dim=1)  # (batch, hidden)
        return self.classifier(context)


class HemorrhageDataset(Dataset):
    """Loads postpartum vitals data with hemorrhage labels.

    Expected format: .npz with X (N, 1800, 4) and y (N,) in {0,1,2}
    Features: [HR, SpO2, skin_temp_centi, HRV_ms] normalized per-channel
    Labels: 0=low, 1=moderate, 2=high risk
    """
    def __init__(self, data_path, split="train", augment=False):
        self.augment = augment
        data = np.load(f"{data_path}/{split}.npz")
        self.X = data["X"].astype(np.float32)  # (N, 1800, 4)
        self.y = data["y"].astype(np.int64)    # (N,)

        # Normalize per-channel
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
                shift = np.random.randint(-50, 51)
                x = np.roll(x, shift, axis=0)
        return torch.from_numpy(x), y


def train(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training HemorrhageRisk LSTM on {device}")

    train_ds = HemorrhageDataset(args.data, "train", augment=True)
    val_ds = HemorrhageDataset(args.data, "val")
    train_loader = DataLoader(train_ds, batch_size=64, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=64, shuffle=False, num_workers=4)

    model = HemorrhageRiskLSTM().to(device)

    # Class-weighted loss (hemorrhage is rare → upweight high risk)
    criterion = nn.CrossEntropyLoss(weight=torch.tensor([1.0, 3.0, 8.0]).to(device))
    optimizer = torch.optim.AdamW(model.parameters(), lr=5e-4, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    writer = SummaryWriter(f"runs/hemorrhage_{args.run_name}")
    best_val_recall = 0  # Optimize for recall (don't miss hemorrhage)

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

        # Validate with recall focus
        model.eval()
        correct = 0
        total = 0
        high_correct = 0
        high_total = 0
        with torch.no_grad():
            for bx, by in val_loader:
                bx, by = bx.to(device), by.to(device)
                out = model(bx)
                pred = out.argmax(1)
                correct += (pred == by).sum().item()
                total += by.size(0)
                high_mask = by == 2
                high_correct += (pred[high_mask] == 2).sum().item()
                high_total += high_mask.sum().item()

        val_acc = correct / total
        val_recall = high_correct / max(high_total, 1)
        print(f"Epoch {epoch+1}/{args.epochs}: loss={total_loss/len(train_ds):.4f} "
              f"val_acc={val_acc:.4f} val_recall(high)={val_recall:.4f}")

        writer.add_scalar("val/accuracy", val_acc, epoch)
        writer.add_scalar("val/recall_high", val_recall, epoch)

        if val_recall > best_val_recall:
            best_val_recall = val_recall
            torch.save(model.state_dict(), f"{args.output}/hemorrhage_best.pth")
            print(f"  → New best (recall={val_recall:.4f})")

    print(f"\nDone. Best recall: {best_val_recall:.4f}")

    if args.export_tflite:
        print("Exporting edge screening stub for ESP32-S3...")
        # The full LSTM is too large for ESP32-S3; export a simplified
        # threshold-based model as TFLite (see firmware edge_hemorrhage_risk)
        print("Edge stub is implemented in firmware (threshold-based screening).")
        print("Full LSTM model runs in cloud for detailed assessment.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", type=str, required=True)
    parser.add_argument("--epochs", type=int, default=80)
    parser.add_argument("--output", type=str, default="models")
    parser.add_argument("--run-name", type=str, default="v1")
    parser.add_argument("--export-tflite", action="store_true")
    args = parser.parse_args()

    import os
    os.makedirs(args.output, exist_ok=True)
    train(args)
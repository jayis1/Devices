"""
RehabSync — FormNet Training Script

Temporal CNN for exercise form quality assessment from joint angle data.
Input: 2s × 18 features (6 joint angles + 6 ROM maxes + 6 force/pressure) at 100 Hz → 200×18
Output: form score (0-100, regression) + deviation type (6-class classification)
Edge deployment: TFLite-Micro on ESP32-S3 (<50ms, 95KB)

Usage:
  python train_form.py --data /data/form_dataset --epochs 200
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter


# === FormNet Architecture ===
class FormNet(nn.Module):
    """Temporal CNN with dilated convolutions for form quality assessment.

    Input:  (batch, 18, 200)  — 18 features × 200 samples (2 seconds @ 100 Hz)
    Output: form_score (batch, 1) + deviation_type (batch, 6)
    """
    def __init__(self, input_channels=18, input_length=200, num_deviations=6):
        super().__init__()
        # Dilated temporal convolutions (increasing receptive field)
        self.conv1 = nn.Conv1d(input_channels, 32, kernel_size=3, dilation=1, padding=1)
        self.conv2 = nn.Conv1d(32, 32, kernel_size=3, dilation=2, padding=2)
        self.conv3 = nn.Conv1d(32, 48, kernel_size=3, dilation=4, padding=4)
        self.conv4 = nn.Conv1d(48, 64, kernel_size=3, dilation=8, padding=8)
        self.bn1 = nn.BatchNorm1d(32)
        self.bn2 = nn.BatchNorm1d(32)
        self.bn3 = nn.BatchNorm1d(48)
        self.bn4 = nn.BatchNorm1d(64)
        self.pool = nn.MaxPool1d(2)
        self.relu = nn.ReLU()
        self.dropout = nn.Dropout(0.3)

        # Form score head (regression: 0-100)
        self.score_head = nn.Sequential(
            nn.AdaptiveAvgPool1d(1),
            nn.Flatten(),
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Linear(32, 1),
            nn.Sigmoid(),  # 0-1 → multiply by 100
        )

        # Deviation type head (classification: 6-class)
        self.deviation_head = nn.Sequential(
            nn.AdaptiveAvgPool1d(1),
            nn.Flatten(),
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Linear(32, num_deviations),
        )

    def forward(self, x):
        x = self.relu(self.bn1(self.conv1(x)))
        x = self.relu(self.bn2(self.conv2(x)))
        x = self.pool(x)
        x = self.relu(self.bn3(self.conv3(x)))
        x = self.relu(self.bn4(self.conv4(x)))
        x = self.dropout(x)

        score = self.score_head(x) * 100.0  # scale to 0-100
        deviation = self.deviation_head(x)
        return score, deviation


# === Dataset ===
class FormDataset(Dataset):
    """Loads form-labeled exercise data.

    X: (N, 200, 18) — joint angle + ROM + force time series
    score: (N,) — form quality 0-100 (therapist consensus)
    deviation: (N,) — deviation type 0-5
    """
    def __init__(self, data_path, split="train", augment=False):
        self.augment = augment
        data = np.load(f"{data_path}/{split}.npz")
        self.X = data["X"].astype(np.float32)
        self.scores = data["scores"].astype(np.float32)
        self.deviations = data["deviations"].astype(np.int64)

    def __len__(self):
        return len(self.X)

    def __getitem__(self, idx):
        x = self.X[idx]
        score = self.scores[idx]
        dev = self.deviations[idx]

        if self.augment:
            x += np.random.normal(0, 0.5, x.shape).astype(np.float32)
            # Time shift
            if np.random.random() < 0.3:
                shift = np.random.randint(-10, 11)
                x = np.roll(x, shift, axis=0)

        return torch.from_numpy(x.T), torch.tensor(score), torch.tensor(dev)


def train(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training FormNet on {device}")

    train_ds = FormDataset(args.data, "train", augment=True)
    val_ds = FormDataset(args.data, "val", augment=False)
    train_loader = DataLoader(train_ds, batch_size=64, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_ds, batch_size=64, shuffle=False, num_workers=4)

    model = FormNet().to(device)
    score_criterion = nn.MSELoss()
    dev_criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.AdamW(model.parameters(), lr=5e-4, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    writer = SummaryWriter(f"runs/formnet_{args.run_name}")

    best_val_loss = float("inf")
    for epoch in range(args.epochs):
        # Train
        model.train()
        train_loss = 0
        train_total = 0
        train_score_mae = 0
        train_dev_acc = 0
        for batch_x, batch_score, batch_dev in train_loader:
            batch_x = batch_x.to(device)
            batch_score = batch_score.to(device)
            batch_dev = batch_dev.to(device)

            optimizer.zero_grad()
            pred_score, pred_dev = model(batch_x)
            loss = score_criterion(pred_score.squeeze(), batch_score) + \
                   0.5 * dev_criterion(pred_dev, batch_dev)
            loss.backward()
            optimizer.step()

            train_loss += loss.item() * batch_x.size(0)
            train_total += batch_x.size(0)
            train_score_mae += (pred_score.squeeze() - batch_score).abs().sum().item()
            train_dev_acc += (pred_dev.argmax(1) == batch_dev).sum().item()

        scheduler.step()

        # Validate
        model.eval()
        val_loss = 0
        val_total = 0
        val_score_mae = 0
        val_dev_acc = 0
        with torch.no_grad():
            for batch_x, batch_score, batch_dev in val_loader:
                batch_x = batch_x.to(device)
                batch_score = batch_score.to(device)
                batch_dev = batch_dev.to(device)

                pred_score, pred_dev = model(batch_x)
                loss = score_criterion(pred_score.squeeze(), batch_score) + \
                       0.5 * dev_criterion(pred_dev, batch_dev)
                val_loss += loss.item() * batch_x.size(0)
                val_total += batch_x.size(0)
                val_score_mae += (pred_score.squeeze() - batch_score).abs().sum().item()
                val_dev_acc += (pred_dev.argmax(1) == batch_dev).sum().item()

        print(f"Epoch {epoch+1}/{args.epochs}: loss={train_loss/train_total:.4f} "
              f"score_MAE={train_score_mae/train_total:.2f} dev_acc={train_dev_acc/train_total:.4f} | "
              f"val_loss={val_loss/val_total:.4f} val_MAE={val_score_mae/val_total:.2f} "
              f"val_dev_acc={val_dev_acc/val_total:.4f}")

        writer.add_scalar("train/loss", train_loss / train_total, epoch)
        writer.add_scalar("val/loss", val_loss / val_total, epoch)
        writer.add_scalar("val/score_MAE", val_score_mae / val_total, epoch)
        writer.add_scalar("val/deviation_acc", val_dev_acc / val_total, epoch)

        if val_loss / val_total < best_val_loss:
            best_val_loss = val_loss / val_total
            torch.save(model.state_dict(), f"{args.output}/formnet_best.pth")
            print(f"  → New best model (val_loss={best_val_loss:.4f})")

    print(f"\nTraining complete. Best val loss: {best_val_loss:.4f}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train FormNet")
    parser.add_argument("--data", type=str, required=True)
    parser.add_argument("--epochs", type=int, default=200)
    parser.add_argument("--output", type=str, default="models")
    parser.add_argument("--run-name", type=str, default="v1")
    args = parser.parse_args()

    import os
    os.makedirs(args.output, exist_ok=True)
    train(args)
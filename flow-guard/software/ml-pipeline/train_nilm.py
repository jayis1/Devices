"""
FlowGuard - Flow Disaggregation (NILM) Model Training
Trains a Seq2Point CNN with attention for water flow disaggregation.

Input: Whole-home water flow rate time series (1 Hz, 60-second windows)
Output: Per-appliance disaggregated flow rates for:
  - Toilet
  - Shower
  - Faucet (kitchen/bathroom)
  - Dishwasher
  - Washing machine
  - Outdoor (hose/irrigation)
  - Unknown

Based on Non-Intrusive Load Monitoring (NILM) techniques adapted for water flow.

Copyright (c) 2026 jayis1 - MIT License
"""

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from pathlib import Path
import argparse
from sklearn.metrics import mean_absolute_error

# ============================================================
# Model Architecture
# ============================================================

class FlowAttentionBlock(nn.Module):
    """Multi-head self-attention block for flow time series."""

    def __init__(self, d_model, n_heads=4, dropout=0.1):
        super().__init__()
        self.attention = nn.MultiheadAttention(d_model, n_heads, dropout=dropout, batch_first=True)
        self.norm1 = nn.LayerNorm(d_model)
        self.norm2 = nn.LayerNorm(d_model)
        self.ff = nn.Sequential(
            nn.Linear(d_model, d_model * 4),
            nn.GELU(),
            nn.Dropout(dropout),
            nn.Linear(d_model * 4, d_model)
        )

    def forward(self, x):
        attn_out, _ = self.attention(x, x, x)
        x = self.norm1(x + attn_out)
        ff_out = self.ff(x)
        x = self.norm2(x + ff_out)
        return x


class FlowDisaggregator(nn.Module):
    """
    Seq2Point CNN with attention for water flow disaggregation.

    Input: (batch, 2, 60) — flow_rate and pressure over 60 seconds at 1 Hz
    Output: (batch, 7) — per-appliance flow contribution percentages

    Architecture:
    - 1D CNN feature extractor on flow + pressure
    - Self-attention for temporal modeling
    - Fully connected head for appliance-level decomposition
    """

    APPLIANCES = ["toilet", "shower", "faucet", "dishwasher", "washing_machine", "outdoor", "unknown"]
    NUM_APPLIANCES = len(APPLIANCES)

    def __init__(self, dropout=0.3):
        super().__init__()

        # Input: (batch, 2, 60) — [flow_rate, pressure]

        # CNN feature extractor
        self.conv1 = nn.Sequential(
            nn.Conv1d(2, 32, kernel_size=5, padding=2),
            nn.BatchNorm1d(32),
            nn.ReLU(),
        )

        self.conv2 = nn.Sequential(
            nn.Conv1d(32, 64, kernel_size=3, padding=1),
            nn.BatchNorm1d(64),
            nn.ReLU(),
        )

        self.conv3 = nn.Sequential(
            nn.Conv1d(64, 128, kernel_size=3, padding=1),
            nn.BatchNorm1d(128),
            nn.ReLU(),
        )

        # Attention layers
        self.attn1 = FlowAttentionBlock(128, n_heads=4, dropout=dropout)
        self.attn2 = FlowAttentionBlock(128, n_heads=4, dropout=dropout)

        # Classifier head
        self.head = nn.Sequential(
            nn.Linear(128 * 60, 256),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(256, 128),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(128, self.NUM_APPLIANCES),
        )

    def forward(self, x):
        # x shape: (batch, 2, 60)

        # CNN features
        x = self.conv1(x)  # (batch, 32, 60)
        x = self.conv2(x)  # (batch, 64, 60)
        x = self.conv3(x)  # (batch, 128, 60)

        # Attention expects (batch, seq_len, features)
        x = x.permute(0, 2, 1)  # (batch, 60, 128)
        x = self.attn1(x)
        x = self.attn2(x)
        x = x.permute(0, 2, 1)  # (batch, 128, 60)

        # Flatten and classify
        x = x.flatten(1)  # (batch, 128*60)
        x = self.head(x)  # (batch, 7)

        return x


# ============================================================
# Dataset
# ============================================================

class FlowDataset(Dataset):
    """
    Loads preprocessed flow data from numpy files.

    Expected format:
        data/flow/
        ├── train/
        │   ├── aggregate/   — whole-home flow time series (N, 60)
        │   ├── labels/      — per-appliance labels (N, 7)
        │   └── pressure/    — pressure time series (N, 60)
        └── val/
            ├── aggregate/
            ├── labels/
            └── pressure/
    """

    def __init__(self, data_dir: str, split: str = "train"):
        self.data_dir = Path(data_dir) / split

        # Load aggregate flow data
        self.flow = np.load(self.data_dir / "aggregate" / "flow.npy")
        self.pressure = np.load(self.data_dir / "pressure" / "pressure.npy")
        self.labels = np.load(self.data_dir / "labels" / "labels.npy")

        print(f"Loaded {len(self.flow)} samples for {split}")

    def __len__(self):
        return len(self.flow)

    def __getitem__(self, idx):
        flow = self.flow[idx]       # (60,) flow rate in mL/min
        pressure = self.pressure[idx]  # (60,) pressure in kPa
        labels = self.labels[idx]    # (7,) per-appliance flow contribution

        # Normalize
        flow_norm = flow / 30000.0  # Normalize by max flow (30 L/min)
        pressure_norm = (pressure - 200.0) / 500.0  # Normalize around typical range

        # Stack into (2, 60) input
        x = np.stack([flow_norm, pressure_norm], axis=0)  # (2, 60)

        # Labels: per-appliance flow ratios (sum to 1)
        label_sum = labels.sum() + 1e-8
        labels_norm = labels / label_sum

        return (
            torch.FloatTensor(x),
            torch.FloatTensor(labels_norm)
        )


# ============================================================
# Training
# ============================================================

def train_model(args):
    """Train the flow disaggregation model."""

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on: {device}")

    # Datasets
    train_dataset = FlowDataset(args.data_dir, split="train")
    val_dataset = FlowDataset(args.data_dir, split="val")

    train_loader = DataLoader(train_dataset, batch_size=args.batch_size, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_dataset, batch_size=args.batch_size, shuffle=False, num_workers=4)

    # Model
    model = FlowDisaggregator().to(device)
    print(f"Model parameters: {sum(p.numel() for p in model.parameters()):,}")

    # Loss: combination of KL divergence (for ratios) and MSE (for absolute values)
    criterion_mse = nn.MSELoss()
    criterion_kl = nn.KLDivLoss(reduction="batchmean")
    optimizer = optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    best_val_loss = float('inf')

    for epoch in range(args.epochs):
        # Train
        model.train()
        train_loss = 0.0

        for batch_idx, (inputs, labels) in enumerate(train_loader):
            inputs, labels = inputs.to(device), labels.to(device)

            optimizer.zero_grad()
            outputs = model(inputs)

            # Combined loss: MSE for absolute values + KL divergence for ratios
            loss_mse = criterion_mse(outputs, labels)
            loss_kl = criterion_kl(torch.log_softmax(outputs, dim=1),
                                    torch.softmax(labels, dim=1))
            loss = loss_mse + 0.5 * loss_kl

            loss.backward()
            optimizer.step()

            train_loss += loss.item()

        avg_train_loss = train_loss / len(train_loader)

        # Validate
        model.eval()
        val_loss = 0.0
        all_outputs = []
        all_labels = []

        with torch.no_grad():
            for inputs, labels in val_loader:
                inputs, labels = inputs.to(device), labels.to(device)
                outputs = model(inputs)

                loss_mse = criterion_mse(outputs, labels)
                val_loss += loss_mse.item()

                all_outputs.append(outputs.cpu().numpy())
                all_labels.append(labels.cpu().numpy())

        avg_val_loss = val_loss / len(val_loader)

        print(f"Epoch {epoch+1}/{args.epochs} | "
              f"Train Loss: {avg_train_loss:.4f} | Val Loss: {avg_val_loss:.4f}")

        if avg_val_loss < best_val_loss:
            best_val_loss = avg_val_loss
            torch.save({
                'epoch': epoch,
                'model_state_dict': model.state_dict(),
                'val_loss': avg_val_loss,
            }, Path(args.output_dir) / "best_nilm_model.pt")
            print(f"  → New best model saved (val_loss: {avg_val_loss:.4f})")

        scheduler.step()

    print(f"\nTraining complete. Best validation loss: {best_val_loss:.4f}")
    return model


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train FlowGuard flow disaggregation model")
    parser.add_argument("--data_dir", type=str, default="data/flow",
                        help="Directory containing training data")
    parser.add_argument("--output_dir", type=str, default="models",
                        help="Output directory for saved models")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--batch_size", type=int, default=64)
    parser.add_argument("--lr", type=float, default=5e-4)

    args = parser.parse_args()
    Path(args.output_dir).mkdir(parents=True, exist_ok=True)

    model = train_model(args)
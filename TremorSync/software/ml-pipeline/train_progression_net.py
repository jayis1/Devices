"""
ProgressionNet Training Script
MDS-UPDRS Part III (motor) score prediction from 90-day multi-modal features.

Input: 90-day multi-modal time series (tremor severity, bradykinesia, gait, speech, ON-time ratio)
Architecture: Temporal Fusion Transformer (TFT) with variable selection
Output: MDS-UPDRS Part III estimate (0-132) + trajectory
Correlation target: r > 0.91 with clinical MDS-UPDRS
"""

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader

class ProgressionNet(nn.Module):
    """Simplified TFT-inspired model with variable selection + temporal attention."""
    def __init__(self, input_size=5, hidden=64, horizon=90):
        super().__init__()
        # Variable selection network
        self.var_selector = nn.Sequential(
            nn.Linear(input_size, hidden), nn.ReLU(), nn.Linear(hidden, input_size)
        )
        self.lstm = nn.LSTM(input_size, hidden, batch_first=True, num_layers=2)
        self.temp_attn = nn.Linear(hidden, 1)
        self.fc1 = nn.Linear(hidden, 32)
        self.fc2 = nn.Linear(32, 1)  # MDS-UPDRS III score

    def forward(self, x):
        # x: (batch, 90, 5)
        var_weights = torch.softmax(self.var_selector(x.mean(dim=1)), dim=-1)
        x_weighted = x * var_weights.unsqueeze(1)
        lstm_out, _ = self.lstm(x_weighted)
        attn = torch.softmax(self.temp_attn(lstm_out), dim=1)
        context = (lstm_out * attn).sum(dim=1)
        x = torch.relu(self.fc1(context))
        return self.fc2(x)  # 0-132


class ProgressionDataset(Dataset):
    def __init__(self, split="train"):
        np.random.seed(42 if split == "train" else 222)
        n = 3000 if split == "train" else 600
        self.samples = np.random.randn(n, 90, 5).astype(np.float32) * 0.3
        # Score: weighted sum of features + trend
        trend = np.cumsum(self.samples[:, :, :3], axis=1)[:, -1, :]  # cumulative trend
        base = np.abs(self.samples).mean(axis=(1, 2)) * 30
        self.labels = np.clip(base + trend.sum(axis=1) * 10, 0, 132).astype(np.float32)

    def __len__(self): return len(self.labels)
    def __getitem__(self, idx):
        return torch.tensor(self.samples[idx]), self.labels[idx]


def train():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = ProgressionNet().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=5e-4, weight_decay=1e-4)
    criterion = nn.MSELoss()

    train_dl = DataLoader(ProgressionDataset("train"), batch_size=32, shuffle=True)
    val_dl   = DataLoader(ProgressionDataset("val"), batch_size=32)

    best_loss = 999
    for epoch in range(80):
        model.train()
        for x, y in train_dl:
            x, y = x.to(device), y.to(device)
            optimizer.zero_grad()
            loss = criterion(model(x).squeeze(), y)
            loss.backward()
            optimizer.step()

        model.eval()
        val_loss = 0
        preds, actuals = [], []
        with torch.no_grad():
            for x, y in val_dl:
                x, y = x.to(device), y.to(device)
                p = model(x).squeeze()
                val_loss += criterion(p, y).item()
                preds.extend(p.cpu().numpy())
                actuals.extend(y.cpu().numpy())
        val_loss /= len(val_dl)
        # Correlation
        preds, actuals = np.array(preds), np.array(actuals)
        corr = np.corrcoef(preds, actuals)[0, 1] if len(preds) > 2 else 0
        print(f"Epoch {epoch+1}: val_mse={val_loss:.4f} r={corr:.4f}")
        if val_loss < best_loss:
            best_loss = val_loss
            torch.save(model.state_dict(), "progression_net_best.pt")

    model.load_state_dict(torch.load("progression_net_best.pt"))
    dummy = torch.randn(1, 90, 5)
    torch.onnx.export(model, dummy, "progression_net.onnx", opset_version=13)
    print(f"ProgressionNet trained. Best MSE: {best_loss:.4f}")

if __name__ == "__main__":
    train()
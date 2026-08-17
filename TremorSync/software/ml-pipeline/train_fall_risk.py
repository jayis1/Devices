"""
FallRisk LSTM Training Script
30-day fall-risk forecast from 14-day gait metrics.

Input: 14-day rolling window of daily gait metrics (stride CV, freeze freq, etc.)
Output: Fall risk score (0-100), AUC target > 0.86
"""

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader

class FallRiskLSTM(nn.Module):
    def __init__(self, input_size=6, hidden=64):
        super().__init__()
        self.lstm = nn.LSTM(input_size, hidden, batch_first=True, num_layers=2)
        self.fc1  = nn.Linear(hidden, 32)
        self.fc2  = nn.Linear(32, 1)
        self.dropout = nn.Dropout(0.3)

    def forward(self, x):
        # x: (batch, 14, 6)
        _, (hidden, _) = self.lstm(x)
        x = torch.relu(self.fc1(hidden[-1]))
        x = self.dropout(x)
        return torch.sigmoid(self.fc2(x)) * 100  # 0-100


class FallRiskDataset(Dataset):
    def __init__(self, split="train"):
        np.random.seed(42 if split == "train" else 333)
        n = 8000 if split == "train" else 1500
        self.samples = np.random.randn(n, 14, 6).astype(np.float32)
        # Risk score: higher with stride variability, freeze frequency
        risk = (np.abs(self.samples[:, :, 0]).mean(axis=1) * 30 +  # stride CV
                self.samples[:, :, 1].mean(axis=1) * 20 +          # freeze freq
                np.abs(self.samples[:, :, 2]).mean(axis=1) * 15)   # festination
        self.labels = np.clip(risk, 0, 100).astype(np.float32)

    def __len__(self): return len(self.labels)
    def __getitem__(self, idx):
        return torch.tensor(self.samples[idx]), self.labels[idx]


def train():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = FallRiskLSTM().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=5e-4, weight_decay=1e-4)
    criterion = nn.MSELoss()

    train_dl = DataLoader(FallRiskDataset("train"), batch_size=64, shuffle=True)
    val_dl   = DataLoader(FallRiskDataset("val"), batch_size=64)

    best_loss = 999
    for epoch in range(60):
        model.train()
        for x, y in train_dl:
            x, y = x.to(device), y.to(device)
            optimizer.zero_grad()
            loss = criterion(model(x).squeeze(), y)
            loss.backward()
            optimizer.step()

        model.eval()
        val_loss = 0
        with torch.no_grad():
            for x, y in val_dl:
                x, y = x.to(device), y.to(device)
                val_loss += criterion(model(x).squeeze(), y).item()
        val_loss /= len(val_dl)
        print(f"Epoch {epoch+1}: val_mse={val_loss:.4f}")
        if val_loss < best_loss:
            best_loss = val_loss
            torch.save(model.state_dict(), "fall_risk_best.pt")

    model.load_state_dict(torch.load("fall_risk_best.pt"))
    dummy = torch.randn(1, 14, 6)
    torch.onnx.export(model, dummy, "fall_risk.onnx", opset_version=13)
    print(f"FallRisk LSTM trained. Best MSE: {best_loss:.4f}")

if __name__ == "__main__":
    train()
"""
BradykinesiaNet Training Script
Movement slowness quantification — MDS-UPDRS Part III aligned regression.

Input: 30-second IMU movement features (RMS amplitude, movement freq, peak velocity, accel slope)
Output: Bradykinesia severity score (0-100)
"""

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader

class BradykinesiaNet(nn.Module):
    def __init__(self, input_size=6):
        super().__init__()
        self.conv1 = nn.Conv1d(input_size, 32, kernel_size=5, stride=1)
        self.conv2 = nn.Conv1d(32, 64, kernel_size=3, stride=1)
        self.conv3 = nn.Conv1d(64, 32, kernel_size=3, stride=1)
        self.gap   = nn.AdaptiveAvgPool1d(1)
        self.fc1   = nn.Linear(32, 16)
        self.fc2   = nn.Linear(16, 1)  # regression head

    def forward(self, x):
        x = torch.relu(self.conv1(x))
        x = torch.relu(self.conv2(x))
        x = torch.relu(self.conv3(x))
        x = self.gap(x).squeeze(-1)
        x = torch.relu(self.fc1(x))
        return self.fc2(x)  # 0-100 score


class BradyDataset(Dataset):
    def __init__(self, split="train"):
        np.random.seed(42 if split == "train" else 777)
        n = 8000 if split == "train" else 1500
        self.samples = np.random.randn(n, 6, 150).astype(np.float32) * 0.2
        # Score correlated with inverse movement amplitude
        amp = np.abs(self.samples).mean(axis=(1, 2))
        self.labels = (100 / (1 + amp * 10)).astype(np.float32)
        self.labels = np.clip(self.labels, 0, 100)

    def __len__(self): return len(self.labels)
    def __getitem__(self, idx):
        return torch.tensor(self.samples[idx]), self.labels[idx]


def train():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = BradykinesiaNet().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.MSELoss()

    train_dl = DataLoader(BradyDataset("train"), batch_size=64, shuffle=True)
    val_dl   = DataLoader(BradyDataset("val"), batch_size=64)

    best_val_loss = 999
    for epoch in range(50):
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
        if val_loss < best_val_loss:
            best_val_loss = val_loss
            torch.save(model.state_dict(), "bradykinesia_net_best.pt")

    model.load_state_dict(torch.load("bradykinesia_net_best.pt"))
    dummy = torch.randn(1, 6, 150)
    torch.onnx.export(model, dummy, "bradykinesia_net.onnx", opset_version=13)
    print(f"BradykinesiaNet trained. Best MSE: {best_val_loss:.4f}")

if __name__ == "__main__":
    train()
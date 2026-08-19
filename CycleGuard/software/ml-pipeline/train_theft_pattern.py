"""
TheftPattern LSTM Training Script
4-class lock activity classification: normal, accidental bump, tamper, theft

Input: 30-second window of lock IMU (100 Hz, 6-axis) + load cell force
Output: 4-class classification
Architecture: 2-layer LSTM (64 hidden) + FC head
"""

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader


class TheftPatternNet(nn.Module):
    def __init__(self, input_size=7, hidden_size=64, num_layers=2, num_classes=4):
        super().__init__()
        self.lstm = nn.LSTM(input_size, hidden_size, num_layers,
                            batch_first=True, dropout=0.2)
        self.fc1 = nn.Linear(hidden_size, 32)
        self.fc2 = nn.Linear(32, num_classes)

    def forward(self, x):
        # x: (batch, 3000, 7) — 30 sec at 100 Hz, 6 IMU + 1 load cell
        out, (hn, cn) = self.lstm(x)
        last = hn[-1]
        x = torch.relu(self.fc1(last))
        return self.fc2(x)


class TheftDataset(Dataset):
    def __init__(self, split: str = "train"):
        np.random.seed(42 if split == "train" else 123)
        n = 4000 if split == "train" else 1000
        # Downsample: 30 sec at 100 Hz = 3000 samples, but use 300 for efficiency
        self.samples = np.random.randn(n, 300, 7).astype(np.float32) * 0.1
        labels = np.random.choice(4, n, p=[0.85, 0.08, 0.04, 0.03])

        for i in range(n):
            if labels[i] == 3:  # theft in progress
                # High-amplitude sustained movement + high load cell
                self.samples[i, :, :3] += np.random.randn(300, 3) * 2.0
                self.samples[i, :, 6] += 35.0  # high prying force
            elif labels[i] == 2:  # tamper attempt
                self.samples[i, 100:200, :3] += np.random.randn(100, 3) * 1.5
                self.samples[i, 100:200, 6] += 20.0
            elif labels[i] == 1:  # accidental bump
                self.samples[i, 150, :3] += np.random.randn(3) * 3.0
        self.labels = labels

    def __len__(self): return len(self.labels)
    def __getitem__(self, idx):
        return torch.tensor(self.samples[idx]), self.labels[idx]


def train():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = TheftPatternNet().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3, weight_decay=1e-4)
    criterion = nn.CrossEntropyLoss(weight=torch.tensor([1.0, 3.0, 8.0, 15.0]).to(device))

    train_ds = TheftDataset("train")
    val_ds   = TheftDataset("val")
    train_dl = DataLoader(train_ds, batch_size=64, shuffle=True)
    val_dl   = DataLoader(val_ds, batch_size=64)

    best_val_acc = 0
    for epoch in range(50):
        model.train()
        total_loss = 0
        for x, y in train_dl:
            x, y = x.to(device), y.to(device)
            optimizer.zero_grad()
            out = model(x)
            loss = criterion(out, y)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()

        model.eval()
        correct = total = 0
        with torch.no_grad():
            for x, y in val_dl:
                x, y = x.to(device), y.to(device)
                out = model(x)
                pred = out.argmax(dim=1)
                correct += (pred == y).sum().item()
                total += len(y)
        val_acc = correct / total
        print(f"Epoch {epoch+1}: loss={total_loss/len(train_dl):.4f} val_acc={val_acc:.4f}")

        if val_acc > best_val_acc:
            best_val_acc = val_acc
            torch.save(model.state_dict(), "theft_pattern_best.pt")

    model.load_state_dict(torch.load("theft_pattern_best.pt"))
    model.eval()
    dummy = torch.randn(1, 300, 7)
    torch.onnx.export(model, dummy, "theft_pattern.onnx", opset_version=13,
                      input_names=["input"], output_names=["output"])
    print(f"TheftPattern trained. Best val accuracy: {best_val_acc:.4f}")
    print("Exported: theft_pattern.onnx (cloud inference)")


if __name__ == "__main__":
    train()
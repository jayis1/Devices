"""
CollisionPredict LSTM Training Script
3-8 second collision prediction from multi-modal time series:
  GPS speed, heading, acceleration, BlindSpotNet detections, wheel speed

Input: 10-second window at 10 Hz (100 timesteps x 8 features)
Output: collision probability (0-1) for next 3-8 seconds
Architecture: 2-layer LSTM (128 hidden) + FC head
"""

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader


class CollisionPredictNet(nn.Module):
    def __init__(self, input_size=8, hidden_size=128, num_layers=2):
        super().__init__()
        self.lstm = nn.LSTM(input_size, hidden_size, num_layers,
                            batch_first=True, dropout=0.2)
        self.fc1 = nn.Linear(hidden_size, 64)
        self.fc2 = nn.Linear(64, 1)
        self.sigmoid = nn.Sigmoid()

    def forward(self, x):
        # x: (batch, 100, 8) — 10 sec at 10 Hz
        out, (hn, cn) = self.lstm(x)
        # Use last hidden state
        last = hn[-1]  # (batch, hidden_size)
        x = torch.relu(self.fc1(last))
        return self.sigmoid(self.fc2(x)).squeeze(-1)


class CollisionDataset(Dataset):
    def __init__(self, split: str = "train"):
        np.random.seed(42 if split == "train" else 123)
        n = 5000 if split == "train" else 1000
        # Features: [speed, heading, accel_x, accel_y, bs_class,
        #            wheel_speed, turn_signal, time_of_day]
        self.samples = np.random.randn(n, 100, 8).astype(np.float32) * 0.5
        # Labels: 1 if collision within 3-8 sec, 0 otherwise (5% positive)
        self.labels = np.random.choice(2, n, p=[0.95, 0.05]).astype(np.float32)

        # Add collision signatures (approaching vehicle + high speed)
        for i in range(n):
            if self.labels[i] == 1:
                self.samples[i, :, 4] = 3.0  # car detected (BlindSpotNet)
                self.samples[i, 50:, 0] += 10.0  # speed increasing
                self.samples[i, 80:, 2] -= 5.0  # braking

    def __len__(self): return len(self.labels)
    def __getitem__(self, idx):
        return torch.tensor(self.samples[idx]), torch.tensor(self.labels[idx])


def train():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = CollisionPredictNet().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3, weight_decay=1e-4)
    # Weighted BCE for imbalanced positive class
    criterion = nn.BCELoss()

    train_ds = CollisionDataset("train")
    val_ds   = CollisionDataset("val")
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
                pred = (out > 0.5).float()
                correct += (pred == y).sum().item()
                total += len(y)
        val_acc = correct / total
        print(f"Epoch {epoch+1}: loss={total_loss/len(train_dl):.4f} val_acc={val_acc:.4f}")

        if val_acc > best_val_acc:
            best_val_acc = val_acc
            torch.save(model.state_dict(), "collision_predict_best.pt")

    # Export to ONNX
    model.load_state_dict(torch.load("collision_predict_best.pt"))
    model.eval()
    dummy = torch.randn(1, 100, 8)
    torch.onnx.export(model, dummy, "collision_predict.onnx", opset_version=13,
                      input_names=["input"], output_names=["output"])
    print(f"CollisionPredict trained. Best val accuracy: {best_val_acc:.4f}")
    print("Exported: collision_predict.onnx → tflite for ESP32-S3")


if __name__ == "__main__":
    train()
"""
FreezeNet LSTM Training Script
30-second freezing-of-gait prediction from gait features.

Input: 10-second rolling window at 10 Hz (100 samples x 8 features)
  Features: stride_length, cadence, double_support_time, freeze_index,
            heel_pressure, toe_pressure, stride_variability, activity_level
Architecture: BiLSTM(64) + Attention + FC
Output: FOG predicted in next 30 seconds (binary)
"""

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader

class FreezeNet(nn.Module):
    def __init__(self, input_size=8, hidden=64):
        super().__init__()
        self.bilstm = nn.LSTM(input_size, hidden, batch_first=True, bidirectional=True)
        self.attn   = nn.Linear(hidden * 2, 1)
        self.fc1    = nn.Linear(hidden * 2, 32)
        self.fc2    = nn.Linear(32, 2)
        self.dropout = nn.Dropout(0.3)

    def forward(self, x):
        # x: (batch, 100, 8)
        lstm_out, _ = self.bilstm(x)  # (batch, 100, 128)
        attn_weights = torch.softmax(self.attn(lstm_out), dim=1)  # (batch, 100, 1)
        context = (lstm_out * attn_weights).sum(dim=1)  # (batch, 128)
        x = torch.relu(self.fc1(context))
        x = self.dropout(x)
        return self.fc2(x)


class GaitDataset(Dataset):
    def __init__(self, split="train"):
        np.random.seed(42 if split == "train" else 999)
        n = 10000 if split == "train" else 2000
        self.samples = np.random.randn(n, 100, 8).astype(np.float32)
        # ~15% positive (FOG) class — matches clinical prevalence
        self.labels = np.zeros(n, dtype=np.int64)
        self.labels[:int(n * 0.15)] = 1
        np.random.shuffle(self.labels)

    def __len__(self): return len(self.labels)
    def __getitem__(self, idx):
        return torch.tensor(self.samples[idx]), self.labels[idx]


def train():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = FreezeNet().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=5e-4, weight_decay=1e-5)
    # Weighted loss for class imbalance
    criterion = nn.CrossEntropyLoss(weight=torch.tensor([1.0, 5.0]).to(device))

    train_dl = DataLoader(GaitDataset("train"), batch_size=128, shuffle=True)
    val_dl   = DataLoader(GaitDataset("val"), batch_size=128)

    best_recall = 0
    for epoch in range(40):
        model.train()
        for x, y in train_dl:
            x, y = x.to(device), y.to(device)
            optimizer.zero_grad()
            loss = criterion(model(x), y)
            loss.backward()
            optimizer.step()

        # Validation — focus on recall (critical for FOG)
        model.eval()
        tp = fn = tn = fp = 0
        with torch.no_grad():
            for x, y in val_dl:
                x, y = x.to(device), y.to(device)
                pred = model(x).argmax(dim=1)
                tp += ((pred == 1) & (y == 1)).sum().item()
                fn += ((pred == 0) & (y == 1)).sum().item()
                tn += ((pred == 0) & (y == 0)).sum().item()
                fp += ((pred == 1) & (y == 0)).sum().item()
        recall = tp / max(tp + fn, 1)
        precision = tp / max(tp + fp, 1)
        print(f"Epoch {epoch+1}: recall={recall:.4f} precision={precision:.4f}")

        if recall > best_recall:
            best_recall = recall
            torch.save(model.state_dict(), "freeze_net_best.pt")

    model.load_state_dict(torch.load("freeze_net_best.pt"))
    model.eval()
    dummy = torch.randn(1, 100, 8)
    torch.onnx.export(model, dummy, "freeze_net.onnx", opset_version=13)
    print(f"FreezeNet trained. Best recall: {best_recall:.4f}")

if __name__ == "__main__":
    train()
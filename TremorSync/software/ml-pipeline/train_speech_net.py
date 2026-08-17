"""
SpeechNet CNN Training Script
5-class speech classification: Normal, Mild/Moderate/Severe Hypophonia, Dysarthric

Input: Throat-mic audio features (RMS, F0 mean, F0 std, jitter, shimmer, HNR, formant slopes)
Output: Softmax 5-class + severity score (0-100)
"""

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader

class SpeechNet(nn.Module):
    def __init__(self, input_size=8, num_classes=5):
        super().__init__()
        self.conv1 = nn.Conv1d(1, 32, kernel_size=3)
        self.conv2 = nn.Conv1d(32, 64, kernel_size=3)
        self.conv3 = nn.Conv1d(64, 128, kernel_size=3)
        self.conv4 = nn.Conv1d(128, 64, kernel_size=3)
        self.gap   = nn.AdaptiveAvgPool1d(1)
        self.fc1   = nn.Linear(64, 32)
        self.fc2   = nn.Linear(32, num_classes)
        self.fc_sev = nn.Linear(32, 1)  # severity regression head
        self.dropout = nn.Dropout(0.2)

    def forward(self, x):
        # x: (batch, 1, 8) — feature vector as 1D signal
        x = torch.relu(self.conv1(x))
        x = torch.relu(self.conv2(x))
        x = torch.relu(self.conv3(x))
        x = torch.relu(self.conv4(x))
        x = self.gap(x).squeeze(-1)
        x = torch.relu(self.fc1(x))
        x = self.dropout(x)
        return self.fc2(x), self.fc_sev(x).squeeze()


class SpeechDataset(Dataset):
    def __init__(self, split="train"):
        np.random.seed(42 if split == "train" else 555)
        n = 6000 if split == "train" else 1200
        # Features: RMS, F0_mean, F0_std, jitter, shimmer, HNR, F1_slope, F2_slope
        self.samples = np.random.randn(n, 1, 8).astype(np.float32)
        self.labels = np.random.randint(0, 5, n)
        # Severity correlated with class
        self.severity = self.labels.astype(np.float32) * 20 + np.random.randn(n) * 5

    def __len__(self): return len(self.labels)
    def __getitem__(self, idx):
        return (torch.tensor(self.samples[idx]),
                self.labels[idx],
                self.severity[idx])


def train():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = SpeechNet().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    cls_loss = nn.CrossEntropyLoss()
    reg_loss = nn.MSELoss()

    train_dl = DataLoader(SpeechDataset("train"), batch_size=64, shuffle=True)
    val_dl   = DataLoader(SpeechDataset("val"), batch_size=64)

    best_acc = 0
    for epoch in range(50):
        model.train()
        for x, y, sev in train_dl:
            x, y, sev = x.to(device), y.to(device), sev.to(device)
            optimizer.zero_grad()
            cls_out, sev_out = model(x)
            loss = cls_loss(cls_out, y) + 0.5 * reg_loss(sev_out, sev)
            loss.backward()
            optimizer.step()

        model.eval()
        correct = total = 0
        with torch.no_grad():
            for x, y, sev in val_dl:
                x, y = x.to(device), y.to(device)
                pred = model(x)[0].argmax(dim=1)
                correct += (pred == y).sum().item()
                total += len(y)
        acc = correct / total
        print(f"Epoch {epoch+1}: val_acc={acc:.4f}")
        if acc > best_acc:
            best_acc = acc
            torch.save(model.state_dict(), "speech_net_best.pt")

    model.load_state_dict(torch.load("speech_net_best.pt"))
    dummy = torch.randn(1, 1, 8)
    torch.onnx.export(model, dummy, "speech_net.onnx", opset_version=13)
    print(f"SpeechNet trained. Best accuracy: {best_acc:.4f}")

if __name__ == "__main__":
    train()
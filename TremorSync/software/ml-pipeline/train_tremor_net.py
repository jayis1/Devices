"""
TremorNet 1D-CNN Training Script
4-class tremor classification: None, Resting, Postural, Action

Input: 3-axis accel + 3-axis gyro @ 200 Hz, 2-second windows (400 samples x 6 channels)
Architecture: 4 Conv1D + 2 FC, quantized to int8 for ESP32-S3 tflite-micro
"""

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.quantization import quantize_dynamic

# ---- Model ----
class TremorNet(nn.Module):
    def __init__(self, num_classes=4):
        super().__init__()
        self.conv1 = nn.Conv1d(6, 32, kernel_size=7, stride=2)
        self.bn1   = nn.BatchNorm1d(32)
        self.conv2 = nn.Conv1d(32, 64, kernel_size=5, stride=2)
        self.bn2   = nn.BatchNorm1d(64)
        self.conv3 = nn.Conv1d(64, 128, kernel_size=3, stride=1)
        self.bn3   = nn.BatchNorm1d(128)
        self.conv4 = nn.Conv1d(128, 64, kernel_size=3, stride=1)
        self.bn4   = nn.BatchNorm1d(64)
        self.gap   = nn.AdaptiveAvgPool1d(1)
        self.fc1   = nn.Linear(64, 32)
        self.fc2   = nn.Linear(32, num_classes)
        self.dropout = nn.Dropout(0.2)

    def forward(self, x):
        # x: (batch, 6, 400)
        x = torch.relu(self.bn1(self.conv1(x)))
        x = torch.relu(self.bn2(self.conv2(x)))
        x = torch.relu(self.bn3(self.conv3(x)))
        x = torch.relu(self.bn4(self.conv4(x)))
        x = self.gap(x).squeeze(-1)
        x = torch.relu(self.fc1(x))
        x = self.dropout(x)
        return self.fc2(x)


# ---- Dataset ----
class TremorDataset(Dataset):
    def __init__(self, data_path: str, split: str = "train"):
        # In production: load from PPMI / OPDC tremor datasets
        # Synthetic placeholder data
        np.random.seed(42 if split == "train" else 123)
        n = 5000 if split == "train" else 1000
        self.samples = np.random.randn(n, 6, 400).astype(np.float32) * 0.1
        self.labels  = np.random.randint(0, 4, n)

    def __len__(self): return len(self.labels)
    def __getitem__(self, idx):
        return torch.tensor(self.samples[idx]), self.labels[idx]


def train():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = TremorNet().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.CrossEntropyLoss()

    train_ds = TremorDataset("data/", "train")
    val_ds   = TremorDataset("data/", "val")
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

        # Validation
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
            torch.save(model.state_dict(), "tremor_net_best.pt")

    # Export to ONNX
    model.load_state_dict(torch.load("tremor_net_best.pt"))
    model.eval()
    dummy = torch.randn(1, 6, 400)
    torch.onnx.export(model, dummy, "tremor_net.onnx", opset_version=13,
                      input_names=["input"], output_names=["output"])
    print(f"TremorNet trained. Best val accuracy: {best_val_acc:.4f}")
    print("Exported: tremor_net.onnx (convert to tflite with onnx2tf)")

if __name__ == "__main__":
    train()
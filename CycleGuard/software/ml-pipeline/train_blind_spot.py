"""
BlindSpotNet Training Script
8-class rear proximity classification from camera frames:
  clear, bicycle, motorcycle, car, truck, bus, pedestrian, obstacle

Input: 224x224 RGB camera frames (from OV5640 rear-facing on Hub)
Architecture: MobileNetV3-small backbone + FC head, int8 quantized for ESP32-S3
"""

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
import torchvision.models as models
import torchvision.transforms as transforms


class BlindSpotNet(nn.Module):
    def __init__(self, num_classes=8):
        super().__init__()
        # MobileNetV3-small backbone (lightweight for ESP32-S3)
        self.backbone = models.mobilenet_v3_small(weights=None, num_classes=128)
        # Replace classifier for 8 classes
        self.backbone.classifier[3] = nn.Linear(128, num_classes)

    def forward(self, x):
        # x: (batch, 3, 224, 224)
        return self.backbone(x)


class BlindSpotDataset(Dataset):
    def __init__(self, split: str = "train"):
        # In production: load from COCO + custom cycling rear-camera dataset
        np.random.seed(42 if split == "train" else 123)
        n = 5000 if split == "train" else 1000
        self.samples = np.random.randn(n, 3, 224, 224).astype(np.float32)
        self.labels = np.random.randint(0, 8, n)
        self.transform = transforms.Compose([
            transforms.Normalize(mean=[0.485, 0.456, 0.406],
                                 std=[0.229, 0.224, 0.225])
        ])

    def __len__(self): return len(self.labels)
    def __getitem__(self, idx):
        return torch.tensor(self.samples[idx]), self.labels[idx]


def train():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = BlindSpotNet().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.CrossEntropyLoss()

    train_ds = BlindSpotDataset("train")
    val_ds   = BlindSpotDataset("val")
    train_dl = DataLoader(train_ds, batch_size=32, shuffle=True)
    val_dl   = DataLoader(val_ds, batch_size=32)

    best_val_acc = 0
    for epoch in range(40):
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
            torch.save(model.state_dict(), "blind_spot_net_best.pt")

    # Export to ONNX
    model.load_state_dict(torch.load("blind_spot_net_best.pt"))
    model.eval()
    dummy = torch.randn(1, 3, 224, 224)
    torch.onnx.export(model, dummy, "blind_spot_net.onnx", opset_version=13,
                      input_names=["input"], output_names=["output"])
    print(f"BlindSpotNet trained. Best val accuracy: {best_val_acc:.4f}")
    print("Exported: blind_spot_net.onnx → tflite int8 for ESP32-S3")


if __name__ == "__main__":
    train()
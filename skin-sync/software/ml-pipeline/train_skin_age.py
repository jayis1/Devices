"""
SkinSync Skin Age Model — Longitudinal Skin Quality Scoring

Computes a "skin age" from multispectral scan features:
  - Wrinkle depth: NIR surface topography analysis
  - Elasticity proxy: NIR backscatter pattern
  - Hydration: UV fluorescence quenching (water reduces fluorescence)
  - Pigmentation index: polarized tone analysis (evenness)
  - Pore size: white light surface texture

Architecture: MobileNetV3-based regressor → single output (skin age in years)
"""

import os
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW

IMG_SIZE      = 224
IMG_CHANNELS  = 4
BATCH_SIZE    = 32
EPOCHS        = 70
LR            = 1e-4
DEVICE        = "cuda" if torch.cuda.is_available() else "cpu"
MODEL_SAVE    = os.path.join(os.path.dirname(__file__), "skin_age_model.pt")


class SkinAgeRegressor(nn.Module):
    """MobileNetV3-based skin age regressor."""
    def __init__(self):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv2d(IMG_CHANNELS, 16, 3, stride=2, padding=1),
            nn.BatchNorm2d(16), nn.ReLU6(),
            nn.Conv2d(16, 32, 3, stride=1, padding=1),
            nn.BatchNorm2d(32), nn.ReLU6(),
            nn.Conv2d(32, 64, 3, stride=2, padding=1),
            nn.BatchNorm2d(64), nn.ReLU6(),
            nn.Conv2d(64, 128, 3, stride=2, padding=1),
            nn.BatchNorm2d(128), nn.ReLU6(),
            nn.Conv2d(128, 256, 3, stride=2, padding=1),
            nn.BatchNorm2d(256), nn.ReLU6(),
            nn.Conv2d(256, 512, 3, stride=2, padding=1),
            nn.BatchNorm2d(512), nn.ReLU6(),
            nn.AdaptiveAvgPool2d(1),
        )
        self.regressor = nn.Sequential(
            nn.Flatten(), nn.Dropout(0.3),
            nn.Linear(512, 128), nn.ReLU6(),
            nn.Linear(128, 1), nn.ReLU()  # skin age in years
        )

    def forward(self, x):
        return self.regressor(self.features(x))


class SkinAgeDataset(Dataset):
    """Synthetic skin age data. In production: annotated multispectral scans."""
    def __init__(self, n_samples=600):
        np.random.seed(42)
        self.samples = []
        self.labels = []
        for _ in range(n_samples):
            age = np.random.randint(20, 70)
            img = np.random.normal(0.5, 0.15, (IMG_CHANNELS, IMG_SIZE, IMG_SIZE))

            # Age-related spectral signatures:
            # Young skin: high NIR (hydrated, elastic), even polarized tone
            # Old skin: lower NIR (dehydrated), uneven tone, more wrinkles in white
            age_factor = (age - 20) / 50.0  # 0 (young) → 1 (old)
            img[2] = np.clip(img[2] - age_factor * 0.2, 0, 1)  # lower NIR
            img[3] = np.clip(img[3] - age_factor * 0.15, 0, 1) # uneven tone
            img[0] = np.clip(img[0] - age_factor * 0.1, 0, 1)  # wrinkles

            self.samples.append(img.astype(np.float32))
            self.labels.append(float(age))

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        return torch.tensor(self.samples[idx]), torch.tensor(self.labels[idx], dtype=torch.float32)


def train():
    dataset = SkinAgeDataset(n_samples=600)
    loader = DataLoader(dataset, batch_size=BATCH_SIZE, shuffle=True)

    model = SkinAgeRegressor().to(DEVICE)
    optimizer = AdamW(model.parameters(), lr=LR)
    criterion = nn.MSELoss()

    for epoch in range(EPOCHS):
        model.train()
        total_loss = 0
        for x, y in loader:
            x, y = x.to(DEVICE), y.to(DEVICE)
            optimizer.zero_grad()
            pred = model(x).squeeze()
            loss = criterion(pred, y)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
        if (epoch + 1) % 10 == 0:
            rmse = (total_loss / len(loader)) ** 0.5
            print(f"Epoch {epoch+1}/{EPOCHS} — Loss: {total_loss/len(loader):.4f} RMSE: {rmse:.2f}y")

    torch.save(model.state_dict(), MODEL_SAVE)
    print(f"✓ Skin age model saved to {MODEL_SAVE}")


if __name__ == "__main__":
    train()
"""
SkinSync Melanoma ABCDE Detector Training — Lesion Analysis

Trains a lesion analysis model for melanoma warning signs using the ABCDE
criteria: Asymmetry, Border irregularity, Color variegation, Diameter,
Evolution (change over time).

Architecture:
  - YOLOv8-nano for lesion detection (bounding box)
  - Custom CNN for ABCDE scoring (5 binary sub-scores → 0-100 risk)
  - Evolution: comparison with prior scan (image diff + size change)

Datasets: HAM10000, ISIC 2020/2024, Derm7pt, proprietary longitudinal lesion scans.
Edge: binary "benign/suspect" pre-screen (TFLite Micro, <200KB).
"""

import os
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW

IMG_SIZE      = 224
IMG_CHANNELS  = 4   # white, UV, NIR, polarized
BATCH_SIZE    = 16
EPOCHS        = 100
LR            = 1e-4
DEVICE        = "cuda" if torch.cuda.is_available() else "cpu"
MODEL_SAVE    = os.path.join(os.path.dirname(__file__), "abcde_detector.pt")


class ABCDEDetector(nn.Module):
    """ABCDE lesion risk scorer.

    Outputs 5 sub-scores (0-1 each) + overall risk (0-100):
      A (asymmetry): shape analysis — symmetric lesion = low risk
      B (border): irregular/jagged border = high risk
      C (color): multiple colors/variegation = high risk
      D (diameter): >6mm = higher risk
      E (evolution): change from prior scan = highest risk indicator
    """
    def __init__(self):
        super().__init__()
        # Shared feature extractor
        self.features = nn.Sequential(
            nn.Conv2d(IMG_CHANNELS, 32, 3, stride=2, padding=1),
            nn.BatchNorm2d(32), nn.ReLU6(),
            nn.Conv2d(32, 64, 3, stride=1, padding=1),
            nn.BatchNorm2d(64), nn.ReLU6(),
            nn.Conv2d(64, 128, 3, stride=2, padding=1),
            nn.BatchNorm2d(128), nn.ReLU6(),
            nn.Conv2d(128, 256, 3, stride=2, padding=1),
            nn.BatchNorm2d(256), nn.ReLU6(),
            nn.Conv2d(256, 512, 3, stride=2, padding=1),
            nn.BatchNorm2d(512), nn.ReLU6(),
            nn.AdaptiveAvgPool2d(1),
        )
        # ABCDE sub-scores
        self.asymmetry_head = nn.Sequential(nn.Linear(512, 128), nn.ReLU(),
                                             nn.Linear(128, 1), nn.Sigmoid())
        self.border_head = nn.Sequential(nn.Linear(512, 128), nn.ReLU(),
                                          nn.Linear(128, 1), nn.Sigmoid())
        self.color_head = nn.Sequential(nn.Linear(512, 128), nn.ReLU(),
                                         nn.Linear(128, 1), nn.Sigmoid())
        self.diameter_head = nn.Sequential(nn.Linear(512, 128), nn.ReLU(),
                                            nn.Linear(128, 1), nn.Sigmoid())
        self.evolution_head = nn.Sequential(nn.Linear(512, 128), nn.ReLU(),
                                            nn.Linear(128, 1), nn.Sigmoid())

    def forward(self, x):
        f = self.features(x).flatten(1)
        a = self.asymmetry_head(f)
        b = self.border_head(f)
        c = self.color_head(f)
        d = self.diameter_head(f)
        e = self.evolution_head(f)
        # Weighted: Evolution is the strongest indicator (0.3),
        # Color + Border next (0.2 each), Asymmetry (0.15), Diameter (0.15)
        risk = (a * 0.15 + b * 0.20 + c * 0.20 + d * 0.15 + e * 0.30) * 100
        return risk, a, b, c, d, e


class LesionDataset(Dataset):
    """Synthetic lesion data. In production: HAM10000 + ISIC + Derm7pt."""
    def __init__(self, n_samples=800):
        np.random.seed(42)
        self.samples = []
        self.labels = []
        for _ in range(n_samples):
            # Generate synthetic lesion image (4-channel)
            img = np.random.normal(0.5, 0.15, (IMG_CHANNELS, IMG_SIZE, IMG_SIZE))
            # ABCDE sub-scores (0 or 1)
            a = np.random.randint(0, 2)
            b = np.random.randint(0, 2)
            c = np.random.randint(0, 2)
            d = np.random.randint(0, 2)
            e = np.random.randint(0, 2)

            # Encode spectral signatures:
            # Asymmetric lesion: irregular shape in polarized channel
            if a: img[3] = np.clip(img[3] + np.random.normal(0, 0.1, img[3].shape), 0, 1)
            # Irregular border: edge patterns in white
            if b: img[0] = np.clip(img[0] + np.random.normal(0, 0.1, img[0].shape), 0, 1)
            # Color variegation: multi-tone in polarized
            if c: img[3] = np.clip(img[3] * np.random.uniform(0.5, 1.5, img[3].shape), 0, 1)
            # Large diameter: larger affected area
            if d: img = np.clip(img + 0.1, 0, 1)
            # Evolution: NIR shows sub-surface change
            if e: img[2] = np.clip(img[2] + 0.2, 0, 1)

            self.samples.append(img.astype(np.float32))
            # Overall risk score 0-100
            risk = (a * 15 + b * 20 + c * 20 + d * 15 + e * 30)
            self.labels.append((a, b, c, d, e, risk))

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        img = torch.tensor(self.samples[idx])
        a, b, c, d, e, risk = self.labels[idx]
        return img, (torch.tensor(risk, dtype=torch.float32),
                     torch.tensor(a, dtype=torch.float32),
                     torch.tensor(b, dtype=torch.float32),
                     torch.tensor(c, dtype=torch.float32),
                     torch.tensor(d, dtype=torch.float32),
                     torch.tensor(e, dtype=torch.float32))


def train():
    dataset = LesionDataset(n_samples=800)
    loader = DataLoader(dataset, batch_size=BATCH_SIZE, shuffle=True)

    model = ABCDEDetector().to(DEVICE)
    optimizer = AdamW(model.parameters(), lr=LR)
    # MSE for risk score, BCE for sub-scores
    mse = nn.MSELoss()
    bce = nn.BCELoss()

    for epoch in range(EPOCHS):
        model.train()
        total_loss = 0
        for x, (risk, a, b, c, d, e) in loader:
            x = x.to(DEVICE)
            risk, a, b, c, d, e = [v.to(DEVICE) for v in (risk, a, b, c, d, e)]
            optimizer.zero_grad()
            pred_risk, pa, pb, pc, pd, pe = model(x)
            loss = (mse(pred_risk.squeeze(), risk) +
                    bce(pa.squeeze(), a) + bce(pb.squeeze(), b) +
                    bce(pc.squeeze(), c) + bce(pd.squeeze(), d) +
                    bce(pe.squeeze(), e))
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{EPOCHS} — Loss: {total_loss/len(loader):.4f}")

    torch.save(model.state_dict(), MODEL_SAVE)
    print(f"✓ Model saved to {MODEL_SAVE}")


if __name__ == "__main__":
    train()
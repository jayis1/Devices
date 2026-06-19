"""
PawSync Lameness Detector Training

Trains a model to detect lameness severity (0-4) from gait symmetry features:
  - Stride length (cm)
  - Stance time (ms)
  - Gait symmetry index (0-1000)
  - Weight-bearing asymmetry (0-1000)

Output: lameness grade 0-4 (0=normal, 4=severe)

Based on veterinary gait analysis literature adapted for collar-mounted IMU.
"""

import os
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW

INPUT_DIM   = 4   # stride, stance, symmetry, weight_asym
NUM_CLASSES = 5   # grades 0-4
BATCH_SIZE  = 32
EPOCHS      = 30
LR          = 1e-3
DEVICE      = "cuda" if torch.cuda.is_available() else "cpu"
MODEL_SAVE  = os.path.join(os.path.dirname(__file__), "lameness_model.pt")


class LamenessModel(nn.Module):
    """Simple MLP for lameness grading from gait features."""

    def __init__(self):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(INPUT_DIM, 32),
            nn.ReLU(),
            nn.Linear(32, 16),
            nn.ReLU(),
            nn.Linear(16, NUM_CLASSES),
        )

    def forward(self, x):
        return self.net(x)


class LamenessDataset(Dataset):
    """Synthetic gait data for each lameness grade.

    Based on thresholds from veterinary literature:
      Grade 0 (normal): symmetry < 40, weight_asym < 50
      Grade 1 (mild):   symmetry 40-80, weight_asym 50-100
      Grade 2 (moderate): symmetry 80-150, weight_asym 100-200
      Grade 3 (marked): symmetry 150-250, weight_asym 200-350
      Grade 4 (severe): symmetry > 250, weight_asym > 350
    """

    def __init__(self, n_samples=200, split="train"):
        rng = np.random.default_rng(seed=42 if split == "train" else 7)
        self.x = np.zeros((n_samples * NUM_CLASSES, INPUT_DIM), dtype=np.float32)
        self.y = np.zeros(n_samples * NUM_CLASSES, dtype=np.int64)

        for grade in range(NUM_CLASSES):
            for i in range(n_samples):
                idx = grade * n_samples + i
                self.y[idx] = grade

                if grade == 0:
                    self.x[idx, 0] = rng.uniform(40, 80)    # stride cm
                    self.x[idx, 1] = rng.uniform(200, 350)  # stance ms
                    self.x[idx, 2] = rng.uniform(0, 40)     # symmetry
                    self.x[idx, 3] = rng.uniform(0, 50)     # weight asym
                elif grade == 1:
                    self.x[idx, 0] = rng.uniform(35, 70)
                    self.x[idx, 1] = rng.uniform(250, 400)
                    self.x[idx, 2] = rng.uniform(40, 80)
                    self.x[idx, 3] = rng.uniform(50, 100)
                elif grade == 2:
                    self.x[idx, 0] = rng.uniform(30, 60)
                    self.x[idx, 1] = rng.uniform(300, 450)
                    self.x[idx, 2] = rng.uniform(80, 150)
                    self.x[idx, 3] = rng.uniform(100, 200)
                elif grade == 3:
                    self.x[idx, 0] = rng.uniform(25, 50)
                    self.x[idx, 1] = rng.uniform(350, 500)
                    self.x[idx, 2] = rng.uniform(150, 250)
                    self.x[idx, 3] = rng.uniform(200, 350)
                elif grade == 4:
                    self.x[idx, 0] = rng.uniform(15, 40)
                    self.x[idx, 1] = rng.uniform(400, 600)
                    self.x[idx, 2] = rng.uniform(250, 600)
                    self.x[idx, 3] = rng.uniform(350, 600)

                # Normalize
                self.x[idx, 0] /= 100.0  # stride → 0-1
                self.x[idx, 1] /= 600.0  # stance → 0-1
                self.x[idx, 2] /= 600.0  # symmetry → 0-1
                self.x[idx, 3] /= 600.0  # weight_asym → 0-1

    def __len__(self): return len(self.x)
    def __getitem__(self, i):
        return torch.tensor(self.x[i]), torch.tensor(self.y[i])


def train():
    ds_train = LamenessDataset(n_samples=150, split="train")
    ds_val   = LamenessDataset(n_samples=50, split="val")
    dl_train = DataLoader(ds_train, batch_size=BATCH_SIZE, shuffle=True)
    dl_val   = DataLoader(ds_val, batch_size=BATCH_SIZE)

    model = LamenessModel().to(DEVICE)
    opt   = AdamW(model.parameters(), lr=LR, weight_decay=1e-4)
    xent  = nn.CrossEntropyLoss()

    best_val = 1e9
    for epoch in range(EPOCHS):
        model.train()
        total_loss = 0
        for xb, yb in dl_train:
            xb, yb = xb.to(DEVICE), yb.to(DEVICE)
            opt.zero_grad()
            pred = model(xb)
            loss = xent(pred, yb)
            loss.backward()
            opt.step()
            total_loss += loss.item()

        model.eval()
        val_loss = 0
        correct = 0
        with torch.no_grad():
            for xb, yb in dl_val:
                xb, yb = xb.to(DEVICE), yb.to(DEVICE)
                pred = model(xb)
                val_loss += xent(pred, yb).item()
                correct += (pred.argmax(1) == yb).sum().item()
        acc = correct / len(ds_val)
        print(f"Epoch {epoch+1:2d}/{EPOCHS}  loss={total_loss/len(dl_train):.4f}  "
              f"val_loss={val_loss/len(dl_val):.4f}  val_acc={acc:.3f}")

        if val_loss < best_val:
            best_val = val_loss
            torch.save(model.state_dict(), MODEL_SAVE)
            print(f"  -> saved best model to {MODEL_SAVE}")

    print("Training complete.")


if __name__ == "__main__":
    train()
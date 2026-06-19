"""
PawSync Activity Classifier Training — 1D-CNN

Trains a 1D-CNN to classify pet activity from 2s IMU windows (50Hz, 6 axes).
9 activity classes: resting, walking, running, sleeping, scratching,
head_shaking, licking, eating, playing.

Input: (100, 6) — 2s @ 50Hz, 6 channels (accel XYZ + gyro XYZ)
Output: 9-class softmax
Exported as int8 TFLite (<40KB) for on-collar nRF52840 inference.
"""

import os
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW

SEQ_LEN     = 100   # 2s @ 50Hz
INPUT_DIM   = 6     # accel XYZ + gyro XYZ
NUM_CLASSES = 9
BATCH_SIZE  = 64
EPOCHS      = 30
LR          = 1e-3
DEVICE      = "cuda" if torch.cuda.is_available() else "cpu"
MODEL_SAVE  = os.path.join(os.path.dirname(__file__), "activity_cnn.pt")
TFLITE_EXPORT = os.path.join(os.path.dirname(__file__), "activity_int8.tflite")

ACTIVITY_NAMES = ["resting", "walking", "running", "sleeping",
                  "scratching", "head_shaking", "licking", "eating", "playing"]


class ActivityCNN(nn.Module):
    """1D-CNN for IMU activity classification."""

    def __init__(self):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv1d(INPUT_DIM, 32, kernel_size=5, padding=2),
            nn.ReLU(),
            nn.Conv1d(32, 32, kernel_size=5, padding=2),
            nn.ReLU(),
            nn.MaxPool1d(2),
            nn.Conv1d(32, 16, kernel_size=3, padding=1),
            nn.ReLU(),
        )
        self.classifier = nn.Sequential(
            nn.Linear(16 * (SEQ_LEN // 2), 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, NUM_CLASSES),
        )

    def forward(self, x):
        """x: (batch, 6, 100) → (batch, 9) logits."""
        x = self.features(x)
        x = x.view(x.size(0), -1)
        return self.classifier(x)


class ActivityDataset(Dataset):
    """Synthetic IMU data for each activity class."""

    def __init__(self, n_samples=500, split="train"):
        rng = np.random.default_rng(seed=42 if split == "train" else 7)
        self.x = np.zeros((n_samples * NUM_CLASSES, SEQ_LEN, INPUT_DIM), dtype=np.float32)
        self.y = np.zeros(n_samples * NUM_CLASSES, dtype=np.int64)

        for cls in range(NUM_CLASSES):
            for i in range(n_samples):
                idx = cls * n_samples + i
                self.y[idx] = cls
                t = np.arange(SEQ_LEN) / 50.0  # time in seconds

                if cls == 0:  # resting
                    self.x[idx, :, 0] = 0 + rng.normal(0, 0.01, SEQ_LEN)
                    self.x[idx, :, 2] = 1 + rng.normal(0, 0.01, SEQ_LEN)  # gravity
                elif cls == 1:  # walking — ~2Hz oscillation
                    freq = rng.uniform(1.5, 2.5)
                    self.x[idx, :, 0] = 0.3 * np.sin(2 * np.pi * freq * t)
                    self.x[idx, :, 2] = 1 + 0.2 * np.sin(2 * np.pi * freq * t)
                    self.x[idx, :, 3] = 0.5 * np.cos(2 * np.pi * freq * t)
                elif cls == 2:  # running — higher freq + amplitude
                    freq = rng.uniform(3, 4)
                    self.x[idx, :, 0] = 1.0 * np.sin(2 * np.pi * freq * t)
                    self.x[idx, :, 2] = 1 + 0.8 * np.sin(2 * np.pi * freq * t)
                    self.x[idx, :, 3] = 2.0 * np.cos(2 * np.pi * freq * t)
                elif cls == 3:  # sleeping
                    self.x[idx, :, 2] = 1 + rng.normal(0, 0.005, SEQ_LEN)
                elif cls == 4:  # scratching — 8-12Hz vertical vibration
                    freq = rng.uniform(8, 12)
                    self.x[idx, :, 2] = 1 + 0.5 * np.sin(2 * np.pi * freq * t)
                elif cls == 5:  # head_shaking — 3-6Hz yaw rotation
                    freq = rng.uniform(3, 6)
                    self.x[idx, :, 5] = 2.0 * np.sin(2 * np.pi * freq * t)
                elif cls == 6:  # licking — low-amplitude periodic
                    freq = rng.uniform(5, 7)
                    self.x[idx, :, 0] = 0.1 * np.sin(2 * np.pi * freq * t)
                    self.x[idx, :, 1] = 0.1 * np.cos(2 * np.pi * freq * t)
                elif cls == 7:  # eating — low-amplitude 4-6Hz
                    freq = rng.uniform(4, 6)
                    self.x[idx, :, 1] = 0.15 * np.sin(2 * np.pi * freq * t)
                    self.x[idx, :, 2] = 1 + 0.1 * np.sin(2 * np.pi * freq * t)
                elif cls == 8:  # playing — varied high-amplitude
                    self.x[idx, :, 0] = 1.5 * rng.normal(0, 1, SEQ_LEN)
                    self.x[idx, :, 1] = 1.5 * rng.normal(0, 1, SEQ_LEN)
                    self.x[idx, :, 2] = 1 + 1.0 * rng.normal(0, 1, SEQ_LEN)

                # Add noise
                self.x[idx] += rng.normal(0, 0.05, (SEQ_LEN, INPUT_DIM))

    def __len__(self): return len(self.x)
    def __getitem__(self, i):
        # Permute to (6, 100) for Conv1d
        return torch.tensor(self.x[i]).permute(1, 0), torch.tensor(self.y[i])


def train():
    ds_train = ActivityDataset(n_samples=400, split="train")
    ds_val   = ActivityDataset(n_samples=100, split="val")
    dl_train = DataLoader(ds_train, batch_size=BATCH_SIZE, shuffle=True)
    dl_val   = DataLoader(ds_val, batch_size=BATCH_SIZE)

    model = ActivityCNN().to(DEVICE)
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

        # Validation
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
    print(f"\nTo export TFLite:")
    print(f"  1. Load {MODEL_SAVE}")
    print(f"  2. ai_edge_torch.convert(model, sample_input)")
    print(f"  3. int8 quantization → {TFLITE_EXPORT}")
    print(f"  4. xxd -i → activity_model_data[] for nRF52840")


if __name__ == "__main__":
    train()
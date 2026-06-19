"""
PawSync Vocalization Classifier Training — Audio CNN

Classifies pet vocalizations (barks/whines/meows) into 6 categories:
  0=none, 1=pain, 2=anxiety, 3=alert, 4=play, 5=attention, 6=distress

Input: 16kHz mono × 2s → Mel-spectrogram (64 mel bins × 128 frames)
Architecture: Conv2D → ReLU → MaxPool → Conv2D → ReLU → Flatten → Dense
Exported as int8 TFLite (<50KB) for on-camera ESP32-S3 inference.
"""

import os
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW

MEL_BINS    = 64
MEL_FRAMES  = 128
NUM_CLASSES = 7   # 0-6
BATCH_SIZE  = 32
EPOCHS      = 40
LR          = 1e-3
DEVICE      = "cuda" if torch.cuda.is_available() else "cpu"
MODEL_SAVE  = os.path.join(os.path.dirname(__file__), "vocalization_cnn.pt")
TFLITE_EXPORT = os.path.join(os.path.dirname(__file__), "vocalization_int8.tflite")

VOCAL_NAMES = ["none", "pain", "anxiety", "alert", "play", "attention", "distress"]


class VocalizationCNN(nn.Module):
    """2D-CNN on Mel-spectrograms for vocalization classification."""

    def __init__(self):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv2d(1, 32, kernel_size=3, padding=1),
            nn.ReLU(),
            nn.MaxPool2d(2),  # (32, 32, 64)
            nn.Conv2d(32, 16, kernel_size=3, padding=1),
            nn.ReLU(),
            nn.MaxPool2d(2),  # (16, 16, 32)
        )
        self.classifier = nn.Sequential(
            nn.Linear(16 * (MEL_BINS // 4) * (MEL_FRAMES // 4), 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, NUM_CLASSES),
        )

    def forward(self, x):
        """x: (batch, 1, 64, 128) → (batch, 7) logits."""
        x = self.features(x)
        x = x.view(x.size(0), -1)
        return self.classifier(x)


class VocalizationDataset(Dataset):
    """Synthetic Mel-spectrograms for each vocalization class.

    In production, this would use real labeled audio data from:
      - Dog bark datasets (e.g., AudioSet, custom vet-labeled recordings)
      - Cat meow datasets
      - Pain/distress vocalizations (vet clinic recordings)
    """

    def __init__(self, n_samples=200, split="train"):
        rng = np.random.default_rng(seed=42 if split == "train" else 7)
        self.x = np.zeros((n_samples * NUM_CLASSES, 1, MEL_BINS, MEL_FRAMES),
                          dtype=np.float32)
        self.y = np.zeros(n_samples * NUM_CLASSES, dtype=np.int64)

        for cls in range(NUM_CLASSES):
            for i in range(n_samples):
                idx = cls * n_samples + i
                self.y[idx] = cls

                # Generate synthetic spectrogram patterns
                t = np.arange(MEL_FRAMES)
                freq_bins = np.arange(MEL_BINS)

                if cls == 0:  # none — silence
                    self.x[idx, 0] = rng.normal(0, 0.1, (MEL_BINS, MEL_FRAMES))
                elif cls == 1:  # pain — high-pitch, short burst
                    center_freq = rng.integers(40, 60)
                    duration = rng.integers(10, 30)
                    self.x[idx, 0, center_freq-3:center_freq+3, :duration] = \
                        rng.uniform(0.5, 1.0, (6, duration))
                    self.x[idx, 0] += rng.normal(0, 0.1, (MEL_BINS, MEL_FRAMES))
                elif cls == 2:  # anxiety — sustained mid-pitch, rising
                    center_freq = rng.integers(20, 35)
                    freq_rise = np.linspace(0, 10, MEL_FRAMES).astype(int)
                    for f in range(MEL_FRAMES):
                        cf = center_freq + freq_rise[f]
                        if 0 <= cf < MEL_BINS:
                            self.x[idx, 0, cf, f] = rng.uniform(0.4, 0.7)
                    self.x[idx, 0] += rng.normal(0, 0.1, (MEL_BINS, MEL_FRAMES))
                elif cls == 3:  # alert — sharp broadband burst
                    duration = rng.integers(5, 15)
                    self.x[idx, 0, :, :duration] = rng.uniform(0.3, 0.6,
                        (MEL_BINS, duration))
                    self.x[idx, 0] += rng.normal(0, 0.1, (MEL_BINS, MEL_FRAMES))
                elif cls == 4:  # play — variable pitch, mid-length
                    for f in range(MEL_FRAMES):
                        cf = rng.integers(15, 50)
                        self.x[idx, 0, cf, f] = rng.uniform(0.3, 0.6)
                    self.x[idx, 0] += rng.normal(0, 0.1, (MEL_BINS, MEL_FRAMES))
                elif cls == 5:  # attention — repetitive, low-mid
                    period = rng.integers(20, 40)
                    for f in range(0, MEL_FRAMES, period):
                        end = min(f + period // 2, MEL_FRAMES)
                        self.x[idx, 0, 10:25, f:end] = rng.uniform(0.4, 0.6,
                            (15, end - f))
                    self.x[idx, 0] += rng.normal(0, 0.1, (MEL_BINS, MEL_FRAMES))
                elif cls == 6:  # distress — high-pitch, sustained
                    center_freq = rng.integers(45, 62)
                    self.x[idx, 0, center_freq-2:center_freq+2, :] = \
                        rng.uniform(0.6, 0.9, (4, MEL_FRAMES))
                    self.x[idx, 0] += rng.normal(0, 0.1, (MEL_BINS, MEL_FRAMES))

    def __len__(self): return len(self.x)
    def __getitem__(self, i):
        return torch.tensor(self.x[i]), torch.tensor(self.y[i])


def train():
    ds_train = VocalizationDataset(n_samples=150, split="train")
    ds_val   = VocalizationDataset(n_samples=50, split="val")
    dl_train = DataLoader(ds_train, batch_size=BATCH_SIZE, shuffle=True)
    dl_val   = DataLoader(ds_val, batch_size=BATCH_SIZE)

    model = VocalizationCNN().to(DEVICE)
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
    print(f"\nTo export TFLite → {TFLITE_EXPORT} for ESP32-S3")


if __name__ == "__main__":
    train()
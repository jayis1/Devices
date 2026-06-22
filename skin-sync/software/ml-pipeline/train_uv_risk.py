"""
SkinSync UV Risk Model Training — Personal Erythema (Sunburn) Prediction

Trains a personal UV dose → erythema (burn) risk model using the ISO 17166
erythema effectiveness spectrum. Learns each user's personal Minimal Erythema
Dose (MED) from skin-type + observed burn/flush data.

Architecture: Temporal CNN (1D) that processes 24h of UV exposure history
(UVA/UVB dose per 5-min interval) + skin temperature + user metadata
(Fitzpatrick type, personal MED) → outputs:
  - Current burn risk (0-100)
  - Hours to burn at current UV index
  - Predicted MED fraction at end of day

Also computes cumulative annual UVB dose for skin cancer risk correlation.
"""

import os
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW

SEQ_LEN      = 288   # 24h at 5-min intervals
INPUT_DIM    = 5     # [uva_dose, uvb_dose, uv_index, skin_temp, med_fraction]
BATCH_SIZE   = 64
EPOCHS       = 60
LR           = 1e-3
DEVICE       = "cuda" if torch.cuda.is_available() else "cpu"
MODEL_SAVE   = os.path.join(os.path.dirname(__file__), "uv_risk_model.pt")


class UVRiskModel(nn.Module):
    """Temporal CNN for UV burn risk prediction.

    Processes 24h of UV exposure telemetry and predicts:
      1. Current burn risk (0-100)
      2. Hours to burn at current UV index
      3. End-of-day MED fraction
    """
    def __init__(self):
        super().__init__()
        # 1D Conv over time series
        self.conv1 = nn.Conv1d(INPUT_DIM, 64, kernel_size=7, padding=3)
        self.conv2 = nn.Conv1d(64, 128, kernel_size=5, padding=2)
        self.conv3 = nn.Conv1d(128, 256, kernel_size=3, padding=1)
        self.pool = nn.MaxPool1d(2)
        self.relu = nn.ReLU()
        self.dropout = nn.Dropout(0.2)

        # User metadata branch (fitz type, personal MED)
        self.meta_fc = nn.Sequential(
            nn.Linear(2, 32), nn.ReLU(), nn.Linear(32, 32), nn.ReLU()
        )

        # Combined head
        self.head = nn.Sequential(
            nn.Linear(256 + 32, 128), nn.ReLU(), nn.Dropout(0.2),
            nn.Linear(128, 64), nn.ReLU(),
            nn.Linear(64, 3)  # burn_risk, hours_to_burn, eod_med_frac
        )

    def forward(self, x, meta):
        # x: (B, SEQ_LEN, INPUT_DIM) → transpose to (B, INPUT_DIM, SEQ_LEN)
        x = x.transpose(1, 2)
        x = self.relu(self.conv1(x))
        x = self.pool(x)
        x = self.relu(self.conv2(x))
        x = self.pool(x)
        x = self.relu(self.conv3(x))
        x = self.pool(x)
        x = x.mean(dim=2)  # global average pool → (B, 256)

        m = self.meta_fc(meta)  # (B, 32)
        combined = torch.cat([x, m], dim=1)
        return self.head(combined)


class UVDataset(Dataset):
    """Synthetic UV exposure data for development.

    In production: 90 days of real UV patch telemetry per user,
    labeled with observed burn events (skin temp rise > 2°C).
    """
    def __init__(self, n_samples=500):
        np.random.seed(42)
        self.samples = []
        self.meta = []
        self.labels = []
        for _ in range(n_samples):
            # Random Fitzpatrick type (1-6)
            fitz = np.random.randint(1, 7)
            med = {1: 200, 2: 250, 3: 350, 4: 500, 5: 800, 6: 1200}[fitz]

            # Generate 24h UV exposure profile
            seq = np.zeros((SEQ_LEN, INPUT_DIM))
            # Morning: low UV, afternoon: peak, evening: low
            for t in range(SEQ_LEN):
                hour = t / 12.0  # 5-min intervals → hours
                if 6 < hour < 18:  # daytime
                    uv_idx = max(0, 10 * np.sin(np.pi * (hour - 6) / 12))
                    uvb = uv_idx * 2.5  # W/m²
                    uva = uv_idx * 22.5
                else:
                    uv_idx, uvb, uva = 0, 0, 0

                # Add randomness (clouds, shade, indoor)
                if np.random.random() < 0.3:
                    uv_idx *= 0.3; uvb *= 0.3; uva *= 0.3

                dt = 300  # 5 min in seconds
                seq[t, 0] = uva * dt / 10  # dose delta J/m² * 10
                seq[t, 1] = uvb * dt / 10
                seq[t, 2] = uv_idx
                seq[t, 3] = 32 + np.random.normal(0, 0.5)  # skin temp
                # Cumulative MED fraction
                eff_dose = (uvb * 1.0 + uva * 0.05) * dt
                if t > 0:
                    seq[t, 4] = seq[t-1, 4] + (eff_dose / med) * 100
                else:
                    seq[t, 4] = (eff_dose / med) * 100
                seq[t, 4] = min(seq[t, 4], 150)

            # Labels: burn risk, hours to burn, EOD MED fraction
            eod_med = seq[-1, 4]
            burn_risk = min(100, eod_med)
            if eod_med < 100:
                hours_to_burn = (100 - eod_med) / max(eod_med / 12, 1)  # rough
            else:
                hours_to_burn = 0

            self.samples.append(seq.astype(np.float32))
            self.meta.append(np.array([fitz, med], dtype=np.float32))
            self.labels.append([burn_risk, hours_to_burn, min(eod_med, 100)])

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        return (torch.tensor(self.samples[idx]),
                torch.tensor(self.meta[idx]),
                torch.tensor(self.labels[idx], dtype=torch.float32))


def train():
    dataset = UVDataset(n_samples=500)
    loader = DataLoader(dataset, batch_size=BATCH_SIZE, shuffle=True)

    model = UVRiskModel().to(DEVICE)
    optimizer = AdamW(model.parameters(), lr=LR)
    criterion = nn.MSELoss()

    for epoch in range(EPOCHS):
        model.train()
        total_loss = 0
        for x, meta, y in loader:
            x, meta, y = x.to(DEVICE), meta.to(DEVICE), y.to(DEVICE)
            optimizer.zero_grad()
            pred = model(x, meta)
            loss = criterion(pred, y)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{EPOCHS} — Loss: {total_loss/len(loader):.4f}")

    torch.save(model.state_dict(), MODEL_SAVE)
    print(f"✓ UV risk model saved to {MODEL_SAVE}")


if __name__ == "__main__":
    train()
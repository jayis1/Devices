"""
GreenPulse Watering Prediction Model Training — Per-Plant LSTM

Trains a per-plant LSTM that learns the drying curve of each individual
pot from 30 days of soil moisture + temp + humidity + light data.
Predicts hours-to-wilt and optimal watering time.

Input: 30-day sequence of (soil_moisture, temp, humidity, lux) at 15-min steps
       = 2880 time steps × 4 features
Output: hours_to_water (regression), water_risk (0-100)

Personalized per pot (soil volume, plant size, species, microclimate).
"""

import os
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW

SEQ_LEN   = 2880   # 30 days @ 15 min (4/hr × 24 × 30)
INPUT_DIM = 4      # soil_moisture, temp, humidity, lux
HIDDEN    = 64
BATCH     = 16
EPOCHS    = 40
LR        = 1e-3
DEVICE    = "cuda" if torch.cuda.is_available() else "cpu"
SAVE      = os.path.join(os.path.dirname(__file__), "watering_lstm.pt")


class WateringLSTM(nn.Module):
    def __init__(self):
        super().__init__()
        self.lstm = nn.LSTM(INPUT_DIM, HIDDEN, num_layers=2, batch_first=True,
                            dropout=0.2)
        self.head = nn.Sequential(
            nn.Linear(HIDDEN, 32),
            nn.ReLU(),
            nn.Dropout(0.2),
            nn.Linear(32, 2),  # hours_to_water, water_risk
        )

    def forward(self, x):
        out, _ = self.lstm(x)
        last = out[:, -1, :]
        return self.head(last)


class WateringDataset(Dataset):
    """Synthetic drying curves for development.

    In production: real 30-day telemetry per plant tag.
    Models soil moisture decline as function of:
      - Plant transpiration rate (species-dependent)
      - Ambient humidity (higher humidity = slower drying)
      - Temperature (higher temp = faster drying)
      - Light (more light = more transpiration = faster drying)
      - Pot volume (larger = slower drying)
    """
    def __init__(self, n=300):
        np.random.seed(42)
        self.samples = []
        self.labels = []
        for _ in range(n):
            # Random plant parameters
            pot_volume_L = np.random.uniform(1, 5)     # pot size
            species_rate = np.random.uniform(0.5, 3)   # transpiration rate
            start_moisture = np.random.uniform(50, 80)

            # Generate 30-day telemetry
            data = np.zeros((SEQ_LEN, INPUT_DIM))
            moisture = start_moisture
            for t in range(SEQ_LEN):
                temp = 22 + np.random.normal(0, 2)
                humidity = 45 + np.random.normal(0, 10)
                lux = np.random.uniform(100, 2000)
                # Drying rate: more temp/light = faster, more humidity = slower
                dry_rate = (species_rate * (temp / 22) * (lux / 1000) /
                           (humidity / 45) / pot_volume_L) * 0.5
                moisture = max(0, moisture - dry_rate)
                # Simulate watering events (reset to 80% occasionally)
                if moisture < np.random.uniform(20, 40):
                    moisture = 80
                data[t] = [moisture, temp, humidity, lux]

            # Label: hours until moisture hits 30% (species threshold)
            future_moist = data[-1, 0]
            if future_moist > 30:
                # Estimate hours based on last known drying rate
                last_dry = max(0.01, (data[-100, 0] - data[-1, 0]) / (100 * 0.25))
                hours = int((future_moist - 30) / last_dry)
            else:
                hours = 0

            risk = min(100, max(0, int(100 * (1 - hours / 168))))
            self.samples.append(data.astype(np.float32))
            self.labels.append([float(hours), float(risk)])

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        return torch.tensor(self.samples[idx]), torch.tensor(self.labels[idx])


def train():
    ds = WateringDataset(n=300)
    loader = DataLoader(ds, batch_size=BATCH, shuffle=True)
    model = WateringLSTM().to(DEVICE)
    opt = AdamW(model.parameters(), lr=LR)
    crit = nn.MSELoss()

    for ep in range(EPOCHS):
        model.train()
        total = 0
        for x, y in loader:
            x, y = x.to(DEVICE), y.to(DEVICE)
            opt.zero_grad()
            pred = model(x)
            loss = crit(pred, y)
            loss.backward()
            opt.step()
            total += loss.item()
        if (ep + 1) % 10 == 0:
            print(f"Epoch {ep+1}/{EPOCHS} — Loss: {total/len(loader):.4f}")

    torch.save(model.state_dict(), SAVE)
    print(f"✓ Model saved to {SAVE}")


if __name__ == "__main__":
    train()
"""
TrailSync — Altitude Sickness & Storm Detection Models

Two models:
1. Altitude Sickness Detector: Monitors SpO2, HRV, ascent rate to detect
   AMS, HACE, HAPE risk. Uses Lake Louise Scoring criteria correlated with
   physiological sensor data.

2. Storm Predictor: Barometric pressure trend analysis for 2-3 hour
   storm prediction. Detects rapid pressure drops (>4 hPa/3hr) and
   combines with temperature/humidity trends.

SPDX-License-Identifier: MIT
"""
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report
import matplotlib.pyplot as plt


# ═══════════════════════════════════════════════════════════════════════
# MODEL 1: Altitude Sickness Detector
# ═══════════════════════════════════════════════════════════════════════

class AltitudeSicknessDataset(Dataset):
    """Input: [SpO2, HRV_rmssd, ascent_rate_m_hr, altitude_m, skin_temp,
               HR, duration_hr] × 12 timesteps (1 per 30 min over 6 hours)
    Output: 3-class [safe, AMS, HACE/HAPE]"""
    def __init__(self, sequences, labels):
        self.sequences = torch.FloatTensor(sequences)
        self.labels = torch.LongTensor(labels)

    def __len__(self):
        return len(self.labels)

    def __getitem__(self, idx):
        return self.sequences[idx], self.labels[idx]


class AltitudeSicknessModel(nn.Module):
    def __init__(self, input_dim=7, hidden_dim=32, num_classes=3):
        super().__init__()
        self.lstm = nn.LSTM(input_dim, hidden_dim, num_layers=2,
                           batch_first=True, bidirectional=True, dropout=0.2)
        self.head = nn.Sequential(
            nn.Linear(hidden_dim * 2, 32),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(32, num_classes),
        )

    def forward(self, x):
        lstm_out, _ = self.lstm(x)
        return self.head(lstm_out[:, -1, :])


def generate_altitude_data(n_samples=8000):
    """Generate synthetic altitude sickness data based on Lake Louise criteria."""
    np.random.seed(42)
    sequences = np.zeros((n_samples, 12, 7), dtype=np.float32)
    labels = np.zeros(n_samples, dtype=int)

    for i in range(n_samples):
        label = np.random.randint(0, 3)  # safe, AMS, HACE/HAPE
        labels[i] = label

        if label == 0:  # Safe: normal SpO2, good HRV, moderate ascent
            base_spo2 = np.random.uniform(95, 100)
            base_hrv = np.random.uniform(40, 80)
            ascent_rate = np.random.uniform(0, 300)
            altitude = np.random.uniform(0, 2000)
        elif label == 1:  # AMS: SpO2 88-94%, reduced HRV, fast ascent
            base_spo2 = np.random.uniform(88, 94)
            base_hrv = np.random.uniform(15, 40)
            ascent_rate = np.random.uniform(400, 800)
            altitude = np.random.uniform(2000, 4000)
        else:  # HACE/HAPE: SpO2 < 88%, very low HRV, very fast ascent
            base_spo2 = np.random.uniform(70, 88)
            base_hrv = np.random.uniform(5, 20)
            ascent_rate = np.random.uniform(600, 1200)
            altitude = np.random.uniform(3000, 5500)

        for t in range(12):
            spo2 = np.clip(base_spo2 - t * 0.3 + np.random.randn() * 1.5, 60, 100)
            hrv = np.clip(base_hrv - t * 0.5 + np.random.randn() * 3, 5, 100)
            sequences[i, t, 0] = spo2
            sequences[i, t, 1] = hrv
            sequences[i, t, 2] = ascent_rate + np.random.randn() * 50
            sequences[i, t, 3] = altitude + t * ascent_rate / 12
            sequences[i, t, 4] = 36.5 + np.random.randn() * 0.5  # skin temp
            sequences[i, t, 5] = 70 + (100 - spo2) * 1.5 + np.random.randn() * 5  # HR
            sequences[i, t, 6] = t * 0.5  # duration in hours

    return sequences, labels


def train_altitude_model():
    print("\n" + "=" * 50)
    print("Altitude Sickness Detector Training")
    print("=" * 50)

    sequences, labels = generate_altitude_data()
    X_train, X_val, y_train, y_val = train_test_split(sequences, labels, test_size=0.2, stratify=labels)

    train_ds = AltitudeSicknessDataset(X_train, y_train)
    val_ds = AltitudeSicknessDataset(X_val, y_val)
    train_loader = DataLoader(train_ds, batch_size=32, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=32)

    model = AltitudeSicknessModel().to("cuda" if torch.cuda.is_available() else "cpu")
    optimizer = torch.optim.AdamW(model.parameters(), lr=1e-3, weight_decay=1e-4)
    criterion = nn.CrossEntropyLoss()

    for epoch in range(20):
        model.train()
        for seqs, labs in train_loader:
            logits = model(seqs)
            loss = criterion(logits, labs)
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

        if (epoch + 1) % 5 == 0:
            model.eval()
            preds = []
            with torch.no_grad():
                for seqs, _ in val_loader:
                    logits = model(seqs)
                    preds.extend(logits.argmax(dim=-1).numpy())
            from sklearn.metrics import accuracy_score
            acc = accuracy_score(y_val, preds)
            print(f"Epoch {epoch+1}/20 — Val accuracy: {acc:.3f}")

    torch.save(model.state_dict(), "altitude_sickness_model.pt")
    print("Altitude sickness model saved.")
    return model


# ═══════════════════════════════════════════════════════════════════════
# MODEL 2: Storm Predictor
# ═══════════════════════════════════════════════════════════════════════

class StormDataset(Dataset):
    """Input: [pressure_hPa, temp_C, humidity_%] × 36 timesteps (5-min intervals over 3 hours)
    Output: 3-class [no_storm, storm_watch, storm_warning]"""
    def __init__(self, sequences, labels):
        self.sequences = torch.FloatTensor(sequences)
        self.labels = torch.LongTensor(labels)

    def __len__(self):
        return len(self.labels)

    def __getitem__(self, idx):
        return self.sequences[idx], self.labels[idx]


class StormPredictorModel(nn.Module):
    def __init__(self, input_dim=3, hidden_dim=32, num_classes=3):
        super().__init__()
        self.lstm = nn.LSTM(input_dim, hidden_dim, num_layers=2,
                           batch_first=True, bidirectional=True, dropout=0.2)
        self.head = nn.Sequential(
            nn.Linear(hidden_dim * 2, 32),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(32, num_classes),
        )

    def forward(self, x):
        lstm_out, _ = self.lstm(x)
        return self.head(lstm_out[:, -1, :])


def generate_storm_data(n_samples=8000):
    """Generate synthetic storm prediction data."""
    np.random.seed(42)
    sequences = np.zeros((n_samples, 36, 3), dtype=np.float32)
    labels = np.zeros(n_samples, dtype=int)

    for i in range(n_samples):
        label = np.random.randint(0, 3)  # no_storm, watch, warning
        labels[i] = label

        if label == 0:  # Stable weather
            base_pressure = np.random.uniform(1010, 1025)
            delta_pressure = np.random.uniform(-1, 1)  # small change
            base_temp = np.random.uniform(15, 30)
            base_humidity = np.random.uniform(30, 60)
        elif label == 1:  # Storm watch: moderate pressure drop
            base_pressure = np.random.uniform(1000, 1015)
            delta_pressure = np.random.uniform(-5, -2)  # 2-5 hPa drop
            base_temp = np.random.uniform(10, 25)
            base_humidity = np.random.uniform(60, 85)
        else:  # Storm warning: rapid pressure drop
            base_pressure = np.random.uniform(985, 1005)
            delta_pressure = np.random.uniform(-10, -4)  # >4 hPa drop
            base_temp = np.random.uniform(5, 20)
            base_humidity = np.random.uniform(80, 100)

        for t in range(36):
            pressure = base_pressure + delta_pressure * (t / 36) + np.random.randn() * 0.3
            temp = base_temp - 0.5 * (t / 36) + np.random.randn() * 0.5
            humidity = base_humidity + 5 * (t / 36) + np.random.randn() * 2
            sequences[i, t, 0] = pressure
            sequences[i, t, 1] = temp
            sequences[i, t, 2] = np.clip(humidity, 0, 100)

    return sequences, labels


def train_storm_model():
    print("\n" + "=" * 50)
    print("Storm Predictor Training")
    print("=" * 50)

    sequences, labels = generate_storm_data()
    X_train, X_val, y_train, y_val = train_test_split(sequences, labels, test_size=0.2, stratify=labels)

    train_ds = StormDataset(X_train, y_train)
    val_ds = StormDataset(X_val, y_val)
    train_loader = DataLoader(train_ds, batch_size=32, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=32)

    model = StormPredictorModel().to("cuda" if torch.cuda.is_available() else "cpu")
    optimizer = torch.optim.AdamW(model.parameters(), lr=1e-3, weight_decay=1e-4)
    criterion = nn.CrossEntropyLoss()

    for epoch in range(20):
        model.train()
        for seqs, labs in train_loader:
            logits = model(seqs)
            loss = criterion(logits, labs)
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

        if (epoch + 1) % 5 == 0:
            model.eval()
            preds = []
            with torch.no_grad():
                for seqs, _ in val_loader:
                    logits = model(seqs)
                    preds.extend(logits.argmax(dim=-1).numpy())
            from sklearn.metrics import accuracy_score
            acc = accuracy_score(y_val, preds)
            print(f"Epoch {epoch+1}/20 — Val accuracy: {acc:.3f}")

    torch.save(model.state_dict(), "storm_predictor_model.pt")
    print("Storm predictor model saved.")
    return model


# ═══════════════════════════════════════════════════════════════════════
# Combined Training
# ═══════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    print("TrailSync Altitude & Storm Model Training")
    print("=" * 50)

    altitude_model = train_altitude_model()
    storm_model = train_storm_model()

    print("\n✓ All models trained successfully!")
    print("  - altitude_sickness_model.pt")
    print("  - storm_predictor_model.pt")
    print("\nExport to TFLite Micro for on-device deployment on Wrist Unit (nRF52832)")
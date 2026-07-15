"""
LawnSync ML Pipeline — SoilForecast: 14-Day Soil Moisture LSTM

2-layer LSTM (64 hidden units) that predicts daily soil moisture for 14 days
based on 7-day history + 14-day weather forecast.

Input: 7 days history (moisture, temp, rain, ET₀) + 14-day weather forecast
Output: Daily soil moisture prediction for 14 days

Training: 2 years synthetic data (Hydrus-1D simulation) + real fine-tuning
"""

import os
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

MODEL_SAVE_DIR = "./models"
HISTORY_DAYS = 7
FORECAST_DAYS = 14
HIDDEN_SIZE = 64
NUM_LAYERS = 2
INPUT_FEATURES = 4  # moisture, temp, rain, ET₀
FORECAST_FEATURES = 4  # forecast temp, hum, rain_prob, rain_amt
BATCH_SIZE = 128
NUM_EPOCHS = 100
LEARNING_RATE = 0.001


# ---------------------------------------------------------------------------
# Model
# ---------------------------------------------------------------------------

class SoilForecastLSTM(nn.Module):
    """2-layer LSTM for 14-day soil moisture prediction.

    Input:
        history: (batch, 7, 4) — 7 days of [moisture, temp, rain, ET₀]
        forecast: (batch, 14, 4) — 14 days of [temp, hum, rain_prob, rain_amt]
    Output:
        prediction: (batch, 14) — daily soil moisture % for 14 days
    """

    def __init__(self, input_size=INPUT_FEATURES + FORECAST_FEATURES,
                 hidden_size=HIDDEN_SIZE, num_layers=NUM_LAYERS,
                 forecast_days=FORECAST_DAYS):
        super().__init__()
        self.lstm = nn.LSTM(input_size, hidden_size, num_layers,
                            batch_first=True, dropout=0.2)
        self.fc = nn.Sequential(
            nn.Linear(hidden_size, 64),
            nn.ReLU(),
            nn.Dropout(0.2),
            nn.Linear(64, forecast_days),
        )

    def forward(self, history, forecast):
        # Concatenate history (7 days) + forecast (14 days) along time axis
        # Repeat history features to match forecast feature dim
        history_expanded = torch.cat([
            history,
            torch.zeros(history.size(0), HISTORY_DAYS, FORECAST_FEATURES,
                        device=history.device)
        ], dim=2)
        x = torch.cat([history_expanded, forecast], dim=1)  # (batch, 21, 8)
        out, _ = self.lstm(x)
        # Use last hidden state for prediction
        out = out[:, -1, :]  # (batch, hidden)
        return self.fc(out)  # (batch, 14)


# ---------------------------------------------------------------------------
# Synthetic Data Generation (simplified Hydrus-1D)
# ---------------------------------------------------------------------------

class SoilForecastDataset(Dataset):
    """Synthetic soil moisture time-series dataset.

    In production: replace with real data from InfluxDB.
    """

    def __init__(self, n_samples: int = 10000, seed: int = 42):
        rng = np.random.RandomState(seed)
        self.samples = []

        for _ in range(n_samples):
            # Generate 21 days of data (7 history + 14 forecast)
            base_moisture = rng.uniform(15, 28)
            history = np.zeros((HISTORY_DAYS, INPUT_FEATURES))
            forecast = np.zeros((FORECAST_DAYS, FORECAST_FEATURES))
            target = np.zeros(FORECAST_DAYS)

            moisture = base_moisture
            for d in range(HISTORY_DAYS):
                et = rng.uniform(1, 4)
                rain = rng.exponential(3) if rng.random() < 0.2 else 0
                moisture -= et * 0.4
                moisture += rain * 0.3
                moisture = np.clip(moisture, 2, 45)
                history[d] = [moisture, rng.uniform(15, 30),
                             rain, et]

            for d in range(FORECAST_DAYS):
                et = rng.uniform(1, 4)
                rain_prob = rng.random()
                rain_amt = rng.exponential(3) if rain_prob > 0.7 else 0
                moisture -= et * 0.4
                moisture += rain_amt * 0.3
                moisture = np.clip(moisture, 2, 45)
                forecast[d] = [rng.uniform(15, 30), rng.uniform(0.3, 0.8),
                               rain_prob, rain_amt]
                target[d] = moisture

            self.samples.append((history, forecast, target))

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        history, forecast, target = self.samples[idx]
        return (torch.FloatTensor(history),
                torch.FloatTensor(forecast),
                torch.FloatTensor(target))


# ---------------------------------------------------------------------------
# Training
# ---------------------------------------------------------------------------

def train_model():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"[SoilForecast] Training LSTM on {device}")

    dataset = SoilForecastDataset(n_samples=10000)
    train_size = int(0.8 * len(dataset))
    val_size = len(dataset) - train_size
    train_ds, val_ds = torch.utils.data.random_split(dataset, [train_size, val_size])

    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE)

    model = SoilForecastLSTM().to(device)
    criterion = nn.MSELoss()
    optimizer = optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=1e-4)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=NUM_EPOCHS)

    best_val_rmse = float("inf")
    os.makedirs(MODEL_SAVE_DIR, exist_ok=True)

    for epoch in range(NUM_EPOCHS):
        model.train()
        train_loss = 0
        for history, forecast, target in train_loader:
            history = history.to(device)
            forecast = forecast.to(device)
            target = target.to(device)

            optimizer.zero_grad()
            pred = model(history, forecast)
            loss = criterion(pred, target)
            loss.backward()
            optimizer.step()
            train_loss += loss.item()

        scheduler.step()

        # Validate
        model.eval()
        val_loss = 0
        with torch.no_grad():
            for history, forecast, target in val_loader:
                history = history.to(device)
                forecast = forecast.to(device)
                target = target.to(device)
                pred = model(history, forecast)
                val_loss += criterion(pred, target).item()

        val_rmse = np.sqrt(val_loss / len(val_loader))
        train_loss /= len(train_loader)

        print(f"Epoch {epoch+1}/{NUM_EPOCHS} — "
              f"Train Loss: {train_loss:.4f}, Val RMSE: {val_rmse:.4f}")

        if val_rmse < best_val_rmse:
            best_val_rmse = val_rmse
            torch.save(model.state_dict(),
                       os.path.join(MODEL_SAVE_DIR, "soil_forecast_lstm.pth"))

    print(f"\n[SoilForecast] Best Val RMSE: {best_val_rmse:.4f}% VWC")

    # Export ONNX
    model.eval()
    h_dummy = torch.randn(1, HISTORY_DAYS, INPUT_FEATURES)
    f_dummy = torch.randn(1, FORECAST_DAYS, FORECAST_FEATURES)
    torch.onnx.export(model, (h_dummy, f_dummy),
                      os.path.join(MODEL_SAVE_DIR, "soil_forecast_lstm.onnx"),
                      input_names=["history", "forecast"],
                      output_names=["prediction"],
                      opset_version=13)

    return best_val_rmse


if __name__ == "__main__":
    train_model()
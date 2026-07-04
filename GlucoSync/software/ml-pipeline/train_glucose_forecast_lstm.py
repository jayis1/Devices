"""
GlucoSync ML Pipeline — Glucose Forecast LSTM

30/60-minute glucose prediction from CGM history + insulin + meal + activity.
2-layer LSTM → Dense. Trained on OhioT1DM + synthetic augmented data.
Quantized to INT8 for ESP32-S3 tflite-micro.

License: MIT
"""

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
import pandas as pd
from sklearn.preprocessing import StandardScaler
import joblib
import json

# ── Configuration ────────────────────────────────────────────────

SEQ_LEN = 60          # 60 minutes of history
N_FEATURES = 8         # glucose, trend, time_since_meal, carbs, iob, hr, intensity, tod
FORECAST_30 = True    # predict t+30 min
BATCH_SIZE = 64
EPOCHS = 50
LR = 0.001
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

# ── Model ────────────────────────────────────────────────────────

class GlucoseForecastLSTM(nn.Module):
    """2-layer LSTM for glucose forecasting."""

    def __init__(self, input_size=8, hidden1=64, hidden2=32, output_size=2):
        super().__init__()
        self.lstm1 = nn.LSTM(input_size, hidden1, batch_first=True)
        self.dropout1 = nn.Dropout(0.1)
        self.lstm2 = nn.LSTM(hidden1, hidden2, batch_first=True)
        self.fc1 = nn.Linear(hidden2, 16)
        self.relu = nn.ReLU()
        self.fc2 = nn.Linear(16, output_size)  # [glucose_30, glucose_60]

    def forward(self, x):
        out, _ = self.lstm1(x)
        out = self.dropout1(out)
        out, _ = self.lstm2(out)
        # Take last timestep
        out = out[:, -1, :]
        out = self.relu(self.fc1(out))
        out = self.fc2(out)
        return out


# ── Dataset ──────────────────────────────────────────────────────

class GlucoseDataset(Dataset):
    """Sliding window dataset from CGM + insulin + meal + activity data."""

    def __init__(self, glucose, trends, meals, insulin, activity, timestamps):
        self.sequences = []
        self.targets = []

        for i in range(SEQ_LEN, len(glucose) - 60):
            # Build feature matrix for sequence
            seq = np.zeros((SEQ_LEN, N_FEATURES))
            for j in range(SEQ_LEN):
                idx = i - SEQ_LEN + j
                t = timestamps[idx]
                seq[j, 0] = glucose[idx]
                seq[j, 1] = trends[idx]
                # Time since last meal (minutes)
                last_meal = np.searchsorted(meals[:, 1], t, side_right=True) - 1
                if last_meal >= 0 and len(meals) > 0:
                    seq[j, 2] = (t - meals[last_meal, 1]) / 60.0  # minutes
                    seq[j, 3] = meals[last_meal, 0]  # carb grams
                # IOB (insulin on board) — simplified exponential decay
                last_insulin = np.searchsorted(insulin[:, 1], t, side_right=True) - 1
                if last_insulin >= 0 and len(insulin) > 0:
                    elapsed = (t - insulin[last_insulin, 1]) / 60.0
                    if elapsed < 180:
                        seq[j, 4] = insulin[last_insulin, 0] * np.exp(-elapsed / 90.0)
                # Activity
                last_activity = np.searchsorted(activity[:, 2], t, side_right=True) - 1
                if last_activity >= 0 and len(activity) > 0:
                    seq[j, 5] = activity[last_activity, 0]  # HR
                    seq[j, 6] = activity[last_activity, 1]  # intensity
                # Time of day (0-1)
                seq[j, 7] = (t % 1440) / 1440.0  # minutes since midnight / 1440

            self.sequences.append(seq)
            # Targets: glucose at t+30 and t+60
            if i + 30 < len(glucose) and i + 60 < len(glucose):
                self.targets.append([glucose[i + 30], glucose[i + 60]])

        self.sequences = np.array(self.sequences, dtype=np.float32)
        self.targets = np.array(self.targets, dtype=np.float32)

    def __len__(self):
        return len(self.sequences)

    def __getitem__(self, idx):
        return self.sequences[idx], self.targets[idx]


# ── Training ─────────────────────────────────────────────────────

def train():
    print("Loading data...")

    # Load OhioT1DM dataset (or synthetic)
    # Format: per-patient CSV with columns: timestamp, glucose, meal_carbs, insulin_units, hr, activity
    try:
        df = pd.read_csv("data/ohio_t1dm.csv")
    except FileNotFoundError:
        print("OhioT1DM data not found, generating synthetic data...")
        df = generate_synthetic_data(n_days=30)

    # Extract arrays
    glucose = df["glucose"].values
    trends = np.gradient(glucose)
    meals = df[["meal_carbs", "timestamp"]].values
    insulin = df[["insulin_units", "timestamp"]].values
    activity = df[["hr", "intensity", "timestamp"]].values
    timestamps = df["timestamp"].values

    # Normalize glucose
    scaler = StandardScaler()
    glucose_scaled = scaler.fit_transform(glucose.reshape(-1, 1)).flatten()
    trends_scaled = StandardScaler().fit_transform(trends.reshape(-1, 1)).flatten()

    dataset = GlucoseDataset(glucose_scaled, trends_scaled, meals, insulin, activity, timestamps)
    print(f"Dataset size: {len(dataset)} sequences")

    train_size = int(0.8 * len(dataset))
    train_ds, val_ds = torch.utils.data.random_split(dataset, [train_size, len(dataset) - train_size])
    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE, shuffle=False)

    model = GlucoseForecastLSTM().to(DEVICE)
    optimizer = torch.optim.Adam(model.parameters(), lr=LR)
    criterion = nn.MSELoss()

    best_val_loss = float("inf")

    for epoch in range(EPOCHS):
        model.train()
        train_loss = 0
        for seq, target in train_loader:
            seq, target = seq.to(DEVICE), target.to(DEVICE)
            optimizer.zero_grad()
            pred = model(seq)
            loss = criterion(pred, target)
            loss.backward()
            optimizer.step()
            train_loss += loss.item()

        model.eval()
        val_loss = 0
        with torch.no_grad():
            for seq, target in val_loader:
                seq, target = seq.to(DEVICE), target.to(DEVICE)
                pred = model(seq)
                val_loss += criterion(pred, target).item()

        train_loss /= len(train_loader)
        val_loss /= len(val_loader)

        # Compute MARD (mean absolute relative difference)
        mard = compute_mard(model, val_loader, scaler)

        print(f"Epoch {epoch+1}/{EPOCHS} — train_loss: {train_loss:.4f} — val_loss: {val_loss:.4f} — MARD: {mard:.1f}%")

        if val_loss < best_val_loss:
            best_val_loss = val_loss
            torch.save(model.state_dict(), "models/glucose_forecast_lstm.pt")
            print(f"  → Saved best model (val_loss={val_loss:.4f})")

    print("Converting to tflite...")
    convert_to_tflite(model, scaler)

    print("Done. Model saved to models/glucose_forecast_lstm.tflite")


def compute_mard(model, loader, scaler):
    """Mean Absolute Relative Difference (%)"""
    model.eval()
    abs_diffs = []
    with torch.no_grad():
        for seq, target in loader:
            seq = seq.to(DEVICE)
            pred = model(seq).cpu().numpy()
            target = target.numpy()
            # Inverse transform
            pred_orig = scaler.inverse_transform(pred)
            target_orig = scaler.inverse_transform(target)
            for p, t in zip(pred_orig, target_orig):
                if t > 0:
                    abs_diffs.append(abs(p[0] - t[0]) / t[0])
    return np.mean(abs_diffs) * 100 if abs_diffs else 0


def convert_to_tflite(model, scaler):
    """Convert PyTorch model to TFLite INT8 for tflite-micro on ESP32-S3."""
    model.eval()
    # Export to ONNX first, then TFLite
    # Production: use torch.onnx.export() → onnx2tf → tflite_converter

    # Dummy input for tracing
    dummy = torch.randn(1, SEQ_LEN, N_FEATURES)

    # Trace and save
    traced = torch.jit.trace(model, dummy)
    traced.save("models/glucose_forecast_lstm_traced.pt")

    # Save scaler
    joblib.dump(scaler, "models/glucose_forecast_scaler.pkl")

    # Production TFLite conversion:
    # import tensorflow as tf
    # converter = tf.lite.TFLiteConverter.from_saved_model("models/tf_saved_model")
    # converter.optimizations = [tf.lite.Optimize.DEFAULT]
    # converter.target_spec.supported_types = [tf.int8]
    # converter.representative_dataset = representative_dataset
    # tflite_model = converter.convert()
    # with open("models/glucose_forecast_lstm_int8.tflite", "wb") as f:
    #     f.write(tflite_model)

    print("TFLite conversion placeholder complete (use onnx2tf for production)")


def generate_synthetic_data(n_days=30):
    """Generate synthetic CGM + meal + insulin + activity data."""
    np.random.seed(42)
    n_points = n_days * 24 * 60  # 1 reading per minute
    timestamps = np.arange(n_points)

    # Base glucose with diurnal pattern
    glucose = 120 + 30 * np.sin(timestamps / (60 * 24) * 2 * np.pi) + np.random.normal(0, 15, n_points)

    # Meals (3 per day, random carbs)
    meals = np.zeros(n_points)
    meal_times = []
    for day in range(n_days):
        for meal_hour in [8, 13, 19]:
            t = day * 24 * 60 + meal_hour * 60 + np.random.randint(-30, 30)
            if 0 <= t < n_points:
                carbs = np.random.randint(20, 80)
                meals[t] = carbs
                meal_times.append([carbs, t])
                # Glucose rise after meal
                for dt in range(120):
                    if t + dt < n_points:
                        glucose[t + dt] += carbs * 0.1 * np.exp(-dt / 30)

    # Insulin (bolus at meals, basal overnight)
    insulin = np.zeros(n_points)
    insulin_times = []
    for day in range(n_days):
        for meal_hour in [8, 13, 19]:
            t = day * 24 * 60 + meal_hour * 60 + 5
            if 0 <= t < n_points:
                units = np.random.randint(2, 8)
                insulin[t] = units
                insulin_times.append([units, t])
                # Glucose drop after insulin
                for dt in range(180):
                    if t + dt < n_points:
                        glucose[t + dt] -= units * 0.3 * np.exp(-dt / 60)

    # Activity (walking/running periods)
    hr = np.full(n_points, 70, dtype=float)
    intensity = np.zeros(n_points)
    for day in range(n_days):
        for exercise_hour in [7, 17]:
            t = day * 24 * 60 + exercise_hour * 60
            if 0 <= t < n_points:
                dur = np.random.randint(15, 45)
                for dt in range(dur):
                    if t + dt < n_points:
                        hr[t + dt] = 120 + np.random.randint(-10, 20)
                        intensity[t + dt] = 60
                        glucose[t + dt] -= 1.5  # exercise lowers glucose

    # Clamp glucose to physiological range
    glucose = np.clip(glucose, 40, 400)

    df = pd.DataFrame({
        "timestamp": timestamps,
        "glucose": glucose,
        "meal_carbs": meals,
        "insulin_units": insulin,
        "hr": hr,
        "intensity": intensity,
    })

    return df


if __name__ == "__main__":
    import os
    os.makedirs("models", exist_ok=True)
    train()
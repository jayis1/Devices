"""
JointSync ML Pipeline — 7-Day Flare Prediction LSTM

Trains an LSTM model to predict arthritis flare probability 7 days ahead
based on joint ROM, bilateral temperature delta, HRV, and activity data.

License: MIT
"""

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import Adam
import json
import os
from datetime import datetime, timedelta
from sklearn.metrics import roc_auc_score, precision_recall_curve
import matplotlib.pyplot as plt

# ─────────────────────────────────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────────────────────────────────

LOOKBACK_DAYS = 7
FORECAST_DAYS = 7
NUM_FEATURES = 10
BATCH_SIZE = 64
EPOCHS = 100
LR = 1e-3
WEIGHT_DECAY = 1e-4
PATIENCE = 10

FEATURE_NAMES = [
    "mean_rom",            # Mean range of motion (degrees)
    "rom_decline_rate",    # ROM derivative (degrees/day)
    "mean_temp_delta",     # Mean bilateral temp delta (°C)
    "temp_delta_trend",    # Temp delta derivative
    "mean_hrv",            # Mean HRV RMSSD (ms)
    "hrv_decline_rate",    # HRV derivative
    "activity_intensity",  # Mean activity (0-1)
    "therapy_adherence",   # Compression therapy adherence (0-1)
    "morning_stiffness",   # Morning stiffness duration (minutes)
    "pain_proxy",          # HRV × temp composite
]

# ─────────────────────────────────────────────────────────────────────
# Model
# ─────────────────────────────────────────────────────────────────────

class FlarePredictionLSTM(nn.Module):
    """LSTM for 7-day flare prediction."""

    def __init__(self, input_size=NUM_FEATURES, hidden_size=64,
                 num_layers=2, output_size=1):
        super().__init__()
        self.lstm1 = nn.LSTM(input_size, hidden_size, num_layers=1, batch_first=True)
        self.dropout1 = nn.Dropout(0.2)
        self.lstm2 = nn.LSTM(hidden_size, 32, num_layers=1, batch_first=True)
        self.dropout2 = nn.Dropout(0.2)
        self.fc1 = nn.Linear(32, 16)
        self.relu = nn.ReLU()
        self.fc2 = nn.Linear(16, output_size)
        self.sigmoid = nn.Sigmoid()

    def forward(self, x):
        # x: (batch, seq_len, input_size)
        out, (h1, c1) = self.lstm1(x)
        out = self.dropout1(out)
        out, (h2, c2) = self.lstm2(out)
        out = self.dropout2(out)
        # Use last time step
        out = out[:, -1, :]
        out = self.fc1(out)
        out = self.relu(out)
        out = self.fc2(out)
        out = self.sigmoid(out)
        return out.squeeze(-1)


# ─────────────────────────────────────────────────────────────────────
# Dataset
# ─────────────────────────────────────────────────────────────────────

class FlareDataset(Dataset):
    """Dataset of 7-day lookback windows with 7-day ahead flare labels."""

    def __init__(self, features, labels, lookback=LOOKBACK_DAYS):
        """
        features: (N, T, F) array — N patients, T days, F features
        labels: (N,) array — 0/1 flare in next 7 days
        """
        self.features = torch.FloatTensor(features)
        self.labels = torch.FloatTensor(labels)

    def __len__(self):
        return len(self.features)

    def __getitem__(self, idx):
        return self.features[idx], self.labels[idx]


# ─────────────────────────────────────────────────────────────────────
# Focal Loss (for class imbalance)
# ─────────────────────────────────────────────────────────────────────

class FocalLoss(nn.Module):
    def __init__(self, alpha=0.25, gamma=2.0):
        super().__init__()
        self.alpha = alpha
        self.gamma = gamma

    def forward(self, pred, target):
        bce = nn.functional.binary_cross_entropy(pred, target, reduction='none')
        p_t = target * pred + (1 - target) * (1 - pred)
        alpha_t = target * self.alpha + (1 - target) * (1 - self.alpha)
        focal_weight = alpha_t * (1 - p_t) ** self.gamma
        return (focal_weight * bce).mean()


# ─────────────────────────────────────────────────────────────────────
# Training
# ─────────────────────────────────────────────────────────────────────

def train_flare_lstm():
    """Train the flare prediction LSTM model."""
    print("=== JointSync Flare Prediction LSTM Training ===")

    # Load or generate data
    data_path = "data/flare_train.npz"
    if os.path.exists(data_path):
        print(f"Loading training data from {data_path}")
        data = np.load(data_path)
        X_train, y_train = data['X_train'], data['y_train']
        X_val, y_val = data['X_val'], data['y_val']
    else:
        print("Generating synthetic training data...")
        X_train, y_train, X_val, y_val = generate_synthetic_data(n_train=5000, n_val=1000)
        os.makedirs("data", exist_ok=True)
        np.savez(data_path, X_train=X_train, y_train=y_train,
                 X_val=X_val, y_val=y_val)

    print(f"Train: {X_train.shape}, Val: {X_val.shape}")
    print(f"Train flare rate: {y_train.mean():.2%}")
    print(f"Val flare rate: {y_val.mean():.2%}")

    # Create datasets
    train_ds = FlareDataset(X_train, y_train)
    val_ds = FlareDataset(X_val, y_val)
    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE)

    # Initialize model
    model = FlarePredictionLSTM()
    criterion = FocalLoss(alpha=0.3, gamma=2.0)
    optimizer = Adam(model.parameters(), lr=LR, weight_decay=WEIGHT_DECAY)

    # Training loop
    best_val_auc = 0
    patience_counter = 0
    history = {"train_loss": [], "val_loss": [], "val_auc": []}

    for epoch in range(EPOCHS):
        # Train
        model.train()
        train_loss = 0
        for X_batch, y_batch in train_loader:
            optimizer.zero_grad()
            pred = model(X_batch)
            loss = criterion(pred, y_batch)
            loss.backward()
            optimizer.step()
            train_loss += loss.item()
        train_loss /= len(train_loader)

        # Validate
        model.eval()
        val_loss = 0
        all_preds, all_labels = [], []
        with torch.no_grad():
            for X_batch, y_batch in val_loader:
                pred = model(X_batch)
                val_loss += criterion(pred, y_batch).item()
                all_preds.extend(pred.numpy())
                all_labels.extend(y_batch.numpy())

        val_loss /= len(val_loader)
        val_auc = roc_auc_score(all_labels, all_preds)

        history["train_loss"].append(train_loss)
        history["val_loss"].append(val_loss)
        history["val_auc"].append(val_auc)

        print(f"Epoch {epoch+1:3d}/{EPOCHS} — train_loss: {train_loss:.4f}, "
              f"val_loss: {val_loss:.4f}, val_auc: {val_auc:.4f}")

        # Early stopping
        if val_auc > best_val_auc:
            best_val_auc = val_auc
            patience_counter = 0
            os.makedirs("models", exist_ok=True)
            torch.save(model.state_dict(), "models/flare_lstm_best.pt")
            print(f"  → New best AUC: {val_auc:.4f}, model saved")
        else:
            patience_counter += 1
            if patience_counter >= PATIENCE:
                print(f"Early stopping at epoch {epoch+1}")
                break

    print(f"\nTraining complete. Best validation AUC: {best_val_auc:.4f}")

    # Save training history
    with open("models/flare_lstm_history.json", "w") as f:
        json.dump(history, f, indent=2)

    # Export to ONNX for serving
    model.load_state_dict(torch.load("models/flare_lstm_best.pt"))
    model.eval()
    dummy_input = torch.randn(1, LOOKBACK_DAYS, NUM_FEATURES)
    torch.onnx.export(model, dummy_input, "models/flare_lstm.onnx",
                      input_names=["features"], output_names=["flare_prob"],
                      dynamic_axes={"features": {0: "batch"}})
    print("Model exported to ONNX: models/flare_lstm.onnx")

    return model, history


def generate_synthetic_data(n_train=5000, n_val=1000):
    """Generate synthetic training data with realistic arthritis patterns."""
    np.random.seed(42)

    def gen_sample(flare_in_7days: bool):
        """Generate a single 7-day feature window."""
        days = LOOKBACK_DAYS
        features = np.zeros((days, NUM_FEATURES))

        # Base ROM (normal: 90-130 degrees for knee)
        base_rom = np.random.uniform(90, 130)

        for d in range(days):
            # ROM: declining if flare approaching
            decline = np.random.uniform(0, 3) if flare_in_7days else np.random.uniform(-1, 1)
            base_rom -= decline
            base_rom = max(30, base_rom)
            features[d, 0] = base_rom + np.random.normal(0, 2)

            # ROM decline rate
            features[d, 1] = decline + np.random.normal(0, 0.5)

            # Bilateral temp delta (normal: <0.5°C, flare: >2.0°C)
            if flare_in_7days:
                features[d, 2] = np.random.uniform(0.5, 3.0) * (d / days)
            else:
                features[d, 2] = np.random.uniform(0, 0.8)

            features[d, 3] = features[d, 2] - features[d-1, 2] if d > 0 else 0

            # HRV (normal: 30-60 ms, declining before flare)
            base_hrv = np.random.uniform(30, 60)
            if flare_in_7days:
                base_hrv -= np.random.uniform(0, 2) * d
                base_hrv = max(10, base_hrv)
            features[d, 4] = base_hrv

            features[d, 5] = -np.random.uniform(0, 2) if flare_in_7days else np.random.uniform(-1, 1)

            # Activity (0-1)
            features[d, 6] = np.random.uniform(0.2, 0.8) * (1 - 0.3 * flare_in_7days)

            # Therapy adherence (0-1)
            features[d, 7] = np.random.uniform(0.5, 0.9)

            # Morning stiffness (minutes)
            if flare_in_7days:
                features[d, 8] = np.random.uniform(15, 60) * (d / days)
            else:
                features[d, 8] = np.random.uniform(0, 15)

            # Pain proxy (HRV × temp)
            features[d, 9] = features[d, 4] * features[d, 2] / 100

        return features

    # Generate training data with 20% flare rate (class imbalance)
    X_train, y_train = [], []
    for _ in range(n_train):
        flare = np.random.random() < 0.2
        X_train.append(gen_sample(flare))
        y_train.append(1 if flare else 0)

    X_val, y_val = [], []
    for _ in range(n_val):
        flare = np.random.random() < 0.2
        X_val.append(gen_sample(flare))
        y_val.append(1 if flare else 0)

    return np.array(X_train), np.array(y_train), np.array(X_val), np.array(y_val)


def evaluate_model(model, X_test, y_test):
    """Evaluate trained model and print metrics."""
    model.eval()
    with torch.no_grad():
        preds = model(torch.FloatTensor(X_test)).numpy()

    auc = roc_auc_score(y_test, preds)
    print(f"Test AUC: {auc:.4f}")

    # Precision-Recall curve
    precision, recall, thresholds = precision_recall_curve(y_test, preds)
    f1 = 2 * precision * recall / (precision + recall + 1e-8)
    best_idx = np.argmax(f1)
    print(f"Best F1: {f1[best_idx]:.4f} at threshold {thresholds[best_idx]:.4f}")
    print(f"  Precision: {precision[best_idx]:.4f}")
    print(f"  Recall: {recall[best_idx]:.4f}")

    return auc, preds


if __name__ == "__main__":
    model, history = train_flare_lstm()
    print("\nFlare LSTM training complete!")
    print("Model saved to: models/flare_lstm_best.pt")
    print("ONNX export: models/flare_lstm.onnx")
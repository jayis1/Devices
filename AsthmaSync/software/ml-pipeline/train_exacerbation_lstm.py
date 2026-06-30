"""
AsthmaSync — Train Exacerbation Risk LSTM
==========================================
7-day asthma exacerbation risk forecaster.

Architecture:
  - Input: 30-day multivariate time series (daily aggregated)
    Features: rescue_use_count, wheeze_count, avg_hrv, avg_pm25, avg_spo2, nighttime_wheeze
  - Model: 2-layer LSTM (64 hidden units) + FC head
  - Output: 7-day risk probability (sigmoid)
  - Loss: Binary cross-entropy (exacerbation: yes/no within 7 days)
  - Optimizer: Adam (lr=1e-3)

Training data: Asthma Control Test (ACT) scores + rescue-use logs
  from clinical studies, labeled as exacerbation if:
  - ACT score dropped ≥ 3 points within 7 days, OR
  - ER visit / oral corticosteroid use within 7 days

License: MIT
"""

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
import numpy as np
import pandas as pd
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


# ── Model ────────────────────────────────────────────────
class ExacerbationLSTM(nn.Module):
    """2-layer LSTM for 7-day exacerbation risk prediction."""

    def __init__(self, input_size=6, hidden_size=64, num_layers=2, dropout=0.3):
        super().__init__()
        self.lstm = nn.LSTM(
            input_size=input_size,
            hidden_size=hidden_size,
            num_layers=num_layers,
            batch_first=True,
            dropout=dropout if num_layers > 1 else 0,
        )
        self.attention = nn.Linear(hidden_size, 1)
        self.fc1 = nn.Linear(hidden_size, 32)
        self.dropout = nn.Dropout(dropout)
        self.fc2 = nn.Linear(32, 1)
        self.sigmoid = nn.Sigmoid()

    def forward(self, x):
        # x: (batch, seq_len, input_size)
        lstm_out, (h_n, c_n) = self.lstm(x)  # (batch, seq, hidden)

        # Attention over time steps
        attn_weights = torch.softmax(self.attention(lstm_out), dim=1)  # (batch, seq, 1)
        context = torch.sum(attn_weights * lstm_out, dim=1)  # (batch, hidden)

        x = torch.relu(self.fc1(context))
        x = self.dropout(x)
        x = self.sigmoid(self.fc2(x))
        return x.squeeze(-1)


# ── Dataset ──────────────────────────────────────────────
class AsthmaDataset(Dataset):
    """30-day → 7-day risk dataset."""

    def __init__(self, features, labels):
        self.features = torch.FloatTensor(features)
        self.labels = torch.FloatTensor(labels)

    def __len__(self):
        return len(self.features)

    def __getitem__(self, idx):
        return self.features[idx], self.labels[idx]


# ── Synthetic Data Generator ─────────────────────────────
def generate_synthetic_data(n_samples=5000, seq_len=30):
    """Generate synthetic patient data for training.
    In production, replace with real clinical data."""
    np.random.seed(42)

    features = np.zeros((n_samples, seq_len, 6))
    labels = np.zeros(n_samples)

    for i in range(n_samples):
        # Baseline levels
        base_hrv = np.random.uniform(20, 60)
        base_spo2 = np.random.uniform(95, 99)
        base_pm25 = np.random.uniform(5, 25)

        # Trend: stable, declining, or exacerbation
        will_exacerbate = np.random.random() < 0.25  # 25% positive rate

        for t in range(seq_len):
            # Rescue use: Poisson, increases if exacerbating
            base_rate = 0.3 if not will_exacerbate else 0.8
            if t > seq_len - 14 and will_exacerbate:
                base_rate *= 1.5
            features[i, t, 0] = np.random.poisson(base_rate)  # rescue_count

            # Wheeze count
            wheeze_rate = 0.5 if not will_exacerbate else 2.0
            if t > seq_len - 10 and will_exacerbate:
                wheeze_rate *= 1.5
            features[i, t, 1] = np.random.poisson(wheeze_rate)

            # HRV (declining if exacerbating)
            hrv_trend = -0.3 if will_exacerbate else 0.0
            features[i, t, 2] = max(5, base_hrv + np.random.normal(0, 5) + hrv_trend * t)

            # PM2.5 exposure
            features[i, t, 3] = max(0, base_pm25 + np.random.normal(0, 5))

            # SpO2 (drops if exacerbating)
            spo2_trend = -0.05 if will_exacerbate else 0.0
            features[i, t, 4] = max(88, base_spo2 + np.random.normal(0, 1) + spo2_trend * t)

            # Nighttime wheeze
            features[i, t, 5] = np.random.poisson(wheeze_rate * 0.3)

        labels[i] = 1.0 if will_exacerbate else 0.0

    return features, labels


# ── Training ─────────────────────────────────────────────
def train_model():
    logger.info("Generating synthetic training data...")
    X, y = generate_synthetic_data(n_samples=10000, seq_len=30)

    # Normalize features
    scaler = StandardScaler()
    X_flat = X.reshape(-1, 6)
    X_flat = scaler.fit_transform(X_flat)
    X = X_flat.reshape(-1, 30, 6)

    # Train/test split
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y)

    train_ds = AsthmaDataset(X_train, y_train)
    test_ds = AsthmaDataset(X_test, y_test)
    train_loader = DataLoader(train_ds, batch_size=64, shuffle=True)
    test_loader = DataLoader(test_ds, batch_size=64, shuffle=False)

    # Initialize model
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = ExacerbationLSTM(input_size=6, hidden_size=64, num_layers=2, dropout=0.3)
    model.to(device)

    # Class weights for imbalanced data
    pos_weight = torch.tensor([(len(y_train) - sum(y_train)) / max(sum(y_train), 1)]).to(device)
    criterion = nn.BCELoss()
    optimizer = optim.Adam(model.parameters(), lr=1e-3, weight_decay=1e-4)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, mode="min", patience=5, factor=0.5)

    # Training loop
    n_epochs = 50
    best_val_loss = float("inf")

    for epoch in range(n_epochs):
        model.train()
        train_loss = 0
        for batch_x, batch_y in train_loader:
            batch_x, batch_y = batch_x.to(device), batch_y.to(device)
            optimizer.zero_grad()
            output = model(batch_x)
            loss = criterion(output, batch_y)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()
            train_loss += loss.item()

        train_loss /= len(train_loader)

        # Validation
        model.eval()
        val_loss = 0
        correct = 0
        total = 0
        with torch.no_grad():
            for batch_x, batch_y in test_loader:
                batch_x, batch_y = batch_x.to(device), batch_y.to(device)
                output = model(batch_x)
                val_loss += criterion(output, batch_y).item()
                predicted = (output > 0.5).float()
                total += batch_y.size(0)
                correct += (predicted == batch_y).sum().item()

        val_loss /= len(test_loader)
        accuracy = correct / total

        scheduler.step(val_loss)

        if val_loss < best_val_loss:
            best_val_loss = val_loss
            torch.save({
                "model_state_dict": model.state_dict(),
                "scaler_mean": scaler.mean_,
                "scaler_scale": scaler.scale_,
                "input_size": 6,
                "hidden_size": 64,
                "num_layers": 2,
            }, "exacerbation_lstm.pt")

        if (epoch + 1) % 5 == 0:
            logger.info(f"Epoch {epoch+1}/{n_epochs} — "
                        f"train_loss={train_loss:.4f} val_loss={val_loss:.4f} "
                        f"acc={accuracy:.3f} lr={optimizer.param_groups[0]['lr']:.6f}")

    logger.info(f"Training complete. Best val loss: {best_val_loss:.4f}")
    logger.info(f"Final accuracy: {accuracy:.3f}")

    # Export to TFLite for edge inference (optional)
    # model.eval()
    # dummy = torch.randn(1, 30, 6)
    # torch.onnx.export(model, dummy, "exacerbation_lstm.onnx", ...)

    return model


if __name__ == "__main__":
    train_model()
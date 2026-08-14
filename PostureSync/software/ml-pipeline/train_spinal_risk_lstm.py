"""
SpinalRisk LSTM Training Script
90-day spinal health risk forecast from 30-day rolling posture metrics

Architecture: LSTM (128 units) → FC(64) → FC(1)
Input: 30-day daily posture metrics (12 features per day)
Output: 90-day risk score (0-100)
"""

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
import json
import os

# Config
INPUT_DAYS = 30  # 30-day rolling window
NUM_FEATURES = 12  # Per-day features
HORIZON_DAYS = 90  # Forecast horizon


class SpinalRiskLSTM(nn.Module):
    """LSTM for 90-day spinal health risk forecasting"""

    def __init__(self, input_size=NUM_FEATURES, hidden_size=128, num_layers=2):
        super().__init__()
        self.lstm = nn.LSTM(
            input_size=input_size,
            hidden_size=hidden_size,
            num_layers=num_layers,
            batch_first=True,
            dropout=0.2,
        )
        self.fc = nn.Sequential(
            nn.Linear(hidden_size, 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, 1),
            nn.Sigmoid(),  # Output 0-1 (multiply by 100 for score)
        )

    def forward(self, x):
        # x shape: (batch, seq_len, input_size)
        lstm_out, (hidden, cell) = self.lstm(x)
        # Use last hidden state
        out = self.fc(hidden[-1])
        return out.squeeze(-1) * 100.0  # Scale to 0-100


class SpinalRiskDataset(Dataset):
    """Dataset of 30-day posture metric sequences with 90-day risk labels"""

    def __init__(self, sequences: np.ndarray, labels: np.ndarray):
        self.sequences = torch.FloatTensor(sequences)
        self.labels = torch.FloatTensor(labels)

    def __len__(self):
        return len(self.sequences)

    def __getitem__(self, idx):
        return self.sequences[idx], self.labels[idx]


def generate_synthetic_data(n_samples=5000):
    """Generate synthetic posture metric sequences for training"""
    np.random.seed(42)

    sequences = []
    labels = []

    for _ in range(n_samples):
        # Generate 30 days of posture metrics
        seq = np.zeros((INPUT_DAYS, NUM_FEATURES))

        # Feature 0: avg posture score (0-100)
        baseline_score = np.random.uniform(50, 95)
        trend = np.random.choice([-1, 0, 1]) * np.random.uniform(0, 0.5)
        seq[:, 0] = np.clip(
            baseline_score + trend * np.arange(INPUT_DAYS) + np.random.normal(0, 5, INPUT_DAYS),
            0, 100
        )

        # Feature 1: % time in neutral posture
        seq[:, 1] = np.clip(seq[:, 0] / 100 * 80 + np.random.normal(0, 5, INPUT_DAYS), 0, 100)

        # Feature 2: % time in forward head
        seq[:, 2] = np.clip((100 - seq[:, 0]) * 0.3 + np.random.normal(0, 3, INPUT_DAYS), 0, 100)

        # Feature 3: % time slouching
        seq[:, 3] = np.clip((100 - seq[:, 0]) * 0.25 + np.random.normal(0, 3, INPUT_DAYS), 0, 100)

        # Feature 4: avg forward tilt angle (degrees)
        seq[:, 4] = np.clip(15 + (100 - seq[:, 0]) * 0.3 + np.random.normal(0, 2, INPUT_DAYS), 0, 60)

        # Feature 5: avg lateral tilt (degrees)
        seq[:, 5] = np.abs(np.random.normal(3, 2, INPUT_DAYS))

        # Feature 6: EMG asymmetry (%)
        seq[:, 6] = np.clip(10 + (100 - seq[:, 0]) * 0.2 + np.random.normal(0, 3, INPUT_DAYS), 0, 50)

        # Feature 7: sitting duration (hours)
        seq[:, 7] = np.clip(np.random.normal(8, 2, INPUT_DAYS), 0, 16)

        # Feature 8: movement frequency (per hour)
        seq[:, 8] = np.clip(np.random.normal(15, 5, INPUT_DAYS), 0, 60)

        # Feature 9: spine angle variability (std)
        seq[:, 9] = np.clip(np.random.normal(5, 2, INPUT_DAYS), 0, 20)

        # Feature 10: correction response rate (0-1)
        seq[:, 10] = np.clip(np.random.normal(0.6, 0.15, INPUT_DAYS), 0, 1)

        # Feature 11: HRV (ms) — stress indicator
        seq[:, 11] = np.clip(np.random.normal(40, 10, INPUT_DAYS), 10, 100)

        # Generate label: 90-day risk score
        # Higher risk = lower scores, more poor posture, more asymmetry, less movement
        avg_score = np.mean(seq[:, 0])
        poor_pct = np.mean(seq[:, 2] + seq[:, 3])
        asymmetry = np.mean(seq[:, 6])
        movement = np.mean(seq[:, 8])
        response = np.mean(seq[:, 10])

        risk = (100 - avg_score) * 0.4 + poor_pct * 0.2 + asymmetry * 0.2 + (60 - movement) * 0.1 + (1 - response) * 0.1
        risk = np.clip(risk + np.random.normal(0, 5), 0, 100)

        sequences.append(seq)
        labels.append(risk)

    return np.array(sequences), np.array(labels)


def train_model(epochs: int = 100, batch_size: int = 32, lr: float = 0.001,
                save_dir: str = "models"):
    """Train SpinalRisk LSTM"""

    # Generate data
    print("Generating synthetic training data...")
    sequences, labels = generate_synthetic_data(n_samples=5000)
    print(f"Data: {sequences.shape}, Labels: {labels.shape}")

    # Split
    X_train, X_val, y_train, y_val = train_test_split(
        sequences, labels, test_size=0.2, random_state=42
    )

    # Scale features
    scaler = StandardScaler()
    X_train_flat = X_train.reshape(-1, NUM_FEATURES)
    X_train_scaled = scaler.fit_transform(X_train_flat).reshape(X_train.shape)
    X_val_scaled = scaler.transform(X_val.reshape(-1, NUM_FEATURES)).reshape(X_val.shape)

    # Datasets
    train_ds = SpinalRiskDataset(X_train_scaled, y_train)
    val_ds = SpinalRiskDataset(X_val_scaled, y_val)
    train_loader = DataLoader(train_ds, batch_size=batch_size, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=batch_size, shuffle=False)

    # Model
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = SpinalRiskLSTM().to(device)
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=lr, weight_decay=1e-4)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=10, factor=0.5)

    # Training
    best_val_loss = float('inf')
    history = {"train_loss": [], "val_loss": []}

    for epoch in range(epochs):
        model.train()
        train_loss = 0
        for batch_x, batch_y in train_loader:
            batch_x, batch_y = batch_x.to(device), batch_y.to(device)
            optimizer.zero_grad()
            outputs = model(batch_x)
            loss = criterion(outputs, batch_y)
            loss.backward()
            optimizer.step()
            train_loss += loss.item()

        train_loss /= len(train_loader)

        model.eval()
        val_loss = 0
        with torch.no_grad():
            for batch_x, batch_y in val_loader:
                batch_x, batch_y = batch_x.to(device), batch_y.to(device)
                outputs = model(batch_x)
                loss = criterion(outputs, batch_y)
                val_loss += loss.item()

        val_loss /= len(val_loader)
        scheduler.step(val_loss)

        history["train_loss"].append(train_loss)
        history["val_loss"].append(val_loss)

        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{epochs} — train_loss: {train_loss:.4f}, val_loss: {val_loss:.4f}")

        if val_loss < best_val_loss:
            best_val_loss = val_loss
            os.makedirs(save_dir, exist_ok=True)
            torch.save(model.state_dict(), os.path.join(save_dir, "spinal_risk_lstm.pt"))
            # Save scaler
            np.save(os.path.join(save_dir, "spinal_risk_scaler.npy"), scaler)

    print(f"\nBest validation loss: {best_val_loss:.4f}")

    # Export to ONNX
    model.eval()
    dummy_input = torch.randn(1, INPUT_DAYS, NUM_FEATURES)
    torch.onnx.export(
        model, dummy_input,
        os.path.join(save_dir, "spinal_risk_lstm.onnx"),
        input_names=["posture_history"],
        output_names=["risk_score"],
        dynamic_axes={"posture_history": {0: "batch"}, "risk_score": {0: "batch"}},
    )
    print(f"Model exported to ONNX")

    with open(os.path.join(save_dir, "spinal_risk_history.json"), "w") as f:
        json.dump(history, f)

    return model, history


if __name__ == "__main__":
    train_model(epochs=100, batch_size=32, lr=0.001)
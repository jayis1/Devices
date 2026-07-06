"""
SightSync ML Pipeline — Myopia Progression Forecast (LSTM)
============================================================

Trains an LSTM to forecast 30/90-day myopia progression risk
from daily near-work dose, outdoor light exposure, age, and
baseline refractive error.

License: MIT
"""

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader, TensorDataset
import os

MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "firmware", "hub", "models")

# ── LSTM Model ───────────────────────────────────────────────────────

class MyopiaLSTM(nn.Module):
    def __init__(self, input_size=5, hidden_size=64, num_layers=2, output_size=2):
        super().__init__()
        self.lstm = nn.LSTM(input_size, hidden_size, num_layers,
                           batch_first=True, dropout=0.2)
        self.fc1 = nn.Linear(hidden_size, 32)
        self.relu = nn.ReLU()
        self.fc2 = nn.Linear(32, output_size)  # [risk_30day, risk_90day]
        self.sigmoid = nn.Sigmoid()

    def forward(self, x):
        lstm_out, _ = self.lstm(x)
        last_out = lstm_out[:, -1, :]
        x = self.fc1(last_out)
        x = self.relu(x)
        x = self.fc2(x)
        x = self.sigmoid(x)
        return x


# ── Synthetic Data Generation ────────────────────────────────────────

def generate_synthetic_data(n_children=1000, sequence_length=90):
    """Generate synthetic longitudinal data for training."""
    np.random.seed(42)

    features = []
    targets = []

    for i in range(n_children):
        age = np.random.randint(6, 16)
        baseline_refraction = np.random.uniform(-3.0, 0.5)  # diopters

        # 90-day daily sequence
        seq = []
        for day in range(sequence_length):
            near_work_min = np.random.uniform(60, 300)  # near-work minutes/day
            outdoor_min = np.random.uniform(0, 180)     # outdoor light minutes/day
            avg_distance_mm = np.random.uniform(200, 600)
            refraction_change = np.random.uniform(-0.02, 0.005)  # daily change

            seq.append([near_work_min, outdoor_min, avg_distance_mm,
                       age, baseline_refraction])

        features.append(seq)

        # Target: risk based on near-work and outdoor exposure
        total_near = np.sum([s[0] for s in seq])
        total_outdoor = np.sum([s[1] for s in seq])

        risk_30 = min(1.0, max(0.0,
            (total_near / (90 * 300)) * 0.6 +
            (1 - total_outdoor / (90 * 180)) * 0.4
        ))
        risk_90 = min(1.0, max(0.0,
            (total_near / (90 * 300)) * 0.7 +
            (1 - total_outdoor / (90 * 180)) * 0.5 +
            (age < 10) * 0.1
        ))

        targets.append([risk_30, risk_90])

    return np.array(features, dtype=np.float32), np.array(targets, dtype=np.float32)


# ── Training ─────────────────────────────────────────────────────────

def train():
    print("=== SightSync Myopia Progression LSTM Training ===")

    X, y = generate_synthetic_data(n_children=1000, sequence_length=90)

    # Normalize features
    X_mean = X.mean(axis=(0, 1))
    X_std = X.std(axis=(0, 1))
    X_norm = (X - X_mean) / (X_std + 1e-8)

    # Split
    split = int(0.8 * len(X))
    X_train, X_val = X_norm[:split], X_norm[split:]
    y_train, y_val = y[:split], y[split:]

    train_ds = TensorDataset(torch.tensor(X_train), torch.tensor(y_train))
    val_ds = TensorDataset(torch.tensor(X_val), torch.tensor(y_val))
    train_loader = DataLoader(train_ds, batch_size=32, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=32)

    model = MyopiaLSTM(input_size=5, hidden_size=64, num_layers=2, output_size=2)
    criterion = nn.BCELoss()
    optimizer = optim.Adam(model.parameters(), lr=0.001)

    epochs = 50
    best_val_loss = float('inf')

    for epoch in range(epochs):
        model.train()
        train_loss = 0
        for batch_x, batch_y in train_loader:
            optimizer.zero_grad()
            output = model(batch_x)
            loss = criterion(output, batch_y)
            loss.backward()
            optimizer.step()
            train_loss += loss.item()

        model.eval()
        val_loss = 0
        with torch.no_grad():
            for batch_x, batch_y in val_loader:
                output = model(batch_x)
                val_loss += criterion(output, batch_y).item()

        train_loss /= len(train_loader)
        val_loss /= len(val_loader)

        if val_loss < best_val_loss:
            best_val_loss = val_loss
            os.makedirs(MODEL_DIR, exist_ok=True)
            torch.save(model.state_dict(), os.path.join(MODEL_DIR, "myopia_lstm.pt"))

        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}: train_loss={train_loss:.4f} val_loss={val_loss:.4f}")

    print(f"Best val loss: {best_val_loss:.4f}")
    print(f"Model saved: {os.path.join(MODEL_DIR, 'myopia_lstm.pt')}")

    # Export to ONNX for cloud inference
    model.eval()
    dummy_input = torch.randn(1, 90, 5)
    onnx_path = os.path.join(MODEL_DIR, "myopia_lstm.onnx")
    torch.onnx.export(model, dummy_input, onnx_path, opset_version=11,
                     input_names=["input"], output_names=["output"],
                     dynamic_axes={"input": {0: "batch"}, "output": {0: "batch"}})
    print(f"ONNX model exported: {onnx_path}")

    return model


if __name__ == "__main__":
    train()
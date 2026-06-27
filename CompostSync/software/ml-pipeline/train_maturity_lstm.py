"""
Compost Maturity LSTM Training
Trains an LSTM to predict compost maturity score (0-100%) from 14-day sensor time series.

Input:  14 days × 8 channels (temp×3, moisture×3, co2, methane) at 15-min intervals = 1344 timesteps
Output: maturity_score (0-100), phase (6-class)

Architecture:
  LSTM(64, return_sequences=True) → LSTM(32) → Dense(32, relu) → Dense(8) →
    → Dense(1, sigmoid*100) [maturity] + Dense(6, softmax) [phase]
"""
import numpy as np
import pandas as pd
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import Adam
import joblib
import os
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Hyperparameters
SEQ_LEN = 1344      # 14 days × 96 readings/day (15-min intervals)
NUM_CHANNELS = 8     # temp1, temp2, temp3, moist1, moist2, moist3, co2, methane
BATCH_SIZE = 64
EPOCHS = 50
LR = 0.001
DEVICE = torch.device("cuda" if torch.cuda.is_available() else "cpu")


class CompostLSTM(nn.Module):
    def __init__(self, input_size=8, hidden_size=64, num_phases=6):
        super().__init__()
        self.lstm1 = nn.LSTM(input_size, hidden_size, batch_first=True)
        self.lstm2 = nn.LSTM(hidden_size, hidden_size // 2, batch_first=True)
        self.fc1 = nn.Linear(hidden_size // 2, 32)
        self.fc_maturity = nn.Linear(32, 1)
        self.fc_phase = nn.Linear(32, num_phases)
        self.relu = nn.ReLU()
        self.sigmoid = nn.Sigmoid()

    def forward(self, x):
        # x: (batch, seq_len, channels)
        out, _ = self.lstm1(x)
        out, (h_n, _) = self.lstm2(out)
        # Use last hidden state
        feat = self.relu(self.fc1(h_n.squeeze(0)))
        maturity = self.sigmoid(self.fc_maturity(feat)) * 100.0
        phase = self.fc_phase(feat)
        return maturity, phase


class CompostDataset(Dataset):
    def __init__(self, sequences, labels_maturity, labels_phase):
        self.sequences = sequences
        self.maturity = labels_maturity
        self.phase = labels_phase

    def __len__(self):
        return len(self.sequences)

    def __getitem__(self, idx):
        seq = torch.FloatTensor(self.sequences[idx])
        mat = torch.FloatTensor([self.maturity[idx]])
        phase = torch.LongTensor([self.phase[idx]])
        return seq, mat, phase.squeeze()


def load_synthetic_data(data_path="data/synthetic_compost_cycles.csv"):
    """Load synthetic compost simulation data."""
    logger.info(f"Loading data from {data_path}...")
    df = pd.read_csv(data_path)

    # Group by cycle_id and create sequences
    sequences = []
    maturity_labels = []
    phase_labels = []

    for cycle_id in df["cycle_id"].unique():
        cycle_data = df[df["cycle_id"] == cycle_id].sort_values("timestep")

        if len(cycle_data) < SEQ_LEN:
            # Pad with zeros
            padding = np.zeros((SEQ_LEN - len(cycle_data), NUM_CHANNELS))
            seq = np.vstack([padding, cycle_data[["t1", "t2", "t3", "m1", "m2", "m3", "co2", "ch4"]].values])
        else:
            # Take last SEQ_LEN readings
            seq = cycle_data[["t1", "t2", "t3", "m1", "m2", "m3", "co2", "ch4"]].values[-SEQ_LEN:]

        # Normalize
        seq = normalize_sequence(seq)

        sequences.append(seq)
        maturity_labels.append(cycle_data["maturity_score"].iloc[-1])
        phase_labels.append(int(cycle_data["phase"].iloc[-1]))

    return np.array(sequences), np.array(maturity_labels), np.array(phase_labels)


def normalize_sequence(seq):
    """Normalize sensor readings to 0-1 range."""
    # Temperature: -10 to 80°C → 0-1
    seq[:, 0:3] = (seq[:, 0:3] + 10) / 90
    # Moisture: 0-100 → 0-1
    seq[:, 3:6] = seq[:, 3:6] / 100
    # CO2: 0-10000 → 0-1 (log scale)
    seq[:, 6] = np.log1p(np.clip(seq[:, 6], 0, 10000)) / np.log(10001)
    # Methane: 0-5000 → 0-1
    seq[:, 7] = np.clip(seq[:, 7], 0, 5000) / 5000
    return np.clip(seq, 0, 1)


def train():
    """Train the maturity LSTM model."""
    X, y_mat, y_phase = load_synthetic_data()
    logger.info(f"Loaded {len(X)} sequences, shape: {X.shape}")

    # Split train/val
    split = int(0.8 * len(X))
    train_ds = CompostDataset(X[:split], y_mat[:split], y_phase[:split])
    val_ds = CompostDataset(X[split:], y_mat[split:], y_phase[split:])
    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE)

    model = CompostLSTM().to(DEVICE)
    optimizer = Adam(model.parameters(), lr=LR)
    mse_loss = nn.MSELoss()
    ce_loss = nn.CrossEntropyLoss()

    best_val_loss = float("inf")

    for epoch in range(EPOCHS):
        model.train()
        train_loss = 0
        for batch in train_loader:
            seq, mat, phase = [b.to(DEVICE) for b in batch]
            optimizer.zero_grad()
            pred_mat, pred_phase = model(seq)
            loss = mse_loss(pred_mat.squeeze(), mat.squeeze()) + 0.5 * ce_loss(pred_phase, phase)
            loss.backward()
            optimizer.step()
            train_loss += loss.item()

        # Validation
        model.eval()
        val_loss = 0
        with torch.no_grad():
            for batch in val_loader:
                seq, mat, phase = [b.to(DEVICE) for b in batch]
                pred_mat, pred_phase = model(seq)
                loss = mse_loss(pred_mat.squeeze(), mat.squeeze()) + 0.5 * ce_loss(pred_phase, phase)
                val_loss += loss.item()

        train_loss /= len(train_loader)
        val_loss /= len(val_loader)
        logger.info(f"Epoch {epoch+1}/{EPOCHS} — train_loss={train_loss:.4f} val_loss={val_loss:.4f}")

        if val_loss < best_val_loss:
            best_val_loss = val_loss
            torch.save(model.state_dict(), "models/maturity_lstm.pt")
            logger.info(f"  → Saved best model (val_loss={val_loss:.4f})")

    # Export to ONNX for deployment
    model.load_state_dict(torch.load("models/maturity_lstm.pt"))
    model.eval()
    dummy = torch.randn(1, SEQ_LEN, NUM_CHANNELS)
    torch.onnx.export(model, dummy, "models/maturity_lstm.onnx",
                      input_names=["sensor_sequence"],
                      output_names=["maturity", "phase"],
                      dynamic_axes={"sensor_sequence": {0: "batch"}})
    logger.info("Exported to ONNX: models/maturity_lstm.onnx")


if __name__ == "__main__":
    os.makedirs("models", exist_ok=True)
    train()
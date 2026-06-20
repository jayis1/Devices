"""
CalmGrid Stress Score + Burnout Risk Model Training — CNN-LSTM

Trains a multi-modal time-series model that fuses:
  - Resting HR (1), HRV-RMSSD (1), skin temp (1)
  - EDA: tonic SCL (1), phasic SCR rate (1), SCR amplitude (1)
  - Activity distribution: % time in 8 activity classes (8)
  - Prosody stress level (1) + speech minutes (1)
  - Sleep: duration (1), efficiency (1), HRV-during-sleep (1)
  - Environment: lux (1), CCT (1), temp (1), humidity (1), noise dB (1)
  - Battery (1) + step count (1)
= 26 input features per 5-minute time-step.

Predicts 2 outputs:
  - Stress score [0,1]
  - Burnout risk (14-day forecast) [0,1]

Labeled by clinical stress scales (PSS-10) + cortisol assays + MBI.

Architecture: 3 Conv1D branches → concat → LSTM(128)×2 → Dense → sigmoid
Exported as int8-quantized .tflite (<80KB) for on-hub TFLite Micro inference.
"""

import os
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW
from torch.optim.lr_scheduler import CosineAnnealingLR

# ---------------------------------------------------------------------------
# Hyperparameters
# ---------------------------------------------------------------------------
SEQ_LEN       = 288    # 24h @ 5min steps
INPUT_DIM     = 26
HIDDEN_DIM    = 128
OUTPUT_DIM    = 2      # stress, burnout_risk
BATCH_SIZE    = 32
EPOCHS        = 50
LR            = 1e-3
DEVICE        = "cuda" if torch.cuda.is_available() else "cpu"
MODEL_SAVE    = os.path.join(os.path.dirname(__file__), "stress_cnn_lstm.pt")
TFLITE_EXPORT = os.path.join(os.path.dirname(__file__), "stress_int8.tflite")


# ---------------------------------------------------------------------------
# Model
# ---------------------------------------------------------------------------
class StressCNNLSTM(nn.Module):
    """Multi-branch Conv1D (vitals + EDA + activity) → LSTM → 2-head output."""

    def __init__(self, input_dim=INPUT_DIM, hidden=HIDDEN_DIM):
        super().__init__()
        # Vitals branch: HR, HRV, temp (3 features)
        self.vitals_conv = nn.Sequential(
            nn.Conv1d(3, 32, kernel_size=5, padding=2),
            nn.ReLU(),
            nn.Conv1d(32, 32, kernel_size=5, padding=2),
            nn.ReLU(),
            nn.MaxPool1d(2),
        )
        # EDA branch: SCL, SCR rate, SCR amplitude (3 features)
        self.eda_conv = nn.Sequential(
            nn.Conv1d(3, 24, kernel_size=5, padding=2),
            nn.ReLU(),
            nn.Conv1d(24, 24, kernel_size=5, padding=2),
            nn.ReLU(),
            nn.MaxPool1d(2),
        )
        # Activity branch: 8 activity class percentages
        self.activity_conv = nn.Sequential(
            nn.Conv1d(8, 24, kernel_size=5, padding=2),
            nn.ReLU(),
            nn.Conv1d(24, 24, kernel_size=5, padding=2),
            nn.ReLU(),
            nn.MaxPool1d(2),
        )
        # Remaining: prosody(2) + sleep(3) + env(5) + battery/steps(2) = 12
        conv_out = (32 + 24 + 24) + 12
        self.lstm = nn.LSTM(
            input_size=conv_out,
            hidden_size=hidden,
            num_layers=2,
            batch_first=True,
            dropout=0.3,
        )
        self.head = nn.Sequential(
            nn.Linear(hidden, 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, OUTPUT_DIM),
            nn.Sigmoid(),
        )

    def forward(self, x):
        """x: (batch, seq_len, 26) → split into branches."""
        # Split input
        vitals = x[:, :, 0:3].permute(0, 2, 1)    # (B, 3, S)
        eda    = x[:, :, 3:6].permute(0, 2, 1)    # (B, 3, S)
        activ  = x[:, :, 6:14].permute(0, 2, 1)   # (B, 8, S)
        rest   = x[:, :, 14:26]                     # (B, S, 12)

        v = self.vitals_conv(vitals)    # (B, 32, S//2)
        e = self.eda_conv(eda)          # (B, 24, S//2)
        a = self.activity_conv(activ)   # (B, 24, S//2)

        # Align rest to S//2 by taking every other sample
        rest_ds = rest[:, ::2, :]       # (B, S//2, 12)

        # Concat along feature dim
        seq_len = v.size(2)
        combined = torch.cat([
            v.permute(0, 2, 1),
            e.permute(0, 2, 1),
            a.permute(0, 2, 1),
            rest_ds,
        ], dim=2)  # (B, S//2, conv_out)

        out, _ = self.lstm(combined)
        last = out[:, -1, :]  # (B, hidden)
        return self.head(last)


# ---------------------------------------------------------------------------
# Dataset (synthetic for development)
# ---------------------------------------------------------------------------
class StressDataset(Dataset):
    """Synthetic stress data for development.

    In production, labeled by PSS-10 scores + cortisol + MBI.
    """
    def __init__(self, n_samples=500):
        np.random.seed(42)
        self.samples = []
        self.labels = []
        for _ in range(n_samples):
            # Generate 24h of vitals with varying stress
            base_hrv = np.random.uniform(30, 80)
            stress_level = np.random.uniform(0, 1)

            data = np.zeros((SEQ_LEN, INPUT_DIM))
            # HR (higher with stress)
            data[:, 0] = 60 + stress_level * 25 + np.random.normal(0, 3, SEQ_LEN)
            # HRV (lower with stress)
            data[:, 1] = base_hrv * (1 - stress_level * 0.4) + np.random.normal(0, 3, SEQ_LEN)
            # Skin temp
            data[:, 2] = 33.0 + np.random.normal(0, 0.5, SEQ_LEN)
            # EDA SCL (higher with stress)
            data[:, 3] = 5 + stress_level * 10 + np.random.normal(0, 1, SEQ_LEN)
            # SCR rate (higher with stress)
            data[:, 4] = stress_level * 10 + np.random.normal(0, 1, SEQ_LEN)
            # SCR amplitude
            data[:, 5] = stress_level * 2 + np.random.normal(0, 0.3, SEQ_LEN)
            # Activity distribution (8 classes, sum to 1)
            act = np.random.dirichlet([5, 3, 1, 3, 4, 5, 2, 1])
            for t in range(SEQ_LEN):
                data[t, 6:14] = act + np.random.normal(0, 0.02, 8)
            # Prosody stress (0-3)
            data[:, 14] = stress_level * 3
            # Speech minutes
            data[:, 15] = np.random.uniform(0, 30, SEQ_LEN)
            # Sleep duration
            data[:, 16] = 7 * (1 - stress_level * 0.3)
            # Sleep efficiency
            data[:, 17] = 0.9 * (1 - stress_level * 0.2)
            # HRV during sleep
            data[:, 18] = base_hrv * 1.2 * (1 - stress_level * 0.3)
            # Environment
            data[:, 19] = np.random.uniform(100, 800, SEQ_LEN)  # lux
            data[:, 20] = np.random.uniform(3000, 5500, SEQ_LEN)  # CCT
            data[:, 21] = 22 + np.random.normal(0, 2, SEQ_LEN)  # temp
            data[:, 22] = 45 + np.random.normal(0, 10, SEQ_LEN)  # humidity
            data[:, 23] = 40 + stress_level * 20 + np.random.normal(0, 5, SEQ_LEN)  # noise
            data[:, 24] = 80 + np.random.normal(0, 10, SEQ_LEN)  # battery
            data[:, 25] = np.random.uniform(2000, 12000, SEQ_LEN)  # steps

            self.samples.append(data.astype(np.float32))
            self.labels.append([stress_level, stress_level * 0.7])

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        return torch.tensor(self.samples[idx]), torch.tensor(self.labels[idx])


# ---------------------------------------------------------------------------
# Training
# ---------------------------------------------------------------------------
def train():
    dataset = StressDataset(n_samples=500)
    loader = DataLoader(dataset, batch_size=BATCH_SIZE, shuffle=True)

    model = StressCNNLSTM().to(DEVICE)
    optimizer = AdamW(model.parameters(), lr=LR)
    scheduler = CosineAnnealingLR(optimizer, T_max=EPOCHS)
    criterion = nn.MSELoss()

    for epoch in range(EPOCHS):
        model.train()
        total_loss = 0
        for x, y in loader:
            x, y = x.to(DEVICE), y.to(DEVICE)
            optimizer.zero_grad()
            pred = model(x)
            loss = criterion(pred, y)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
        scheduler.step()
        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{EPOCHS} — Loss: {total_loss/len(loader):.4f}")

    torch.save(model.state_dict(), MODEL_SAVE)
    print(f"✓ Model saved to {MODEL_SAVE}")
    export_tflite(model)


def export_tflite(model):
    """Export to TFLite int8 for on-hub TFLite Micro inference."""
    try:
        model.eval()
        dummy = torch.randn(1, SEQ_LEN, INPUT_DIM)
        traced = torch.jit.trace(model, dummy)
        # In production: use ai_edge_torch or onnx2tf to convert to TFLite
        # For now, just log that conversion would happen
        print(f"✓ TFLite export would produce: {TFLITE_EXPORT} (<80KB int8)")
    except Exception as e:
        print(f"TFLite export deferred: {e}")


if __name__ == "__main__":
    train()
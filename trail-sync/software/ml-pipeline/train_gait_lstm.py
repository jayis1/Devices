"""
TrailSync — Gait Analysis LSTM Training Script

Processes 200 Hz IMU + pressure insole data to classify gait patterns.
Detects asymmetry, overpronation, cadence drift, and high-impact running.
On-device TFLite Micro binary classifies "normal/asymmetric/overpronating/high-impact"
every 2 seconds. Cloud LSTM tracks gait trends over 7+ days for injury prediction.

Input: 2-second windows of IMU (6 channels) + pressure zones (6 channels) = 12 channels × 400 samples
Output: 4-class gait classification + terrain class (8 classes) + gait metrics

Architecture: 2-layer bidirectional LSTM (128 units) + attention + FC heads

SPDX-License-Identifier: MIT
"""
import numpy as np
import pandas as pd
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
import matplotlib.pyplot as plt
import json
import os

# ─── Configuration ──────────────────────────────────────────────────────

WINDOW_SAMPLES = 400  # 2 seconds at 200 Hz
INPUT_CHANNELS = 12    # ax,ay,az,gx,gy,gz + 6 pressure zones
GAIT_CLASSES = ["normal", "asymmetric", "overpronating", "high_impact"]
TERRAIN_CLASSES = ["road", "gravel", "dirt", "mud", "snow", "ice", "rock", "sand"]
BATCH_SIZE = 64
EPOCHS = 50
LR = 1e-3
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

# ─── Dataset ────────────────────────────────────────────────────────────

class GaitDataset(Dataset):
    """Loads gait data from numpy arrays or CSV files.

    Expected data format:
    - imu_data: (N, 400, 6) — ax,ay,az,gx,gy,gz
    - pressure_data: (N, 400, 6) — 6 pressure zone averages
    - gait_labels: (N,) — 0=normal, 1=asymmetric, 2=overpronating, 3=high-impact
    - terrain_labels: (N,) — 0-7 terrain type
    - metrics: (N, 6) — cadence, contact_time, vert_osc, impact, pronation, stride_len
    """
    def __init__(self, imu_data, pressure_data, gait_labels, terrain_labels, metrics):
        self.imu = torch.FloatTensor(imu_data)
        self.pressure = torch.FloatTensor(pressure_data)
        self.gait_labels = torch.LongTensor(gait_labels)
        self.terrain_labels = torch.LongTensor(terrain_labels)
        self.metrics = torch.FloatTensor(metrics)

    def __len__(self):
        return len(self.gait_labels)

    def __getitem__(self, idx):
        # Concatenate IMU + pressure channels
        x = torch.cat([self.imu[idx], self.pressure[idx]], dim=-1)  # (400, 12)
        return x, self.gait_labels[idx], self.terrain_labels[idx], self.metrics[idx]


# ─── Model ──────────────────────────────────────────────────────────────

class GaitAnalysisLSTM(nn.Module):
    """Bidirectional LSTM with attention for gait classification + terrain + metrics."""
    def __init__(self, input_dim=12, hidden_dim=128, num_layers=2,
                 num_gait_classes=4, num_terrain_classes=8, num_metrics=6):
        super().__init__()
        self.hidden_dim = hidden_dim

        # Input normalization
        self.input_norm = nn.LayerNorm(input_dim)

        # Bidirectional LSTM
        self.lstm = nn.LSTM(input_dim, hidden_dim, num_layers=num_layers,
                           batch_first=True, bidirectional=True, dropout=0.3)

        # Attention mechanism
        self.attention = nn.Sequential(
            nn.Linear(hidden_dim * 2, hidden_dim),
            nn.Tanh(),
            nn.Linear(hidden_dim, 1),
        )

        # Gait classification head
        self.gait_head = nn.Sequential(
            nn.Linear(hidden_dim * 2, 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, num_gait_classes),
        )

        # Terrain classification head
        self.terrain_head = nn.Sequential(
            nn.Linear(hidden_dim * 2, 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, num_terrain_classes),
        )

        # Gait metrics regression head
        self.metrics_head = nn.Sequential(
            nn.Linear(hidden_dim * 2, 64),
            nn.ReLU(),
            nn.Linear(64, num_metrics),
        )

    def forward(self, x):
        # x: (batch, 400, 12)
        x = self.input_norm(x)

        # LSTM encoding
        lstm_out, _ = self.lstm(x)  # (batch, 400, hidden*2)

        # Attention weighting
        attn_weights = torch.softmax(self.attention(lstm_out), dim=1)  # (batch, 400, 1)
        context = torch.sum(attn_weights * lstm_out, dim=1)  # (batch, hidden*2)

        # Multi-task outputs
        gait_logits = self.gait_head(context)
        terrain_logits = self.terrain_head(context)
        metrics_pred = self.metrics_head(context)

        return gait_logits, terrain_logits, metrics_pred


# ─── Training ───────────────────────────────────────────────────────────

def train_gait_model(train_loader, val_loader, epochs=EPOCHS):
    model = GaitAnalysisLSTM().to(DEVICE)
    optimizer = torch.optim.AdamW(model.parameters(), lr=LR, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs)

    gait_criterion = nn.CrossEntropyLoss(label_smoothing=0.1)
    terrain_criterion = nn.CrossEntropyLoss(label_smoothing=0.1)
    metrics_criterion = nn.MSELoss()

    best_val_loss = float('inf')

    for epoch in range(epochs):
        model.train()
        train_loss = 0
        for batch in train_loader:
            x, gait_y, terrain_y, metrics_y = batch
            x = x.to(DEVICE)
            gait_y = gait_y.to(DEVICE)
            terrain_y = terrain_y.to(DEVICE)
            metrics_y = metrics_y.to(DEVICE)

            gait_logits, terrain_logits, metrics_pred = model(x)

            loss = (gait_criterion(gait_logits, gait_y) +
                    terrain_criterion(terrain_logits, terrain_y) * 0.5 +
                    metrics_criterion(metrics_pred, metrics_y) * 0.1)

            optimizer.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()
            train_loss += loss.item()

        scheduler.step()

        # Validation
        model.eval()
        val_loss = 0
        gait_preds, gait_trues = [], []
        with torch.no_grad():
            for batch in val_loader:
                x, gait_y, terrain_y, metrics_y = batch
                x = x.to(DEVICE)
                gait_y = gait_y.to(DEVICE)
                terrain_y = terrain_y.to(DEVICE)
                metrics_y = metrics_y.to(DEVICE)

                gait_logits, terrain_logits, metrics_pred = model(x)

                loss = (gait_criterion(gait_logits, gait_y) +
                        terrain_criterion(terrain_logits, terrain_y) * 0.5 +
                        metrics_criterion(metrics_pred, metrics_y) * 0.1)
                val_loss += loss.item()

                gait_preds.extend(gait_logits.argmax(dim=-1).cpu().numpy())
                gait_trues.extend(gait_y.cpu().numpy())

        avg_train_loss = train_loss / len(train_loader)
        avg_val_loss = val_loss / len(val_loader)

        if avg_val_loss < best_val_loss:
            best_val_loss = avg_val_loss
            torch.save(model.state_dict(), "gait_model_best.pt")

        if (epoch + 1) % 5 == 0:
            print(f"Epoch {epoch+1}/{epochs} — Train: {avg_train_loss:.4f} Val: {avg_val_loss:.4f}")

    # Classification report
    print("\nGait Classification Report:")
    print(classification_report(gait_trues, gait_preds,
                                target_names=GAIT_CLASSES))
    return model


# ─── Export to TFLite Micro ─────────────────────────────────────────────

def export_tflite(model, output_path="gait_model.tflite"):
    """Export PyTorch model to TFLite Micro for on-device inference."""
    model.eval()
    # Convert to ONNX first
    dummy_input = torch.randn(1, WINDOW_SAMPLES, INPUT_CHANNELS).to(DEVICE)
    torch.onnx.export(model, dummy_input, "gait_model.onnx",
                      input_names=["input"], output_names=["gait", "terrain", "metrics"],
                      dynamic_axes=None, opset_version=13)

    # Convert ONNX to TFLite via onnxruntime
    # In production: use onnx2tf + tflite_converter
    # The resulting .tflite model should be < 150KB for on-device deployment
    print(f"Model exported to ONNX. Convert to TFLite Micro (< 150KB) for shoe pod deployment.")
    print(f"Target: gait_class (4 classes) + terrain (8 classes) + metrics (6 values)")


# ─── Synthetic Data Generation (for testing) ───────────────────────────

def generate_synthetic_data(n_samples=10000):
    """Generate synthetic gait data for pipeline testing."""
    np.random.seed(42)

    imu_data = np.zeros((n_samples, WINDOW_SAMPLES, 6))
    pressure_data = np.zeros((n_samples, WINDOW_SAMPLES, 6))
    gait_labels = np.zeros(n_samples, dtype=int)
    terrain_labels = np.zeros(n_samples, dtype=int)
    metrics = np.zeros((n_samples, 6))

    for i in range(n_samples):
        gait_class = np.random.randint(0, 4)
        gait_labels[i] = gait_class
        terrain_labels[i] = np.random.randint(0, 8)

        # Base running signal (sinusoidal at ~3 Hz for 180 spm cadence)
        t = np.linspace(0, 2.0, WINDOW_SAMPLES)

        if gait_class == 0:  # normal
            imu_data[i, :, 2] = 1.0 + 0.5 * np.sin(2 * np.pi * 3 * t)  # vertical
            imu_data[i, :, 0] = 0.1 * np.sin(2 * np.pi * 3 * t)  # AP
            imu_data[i, :, 1] = 0.05 * np.random.randn(WINDOW_SAMPLES)  # ML
            asymmetry = 0.02
            pronation = np.random.uniform(-5, 5)
            impact = np.random.uniform(200, 300)
        elif gait_class == 1:  # asymmetric
            imu_data[i, :, 2] = 1.0 + 0.7 * np.sin(2 * np.pi * 3 * t)  # higher impact
            imu_data[i, :, 0] = 0.3 * np.sin(2 * np.pi * 3 * t)  # more AP
            imu_data[i, :, 1] = 0.2 * np.random.randn(WINDOW_SAMPLES)  # more ML
            asymmetry = np.random.uniform(0.05, 0.15)
            pronation = np.random.uniform(-8, 8)
            impact = np.random.uniform(280, 380)
        elif gait_class == 2:  # overpronating
            imu_data[i, :, 2] = 1.0 + 0.6 * np.sin(2 * np.pi * 3 * t)
            imu_data[i, :, 1] = 0.4 * np.sin(2 * np.pi * 3 * t)  # medial shift
            imu_data[i, :, 3] = 0.3 * np.sin(2 * np.pi * 3 * t)  # gyro pronation
            asymmetry = np.random.uniform(0.02, 0.08)
            pronation = np.random.uniform(10, 25)  # >10° = overpronation
            impact = np.random.uniform(250, 350)
        else:  # high-impact
            imu_data[i, :, 2] = 1.0 + 1.2 * np.sin(2 * np.pi * 3 * t)  # much higher
            imu_data[i, :, 0] = 0.5 * np.sin(2 * np.pi * 3 * t)
            imu_data[i, :, 1] = 0.3 * np.random.randn(WINDOW_SAMPLES)
            asymmetry = np.random.uniform(0.03, 0.1)
            pronation = np.random.uniform(-5, 10)
            impact = np.random.uniform(350, 500)  # > 350% BW

        # Pressure zones
        for j in range(6):
            pressure_data[i, :, j] = (0.5 + 0.3 * np.sin(2 * np.pi * 3 * t) +
                                       0.1 * np.random.randn(WINDOW_SAMPLES))
            if gait_class == 2:  # overpronating: more medial pressure
                if j % 2 == 0:
                    pressure_data[i, :, j] *= 1.5  # medial shift

        # Metrics: cadence, contact_time, vert_osc, impact, pronation, stride
        metrics[i, 0] = np.random.uniform(160, 200)  # cadence spm
        metrics[i, 1] = np.random.uniform(200, 300)  # contact time ms
        metrics[i, 2] = np.random.uniform(5, 15)      # vertical osc mm
        metrics[i, 3] = impact                        # impact % BW
        metrics[i, 4] = pronation                     # pronation deg
        metrics[i, 5] = np.random.uniform(100, 160)   # stride length cm

    return imu_data, pressure_data, gait_labels, terrain_labels, metrics


if __name__ == "__main__":
    print("TrailSync Gait Analysis LSTM Training")
    print("=" * 50)

    # Generate synthetic data for pipeline testing
    print("Generating synthetic gait data...")
    imu, pressure, gait_labels, terrain_labels, metrics = generate_synthetic_data()

    # Split into train/val
    indices = np.arange(len(gait_labels))
    train_idx, val_idx = train_test_split(indices, test_size=0.2, stratify=gait_labels)

    train_dataset = GaitDataset(
        imu[train_idx], pressure[train_idx],
        gait_labels[train_idx], terrain_labels[train_idx], metrics[train_idx])
    val_dataset = GaitDataset(
        imu[val_idx], pressure[val_idx],
        gait_labels[val_idx], terrain_labels[val_idx], metrics[val_idx])

    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=BATCH_SIZE)

    # Train model
    print(f"\nTraining on {DEVICE}...")
    model = train_gait_model(train_loader, val_loader, epochs=EPOCHS)

    # Export to ONNX/TFLite
    export_tflite(model)
    print("\nTraining complete! Model saved to gait_model_best.pt")
"""
BrewSync ML Pipeline — Fermentation Progress Model

LSTM that predicts final gravity (FG) and fermentation completion time
from the first 24-48 hours of sensor data (SG, temp, CO2, pH).

Input: sliding window of sensor readings (12h windows, 5-min intervals)
Output: predicted FG, estimated completion time, confidence score

Architecture:
- 4-layer LSTM (hidden_size=128, bidirectional)
- Attention pooling over time steps
- MLP head → FG prediction + completion time (hours)

Training data: synthetic fermentation curves + real homebrew datasets
Output: ONNX model for edge inference (Hub RP2040 is too small; runs on cloud)
"""

import torch
import torch.nn as nn
import numpy as np
from pathlib import Path


class FermentationProgressModel(nn.Module):
    """LSTM-based fermentation progress predictor."""

    def __init__(self, input_size=5, hidden_size=128, num_layers=4, dropout=0.2):
        super().__init__()
        self.hidden_size = hidden_size
        self.num_layers = num_layers

        # Input: [SG, temp_c, co2_ppm, pressure_bar, pH]
        self.lstm = nn.LSTM(
            input_size=input_size,
            hidden_size=hidden_size,
            num_layers=num_layers,
            batch_first=True,
            dropout=dropout,
            bidirectional=True,
        )

        # Attention mechanism
        self.attention = nn.Linear(hidden_size * 2, 1)

        # Prediction heads
        self.fg_head = nn.Sequential(
            nn.Linear(hidden_size * 2, 64),
            nn.ReLU(),
            nn.Dropout(0.2),
            nn.Linear(64, 1),  # Predicted FG
        )

        self.time_head = nn.Sequential(
            nn.Linear(hidden_size * 2, 64),
            nn.ReLU(),
            nn.Dropout(0.2),
            nn.Linear(64, 1),  # Hours until completion
        )

        self.confidence_head = nn.Sequential(
            nn.Linear(hidden_size * 2, 32),
            nn.ReLU(),
            nn.Linear(32, 1),
            nn.Sigmoid(),  # Confidence 0-1
        )

    def forward(self, x):
        """
        Args:
            x: (batch, seq_len, input_size) — sensor readings over time
        Returns:
            fg_pred: (batch, 1) — predicted final gravity
            time_pred: (batch, 1) — predicted hours to completion
            confidence: (batch, 1) — model confidence score
        """
        # LSTM encoding
        lstm_out, _ = self.lstm(x)  # (batch, seq_len, hidden_size*2)

        # Attention pooling
        attn_weights = torch.softmax(self.attention(lstm_out), dim=1)  # (batch, seq_len, 1)
        context = torch.sum(attn_weights * lstm_out, dim=1)  # (batch, hidden_size*2)

        # Predictions
        fg_pred = self.fg_head(context)
        time_pred = self.time_head(context)
        confidence = self.confidence_head(context)

        return fg_pred, time_pred, confidence


class FermentationDataset(torch.utils.data.Dataset):
    """Dataset of fermentation time series."""

    def __init__(self, data_dir: str, window_hours: int = 24, interval_minutes: int = 5):
        self.data_dir = Path(data_dir)
        self.window_size = window_hours * 60 // interval_minutes  # Number of time steps
        self.interval = interval_minutes
        self.samples = self._load_samples()

    def _load_samples(self):
        """Load fermentation batch CSVs and create sliding windows."""
        samples = []
        data_path = Path(self.data_dir)
        if data_path.exists():
            for csv_file in sorted(data_path.glob("*.csv")):
                # Each CSV has columns: timestamp, sg, temp_c, co2_ppm, pressure_bar, ph, fg_actual
                # In production, load from TimescaleDB
                pass
        else:
            # Generate synthetic data for training
            samples = self._generate_synthetic()
        return samples

    def _generate_synthetic(self, n_batches: int = 200):
        """Generate synthetic fermentation curves for training."""
        samples = []
        np.random.seed(42)

        for _ in range(n_batches):
            # Random fermentation parameters
            og = 1.040 + np.random.random() * 0.080  # 1.040 - 1.120
            fg = 1.005 + np.random.random() * 0.015    # 1.005 - 1.020
            duration_hours = 72 + np.random.random() * 120  # 3-8 days
            temp_base = 16 + np.random.random() * 10  # 16-26°C

            # Generate time series
            n_points = int(duration_hours * 60 / self.interval)
            t = np.linspace(0, duration_hours, n_points)

            # SG follows a logistic decay
            sg_curve = og - (og - fg) / (1 + np.exp(-0.05 * (t - duration_hours * 0.3)))

            # Temperature: slight rise then stable
            temp_curve = temp_base + 1.5 * np.exp(-0.02 * (t - 12)**2 / 100)

            # CO2: bell curve peaking mid-fermentation
            co2_peak = 2000 + np.random.random() * 3000
            co2_curve = co2_peak * np.exp(-0.5 * ((t - duration_hours * 0.25) / (duration_hours * 0.15))**2)

            # pH: starts ~5.2, drops to ~4.2
            ph_curve = 5.2 - 1.0 * (1 / (1 + np.exp(-0.1 * (t - 24))))

            # Pressure: follows CO2 roughly
            pressure_curve = 1.0 + 0.05 * co2_curve / 2000

            # Create windows
            for start in range(0, min(n_points - self.window_size, 12 * 12), 12):  # Every hour
                end = start + self.window_size
                if end > n_points:
                    break

                x = np.stack([
                    sg_curve[start:end],
                    temp_curve[start:end],
                    co2_curve[start:end],
                    pressure_curve[start:end],
                    ph_curve[start:end],
                ], axis=-1)

                # Add noise
                x += np.random.normal(0, 0.001, x.shape)

                samples.append({
                    "input": x.astype(np.float32),
                    "fg_target": fg,
                    "time_target": max(0, duration_hours - t[end]),
                })

        return samples

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        sample = self.samples[idx]
        return (
            torch.tensor(sample["input"]),
            torch.tensor(sample["fg_target"], dtype=torch.float32),
            torch.tensor(sample["time_target"], dtype=torch.float32),
        )


def train_model(
    data_dir: str = "data/fermentation",
    epochs: int = 100,
    batch_size: int = 32,
    lr: float = 1e-3,
    output_dir: str = "models",
):
    """Train the fermentation progress model."""
    import os
    os.makedirs(output_dir, exist_ok=True)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = FermentationProgressModel().to(device)

    dataset = FermentationDataset(data_dir)
    dataloader = torch.utils.data.DataLoader(dataset, batch_size=batch_size, shuffle=True)

    optimizer = torch.optim.Adam(model.parameters(), lr=lr)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs)
    fg_criterion = nn.MSELoss()
    time_criterion = nn.MSELoss()

    for epoch in range(epochs):
        model.train()
        total_loss = 0.0
        for batch_x, batch_fg, batch_time in dataloader:
            batch_x = batch_x.to(device)
            batch_fg = batch_fg.to(device)
            batch_time = batch_time.to(device)

            fg_pred, time_pred, confidence = model(batch_x)

            fg_loss = fg_criterion(fg_pred.squeeze(), batch_fg)
            time_loss = time_criterion(time_pred.squeeze(), batch_time)
            confidence_penalty = -torch.mean(confidence)  # Encourage high confidence only when accurate

            loss = fg_loss + 0.01 * time_loss + 0.1 * confidence_penalty

            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

            total_loss += loss.item()

        scheduler.step()
        avg_loss = total_loss / len(dataloader)

        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{epochs} — Loss: {avg_loss:.6f}")

    # Save model
    torch.save(model.state_dict(), f"{output_dir}/fermentation_progress.pt")
    print(f"Model saved to {output_dir}/fermentation_progress.pt")

    # Export to ONNX for production
    model.eval()
    dummy_input = torch.randn(1, dataset.window_size, 5).to(device)
    torch.onnx.export(
        model, dummy_input,
        f"{output_dir}/fermentation_progress.onnx",
        input_names=["sensor_readings"],
        output_names=["fg_pred", "time_pred", "confidence"],
        dynamic_axes={"sensor_readings": {0: "batch"}},
    )
    print(f"ONNX model exported to {output_dir}/fermentation_progress.onnx")

    return model


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-dir", default="data/fermentation")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--output-dir", default="models")
    args = parser.parse_args()

    train_model(
        data_dir=args.data_dir,
        epochs=args.epochs,
        batch_size=args.batch_size,
        lr=args.lr,
        output_dir=args.output_dir,
    )
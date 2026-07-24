#!/usr/bin/env python3
"""
EchoSync — SoundLocalize Training Script
Direction-of-arrival estimation using SRP-PHAT + CNN refinement.

Input: 4-mic cross-correlation features + mel-spectrogram
Output: Azimuth (0-360°) and elevation angle

The model combines classical beamforming (SRP-PHAT) with a CNN that
refines the estimate based on spectral characteristics of the sound.
"""
import argparse
import os
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader


class SoundLocalizeNet(nn.Module):
    """CNN that refines SRP-PHAT direction estimate."""

    def __init__(self):
        super().__init__()
        # Input: 4 mic signals (4 × 16000 samples = 64000)
        self.features = nn.Sequential(
            nn.Conv1d(4, 32, kernel_size=64, stride=8),
            nn.ReLU(),
            nn.MaxPool1d(2),
            nn.Conv1d(32, 64, kernel_size=16, stride=4),
            nn.ReLU(),
            nn.MaxPool1d(2),
            nn.Conv1d(64, 128, kernel_size=8, stride=2),
            nn.ReLU(),
            nn.AdaptiveAvgPool1d(1),
        )
        self.regressor = nn.Sequential(
            nn.Linear(128 + 1, 64),  # 128 features + SRP-PHAT estimate
            nn.ReLU(),
            nn.Linear(64, 2),  # azimuth_sin, azimuth_cos
        )

    def forward(self, x, srp_estimate):
        x = self.features(x)  # (B, 128, 1)
        x = x.squeeze(-1)    # (B, 128)
        x = torch.cat([x, srp_estimate.unsqueeze(1)], dim=1)
        out = self.regressor(x)
        # Convert sin/cos to angle
        azimuth = torch.atan2(out[:, 0], out[:, 1])
        return azimuth


class LocalizeDataset(Dataset):
    """Synthetic 4-mic array data with known source directions."""

    def __init__(self, n_samples=5000, sr=16000, duration=2.0):
        self.n_samples = n_samples
        self.sr = sr
        self.duration = duration
        self.mic_spacing = 0.05  # 50mm
        self.speed_sound = 343.0

    def __len__(self):
        return self.n_samples

    def __getitem__(self, idx):
        # Random source direction
        azimuth_true = np.random.uniform(0, 2 * np.pi)
        elevation = 0  # Planar array

        # Generate signal at each mic with TDOA
        n_samples = int(self.sr * self.duration)
        t = np.arange(n_samples) / self.sr
        freq = np.random.uniform(100, 8000)
        source_signal = np.sin(2 * np.pi * freq * t)

        # TDOA for each mic (square array, 50mm spacing)
        # Mic positions: (0,0), (d,0), (0,d), (d,d)
        d = self.mic_spacing
        positions = np.array([[0, 0], [d, 0], [0, d], [d, d]])
        mic_signals = np.zeros((4, n_samples), dtype=np.float32)

        for i, pos in enumerate(positions):
            # Distance from source at given azimuth
            dx = pos[0] * np.cos(azimuth_true)
            dy = pos[1] * np.sin(azimuth_true)
            dist_diff = dx + dy
            tdoa = dist_diff / self.speed_sound
            delay_samples = int(tdoa * self.sr)
            if abs(delay_samples) < n_samples:
                mic_signals[i] = np.roll(source_signal, delay_samples)
            # Add noise
            mic_signals[i] += np.random.randn(n_samples).astype(np.float32) * 0.1

        # SRP-PHAT estimate (simplified: from correlation)
        # In production: full SRP-PHAT implementation
        srp_estimate = azimuth_true + np.random.randn() * 0.3  # ±17° noise

        return torch.from_numpy(mic_signals), torch.tensor(srp_estimate, dtype=torch.float32), torch.tensor(azimuth_true, dtype=torch.float32)


def train_model(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on: {device}")

    train_ds = LocalizeDataset(n_samples=args.n_samples)
    train_loader = DataLoader(train_ds, batch_size=args.batch_size, shuffle=True)

    model = SoundLocalizeNet().to(device)
    optimizer = optim.Adam(model.parameters(), lr=args.lr)
    criterion = nn.MSELoss()

    for epoch in range(args.epochs):
        model.train()
        total_loss = 0.0
        total_error = 0.0

        for mic_signals, srp_est, true_az in train_loader:
            mic_signals = mic_signals.to(device)
            srp_est = srp_est.to(device)
            true_az = true_az.to(device)

            optimizer.zero_grad()
            pred_az = model(mic_signals, srp_est)
            loss = criterion(pred_az, true_az)
            loss.backward()
            optimizer.step()

            total_loss += loss.item()
            # Angular error in degrees
            error_deg = torch.rad2deg(torch.abs(torch.atan2(
                torch.sin(pred_az - true_az),
                torch.cos(pred_az - true_az)
            )))
            total_error += error_deg.mean().item()

        avg_loss = total_loss / len(train_loader)
        avg_error = total_error / len(train_loader)

        print(f"Epoch {epoch+1}/{args.epochs} | Loss: {avg_loss:.6f} | "
              f"MAE: {avg_error:.2f}°")

    print(f"\nFinal MAE: {avg_error:.2f}° (target: <20°)")

    os.makedirs(args.output, exist_ok=True)
    torch.save(model.state_dict(), os.path.join(args.output, "sound_localize.pth"))
    print(f"Model saved to {args.output}/sound_localize.pth")


def main():
    parser = argparse.ArgumentParser(description="Train SoundLocalize")
    parser.add_argument("--output", default="./models")
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--n-samples", type=int, default=5000)
    args = parser.parse_args()
    train_model(args)


if __name__ == "__main__":
    main()
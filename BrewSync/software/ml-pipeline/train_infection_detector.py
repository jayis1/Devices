"""
BrewSync ML Pipeline — Infection Detector

Detects and classifies fermentation infections from spectral data (AS7341).
15-class classifier for common brewing contaminants:
  0: healthy (no infection)
  1: lactobacillus (lactic acid bacteria)
  2: acetobacter (acetic acid bacteria → vinegar)
  3: brettanomyces (wild yeast → funk)
  4: pediococcus (beer sickness)
  5: enterobacter (early contamination)
  6: wild_saccharomyces
  7: mold_aspergillus
  8: mold_fusarium
  9: mold_mucor
  10: acetobacter_pasteurianus
  11: lactobacillus_brevis
  12: lactobacillus_lindneri
  13: pediococcus_damnosus
  14: megasphaera

Uses 11-channel spectral data (AS7341: F1-F8 + Clear + NIR) + metadata.
"""

import numpy as np
import torch
import torch.nn as nn
from pathlib import Path


CLASS_NAMES = [
    "healthy", "lactobacillus", "acetobacter", "brettanomyces", "pediococcus",
    "enterobacter", "wild_saccharomyces", "mold_aspergillus", "mold_fusarium",
    "mold_mucor", "acetobacter_pasteurianus", "lactobacillus_brevis",
    "lactobacillus_lindneri", "pediococcus_damnosus", "megasphaera",
]


class InfectionDetector(nn.Module):
    """1D CNN for spectral infection classification."""

    def __init__(self, n_spectral_channels=11, n_classes=15, n_metadata=3):
        super().__init__()

        # Spectral CNN branch
        self.spectral_net = nn.Sequential(
            nn.Conv1d(1, 32, kernel_size=3, padding=1),
            nn.BatchNorm1d(32),
            nn.ReLU(),
            nn.Conv1d(32, 64, kernel_size=3, padding=1),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.AdaptiveAvgPool1d(1),
            nn.Flatten(),
        )

        # Metadata branch (pH, temp, days_since_start)
        self.metadata_net = nn.Sequential(
            nn.Linear(n_metadata, 16),
            nn.ReLU(),
            nn.Linear(16, 16),
            nn.ReLU(),
        )

        # Combined classifier
        self.classifier = nn.Sequential(
            nn.Linear(64 + 16, 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, n_classes),
        )

    def forward(self, spectral, metadata):
        """
        Args:
            spectral: (batch, 11) — AS7341 11-channel spectral data
            metadata: (batch, 3) — [pH, temp_c, days_since_start]
        Returns:
            logits: (batch, 15) — class logits
        """
        # Reshape for 1D CNN
        x_spec = spectral.unsqueeze(1)  # (batch, 1, 11)
        spec_features = self.spectral_net(x_spec)
        meta_features = self.metadata_net(metadata)
        combined = torch.cat([spec_features, meta_features], dim=1)
        logits = self.classifier(combined)
        return logits


class AnomalyDetector:
    """Simple spectral anomaly detector using Mahalanobis distance."""

    def __init__(self):
        self.mean = None
        self.cov_inv = None
        self.fitted = False

    def fit(self, X: np.ndarray):
        """Fit on healthy spectral data."""
        self.mean = np.mean(X, axis=0)
        cov = np.cov(X.T)
        # Regularize covariance matrix
        cov += np.eye(cov.shape[0]) * 1e-6
        self.cov_inv = np.linalg.inv(cov)
        self.fitted = True

    def score(self, X: np.ndarray) -> np.ndarray:
        """Compute anomaly scores (Mahalanobis distance from healthy baseline)."""
        if not self.fitted:
            raise RuntimeError("Must fit before scoring")
        diff = X - self.mean
        scores = np.sqrt(np.sum(diff @ self.cov_inv * diff, axis=1))
        return scores

    def predict(self, X: np.ndarray, threshold: float = 5.0) -> np.ndarray:
        """Predict anomaly: True if score > threshold."""
        return self.score(X) > threshold


def generate_synthetic_spectral(n_samples: int = 5000, seed: int = 42):
    """Generate synthetic spectral data for training."""
    np.random.seed(seed)

    # Healthy beer spectral signature (AS7341 channels)
    # F1(415nm), F2(445nm), F3(470nm), F4(510nm), F5(555nm),
    # F6(585nm), F7(640nm), F8(690nm), Clear, NIR
    healthy_profile = np.array([
        120, 280, 350, 480, 520, 440, 280, 180, 600, 380, 150
    ], dtype=np.float32)

    # Infection profiles (spectral shifts for each organism)
    infection_shifts = {
        1: np.array([80, 40, -20, -30, -40, -30, -10, 20, 50, 30, 10]),   # lactobacillus
        2: np.array([40, 60, 80, 50, -20, -30, -10, 10, 80, 50, 30]),     # acetobacter
        3: np.array([-30, -20, -10, 20, 40, 50, 30, 10, 100, 60, 40]),    # brettanomyces
        4: np.array([60, 30, -10, -20, -30, -20, 10, 30, 40, 20, 5]),     # pediococcus
        5: np.array([50, 70, 60, 30, -10, -20, -5, 10, 60, 40, 20]),     # enterobacter
        6: np.array([-20, -10, 10, 30, 20, 10, -5, -10, 70, 50, 30]),    # wild_saccharomyces
        7: np.array([100, 80, 50, 20, -10, -20, -30, -10, 30, 10, -5]),  # mold_aspergillus
        8: np.array([60, 40, 20, 10, -5, -10, -20, 10, 40, 20, 5]),      # mold_fusarium
        9: np.array([80, 60, 40, 10, -20, -30, -10, 5, 35, 15, 0]),      # mold_mucor
        10: np.array([30, 50, 70, 40, -10, -25, 0, 15, 70, 45, 25]),     # acetobacter_pasteurianus
        11: np.array([70, 35, -25, -35, -45, -25, -5, 15, 45, 25, 8]),   # lactobacillus_brevis
        12: np.array([75, 45, -15, -25, -35, -20, 5, 25, 55, 35, 12]),   # lactobacillus_lindneri
        13: np.array([55, 25, -5, -15, -25, -15, 15, 35, 35, 15, 3]),    # pediococcus_damnosus
        14: np.array([45, 65, 75, 55, 10, -10, 5, 20, 50, 30, 15]),      # megasphaera
    }

    X_spectral = []
    X_metadata = []
    y = []

    # Healthy samples (class 0)
    n_healthy = n_samples // 3
    for _ in range(n_healthy):
        sample = healthy_profile + np.random.normal(0, 20, 11).astype(np.float32)
        X_spectral.append(sample)
        # Metadata: [pH, temp_c, days_since_start]
        X_metadata.append([
            4.0 + np.random.normal(0, 0.2),    # Normal pH
            20 + np.random.normal(0, 1),         # Normal temp
            np.random.randint(1, 21),              # Days since start
        ])
        y.append(0)

    # Infected samples (classes 1-14)
    n_infected = n_samples - n_healthy
    samples_per_class = n_infected // 14
    for class_idx in range(1, 15):
        shift = infection_shifts[class_idx]
        for _ in range(samples_per_class):
            sample = healthy_profile + shift + np.random.normal(0, 25, 11).astype(np.float32)
            sample = np.maximum(sample, 0)  # No negative spectral values
            X_spectral.append(sample)
            # Infected samples tend to have different pH
            ph = 4.0 + np.random.normal(0.3, 0.3) if class_idx in (1, 4, 11, 12, 13) else 4.0 + np.random.normal(0, 0.2)
            X_metadata.append([ph, 20 + np.random.normal(0, 2), np.random.randint(3, 21)])
            y.append(class_idx)

    return np.array(X_spectral), np.array(X_metadata, dtype=np.float32), np.array(y)


def train_infection_detector(output_dir: str = "models", epochs: int = 50):
    """Train the infection detector model."""
    import os
    os.makedirs(output_dir, exist_ok=True)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

    print("Generating synthetic spectral training data...")
    X_spectral, X_metadata, y = generate_synthetic_spectral(n_samples=10000)

    # Convert to tensors
    X_spectral_t = torch.tensor(X_spectral, dtype=torch.float32).to(device)
    X_metadata_t = torch.tensor(X_metadata, dtype=torch.float32).to(device)
    y_t = torch.tensor(y, dtype=torch.long).to(device)

    # Split into train/val
    n = len(y)
    indices = torch.randperm(n)
    train_size = int(0.8 * n)
    train_idx, val_idx = indices[:train_size], indices[train_size:]

    model = InfectionDetector().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.CrossEntropyLoss()

    batch_size = 64

    for epoch in range(epochs):
        model.train()
        perm = torch.randperm(train_size)
        total_loss = 0.0
        correct = 0
        total = 0

        for i in range(0, train_size, batch_size):
            idx = perm[i:i+batch_size]
            if len(idx) == 0:
                continue

            spec = X_spectral_t[train_idx[idx]]
            meta = X_metadata_t[train_idx[idx]]
            labels = y_t[train_idx[idx]]

            logits = model(spec, meta)
            loss = criterion(logits, labels)

            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

            total_loss += loss.item()
            pred = logits.argmax(dim=1)
            correct += (pred == labels).sum().item()
            total += len(labels)

        if (epoch + 1) % 10 == 0:
            # Validate
            model.eval()
            with torch.no_grad():
                val_logits = model(X_spectral_t[val_idx], X_metadata_t[val_idx])
                val_pred = val_logits.argmax(dim=1)
                val_acc = (val_pred == y_t[val_idx]).float().mean().item()
            print(f"Epoch {epoch+1}/{epochs} — Loss: {total_loss/total:.4f} "
                  f"— Train Acc: {correct/total:.3f} — Val Acc: {val_acc:.3f}")

    # Save model
    torch.save(model.state_dict(), f"{output_dir}/infection_detector.pt")
    print(f"Model saved to {output_dir}/infection_detector.pt")

    # Also train and save anomaly detector
    print("Training anomaly detector...")
    anomaly = AnomalyDetector()
    healthy_mask = y == 0
    anomaly.fit(X_spectral[healthy_mask])
    joblib_path = f"{output_dir}/anomaly_detector.joblib"
    import joblib
    joblib.dump(anomaly, joblib_path)
    print(f"Anomaly detector saved to {joblib_path}")

    return model


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-path", default="data/spectral")
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--output-dir", default="models")
    args = parser.parse_args()

    train_infection_detector(output_dir=args.output_dir, epochs=args.epochs)
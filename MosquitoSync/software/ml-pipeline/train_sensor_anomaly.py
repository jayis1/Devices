#!/usr/bin/env python3
"""
SensorAnomaly — Isolation Forest Multi-Sensor Anomaly Detector

Detects sensor faults, drift, and environmental anomalies across all
MosquitoSync nodes (acoustic energy, IR breaks, temp, humidity, pressure,
rain, wind, battery, motor current) — 14-dim feature vector.

Architecture: Isolation Forest (100 trees, 256 sample size)
Use cases:
  - Microphone blocked/covered → audio energy drops to zero
  - IR beam misaligned → continuous breaks or zero breaks
  - BME280 condensation → humidity stuck at 100%
  - Rain gauge blocked → no tips during confirmed rain
  - Motor stuck → current spike without position change
  - Camera fogged → CaptureCount confidence drops
"""
from __future__ import annotations

import os
import numpy as np
from sklearn.ensemble import IsolationForest
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report

# 14-dim feature vector
ANOMALY_FEATURES = [
    "audio_energy", "ir_breaks", "temp", "humidity", "pressure",
    "rain", "wind_speed", "wind_dir", "battery_v", "motor_current",
    "co2_on", "fan_pct", "propane_pct", "trap_fullness",
]


def generate_synthetic_anomaly_data(
    n_normal: int = 5000, n_anomalous: int = 500
) -> tuple[np.ndarray, np.ndarray]:
    """Generate synthetic sensor data with injected anomalies."""
    rng = np.random.default_rng(42)

    # Normal data
    normal = np.stack([
        rng.uniform(50, 500, n_normal),    # audio_energy
        rng.poisson(5, n_normal),          # ir_breaks
        rng.uniform(15, 35, n_normal),     # temp
        rng.uniform(40, 90, n_normal),     # humidity
        rng.uniform(990, 1030, n_normal),  # pressure
        rng.exponential(2, n_normal),      # rain
        rng.uniform(0, 5, n_normal),       # wind_speed
        rng.uniform(0, 360, n_normal),     # wind_dir
        rng.uniform(3.2, 4.2, n_normal),   # battery_v
        rng.uniform(0, 50, n_normal),      # motor_current
        rng.integers(0, 2, n_normal),      # co2_on
        rng.uniform(0, 100, n_normal),     # fan_pct
        rng.uniform(20, 100, n_normal),   # propane_pct
        rng.uniform(0, 90, n_normal),      # trap_fullness
    ], axis=1).astype(np.float32)

    # Anomalous data (inject faults)
    anomalous = normal[:n_anomalous].copy()
    fault_types = rng.choice(6, n_anomalous)
    for i, fault in enumerate(fault_types):
        if fault == 0:  # Microphone blocked
            anomalous[i, 0] = rng.uniform(0, 10)
        elif fault == 1:  # IR beam stuck
            anomalous[i, 1] = rng.choice([0, 999])
        elif fault == 2:  # Humidity stuck at 100%
            anomalous[i, 3] = 100.0
        elif fault == 3:  # Rain gauge blocked (no rain during storm)
            anomalous[i, 5] = 0.0
        elif fault == 4:  # Motor stuck (current spike, no movement)
            anomalous[i, 9] = rng.uniform(200, 300)
        elif fault == 5:  # Battery failure
            anomalous[i, 8] = rng.uniform(1.5, 2.5)

    features = np.vstack([normal, anomalous])
    labels = np.concatenate([
        np.zeros(n_normal, dtype=int),
        np.ones(n_anomalous, dtype=int),
    ])
    return features, labels


def train_sensor_anomaly(
    save_path: str = "models/sensor_anomaly.pkl",
) -> None:
    """Train Isolation Forest anomaly detector."""
    print("[SensorAnomaly] Training Isolation Forest...")
    features, labels = generate_synthetic_anomaly_data()

    X_train, X_test, y_train, y_test = train_test_split(
        features, labels, test_size=0.2, random_state=42, stratify=labels
    )

    # Isolation Forest
    model = IsolationForest(
        n_estimators=100,
        max_samples=256,
        contamination=0.1,
        random_state=42,
    )
    model.fit(X_train[y_train == 0])  # Train on normal data only

    # Predict: -1 = anomaly, 1 = normal
    y_pred = (model.predict(X_test) == -1).astype(int)
    print(classification_report(y_test, y_pred, target_names=["normal", "anomaly"]))

    import pickle
    os.makedirs(os.path.dirname(save_path), exist_ok=True)
    with open(save_path, "wb") as f:
        pickle.dump(model, f)
    print(f"[SensorAnomaly] Model saved to {save_path}")


if __name__ == "__main__":
    train_sensor_anomaly()
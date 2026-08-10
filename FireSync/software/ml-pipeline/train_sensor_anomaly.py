#!/usr/bin/env python3
"""
FireSync — Sensor Anomaly Detection Training

Isolation Forest for detecting sensor faults across all nodes
(blocked smoke sensor, dead CO cell, thermal array fogged, etc.)
"""
from __future__ import annotations

import os

import numpy as np
from sklearn.ensemble import IsolationForest


def train_sensor_anomaly() -> None:
    """Train Isolation Forest for sensor anomaly detection."""
    print("  Training SensorAnomaly (Isolation Forest)")

    # Features: all telemetry fields across all node types
    # Production: load from 6 months of normal operation data
    # Placeholder: synthetic data
    n_samples = 10000
    n_features = 20  # Aggregated telemetry features

    X_normal = np.random.randn(n_samples, n_features) * 0.5
    # Add some realistic correlations
    X_normal[:, 0] = np.random.uniform(3.0, 4.2, n_samples)  # battery voltage
    X_normal[:, 1] = np.random.uniform(10, 50, n_samples)   # smoke PM2.5
    X_normal[:, 2] = np.random.uniform(0, 10, n_samples)    # CO ppm

    # Train Isolation Forest
    model = IsolationForest(
        n_estimators=100,
        max_samples=256,
        contamination=0.01,  # 1% expected anomalies
        random_state=42,
    )
    model.fit(X_normal)

    # Evaluate with synthetic anomalies
    X_anom = np.random.randn(100, n_features) * 2.0 + 5.0
    y_anom = model.predict(X_anom)
    anomaly_rate = (y_anom == -1).sum() / len(y_anom)

    print(f"\n  SensorAnomaly metrics:")
    print(f"  Anomaly detection rate: {anomaly_rate:.1%} (target: >90%)")

    # Save model
    os.makedirs("models", exist_ok=True)
    import pickle
    with open("models/sensor_anomaly.pkl", "wb") as f:
        pickle.dump(model, f)
    print(f"  Model saved to models/sensor_anomaly.pkl")


if __name__ == "__main__":
    train_sensor_anomaly()
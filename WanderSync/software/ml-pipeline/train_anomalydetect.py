#!/usr/bin/env python3
"""
WanderSync — AnomalyDetect Training Script

Behavioral anomaly detection using Isolation Forest + seasonal decomposition.
Detects unusual patterns (UTI, pain, medication side effects, delirium) —
the #1 cause of sudden behavioral change in dementia.

7-model ML pipeline: model 4 of 7.

Output: Isolation Forest model (cloud deployment).
"""
from __future__ import annotations

import numpy as np
from sklearn.ensemble import IsolationForest
from sklearn.model_selection import train_test_split


def generate_synthetic_data(n_samples: int = 5000):
    """Generate synthetic behavioral feature data (20 features).

    Production: 100,000 person-days of ADL + physiological data with
    labeled anomaly events (UTI, pain, medication reaction, delirium).
    """
    rng = np.random.default_rng(42)

    # 20 features: current vs 7-day baseline ratios
    X = np.ones((n_samples, 20))
    for i in range(n_samples):
        # Normal: ratios around 1.0 with noise
        X[i] = 1.0 + rng.normal(0, 0.15, 20)

    # Inject anomalies (~5%)
    n_anomalies = int(n_samples * 0.05)
    anomaly_indices = rng.choice(n_samples, n_anomalies, replace=False)
    for idx in anomaly_indices:
        # UTI pattern: nighttime activity 340% above baseline, pacing 280%
        X[idx, 0] = 3.4  # nighttime activity ratio
        X[idx, 5] = 2.8  # pacing frequency ratio
        X[idx, 10] = 2.5  # room transitions ratio

    # Labels: 1 = normal, -1 = anomaly
    y = np.ones(n_samples)
    y[anomaly_indices] = -1

    return X, y, anomaly_indices


def train_model():
    X, y, anomaly_idx = generate_synthetic_data(5000)

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y
    )

    model = IsolationForest(
        n_estimators=200,
        contamination=0.05,
        random_state=42,
        n_jobs=-1,
    )

    model.fit(X_train)

    y_pred = model.predict(X_test)

    # Calculate metrics
    tp = np.sum((y_pred == -1) & (y_test == -1))
    fp = np.sum((y_pred == -1) & (y_test == 1))
    fn = np.sum((y_pred == 1) & (y_test == -1))

    precision = tp / (tp + fp) if (tp + fp) > 0 else 0
    recall = tp / (tp + fn) if (tp + fn) > 0 else 0
    f1 = 2 * precision * recall / (precision + recall) if (precision + recall) > 0 else 0

    print(f"AnomalyDetect Isolation Forest trained:")
    print(f"  Precision: {precision:.3f}")
    print(f"  Recall: {recall:.3f}")
    print(f"  F1: {f1:.3f}")
    print(f"  Anomalies detected: {tp}/{np.sum(y_test == -1)}")

    return model


if __name__ == "__main__":
    model = train_model()
    import joblib
    joblib.dump(model, "anomalydetect.joblib")
    print("AnomalyDetect model saved to anomalydetect.joblib")
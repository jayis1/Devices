"""
SightSync ML Pipeline — Blink Anomaly Detector (Isolation Forest)
===================================================================

Trains an isolation forest to detect abnormal blink-rate patterns
indicating dry-eye events or eye strain.

License: MIT
"""

import numpy as np
import joblib
import os
from sklearn.ensemble import IsolationForest
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, roc_auc_score

MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "firmware", "hub", "models")


def generate_synthetic_data(n=10000):
    """Generate synthetic blink-rate windows for training."""
    np.random.seed(42)

    # Normal blink patterns: 12-22 bpm with natural variability
    normal = np.column_stack([
        np.random.normal(15, 4, n // 2),
        np.random.normal(14, 3, n // 2),
        np.random.normal(16, 4, n // 2),
        np.random.normal(15, 3, n // 2),
        np.random.normal(14, 3, n // 2),
    ])

    # Anomalous patterns: very low (<5) or very high (>25) blink rates
    anomalous = np.column_stack([
        np.random.uniform(1, 5, n // 4),
        np.random.uniform(1, 4, n // 4),
        np.random.uniform(2, 6, n // 4),
        np.random.uniform(1, 5, n // 4),
        np.random.uniform(2, 5, n // 4),
    ])

    X = np.vstack([normal, anomalous])
    y = np.array([0] * (n // 2) + [1] * (n // 4))  # 0=normal, 1=anomaly

    return X, y


def train():
    """Train the blink anomaly isolation forest."""
    print("=== SightSync Blink Anomaly Detector Training ===")

    X, y = generate_synthetic_data(10000)

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    model = IsolationForest(
        n_estimators=100,
        contamination=0.25,  # expected anomaly ratio
        random_state=42,
    )
    model.fit(X_train)

    # Evaluate
    y_pred = model.predict(X_test)
    y_pred_binary = (y_pred == -1).astype(int)  # -1 = anomaly

    print(classification_report(y_test, y_pred_binary, target_names=["normal", "anomaly"]))

    try:
        auc = roc_auc_score(y_test, -model.score_samples(X_test))
        print(f"AUROC: {auc:.4f} (target > 0.92)")
    except ValueError:
        print("AUROC not computable (single class in test set)")

    # Save model
    os.makedirs(MODEL_DIR, exist_ok=True)
    model_path = os.path.join(MODEL_DIR, "blink_isoforest.joblib")
    joblib.dump(model, model_path)
    print(f"Model saved: {model_path}")

    return model


if __name__ == "__main__":
    train()
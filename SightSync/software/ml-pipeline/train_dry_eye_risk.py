"""
SightSync ML Pipeline — Dry-Eye Risk Fusion (XGBoost)
======================================================

Fuses blink rate, blink-rate variability, periocular temp delta,
and ambient humidity to predict 24-hour dry-eye risk.

License: MIT
"""

import numpy as np
import pandas as pd
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import roc_auc_score, classification_report
import joblib
import os

MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "firmware", "hub", "models")

FEATURES = ["blink_rate", "blink_rate_variability", "temp_delta_centi", "ambient_humidity"]
TARGET = "dry_eye_risk"  # 0-100


def generate_synthetic_data(n=3000):
    """Generate synthetic dry-eye risk data."""
    np.random.seed(42)
    data = {
        "blink_rate": np.random.uniform(3, 25, n),
        "blink_rate_variability": np.random.uniform(0, 10, n),
        "temp_delta_centi": np.random.uniform(-100, 200, n),  # centi-Celsius
        "ambient_humidity": np.random.uniform(10, 80, n),
    }
    df = pd.DataFrame(data)

    # Heuristic risk
    risk = 0.0
    risk += np.clip((8 - df["blink_rate"]) / 8, 0, 1) * 40
    risk += np.clip(df["blink_rate_variability"] / 10, 0, 1) * 15
    risk += np.clip(df["temp_delta_centi"] / 200, 0, 1) * 30
    risk += np.clip((40 - df["ambient_humidity"]) / 40, 0, 1) * 15
    df[TARGET] = np.clip(risk + np.random.normal(0, 5, n), 0, 100)

    return df


def train():
    print("=== SightSync Dry-Eye Risk Fusion Training ===")

    df = generate_synthetic_data(3000)

    X = df[FEATURES].values
    y = (df[TARGET] > 50).astype(int)  # binary: high risk or not

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    model = xgb.XGBClassifier(
        n_estimators=150,
        max_depth=5,
        learning_rate=0.1,
        subsample=0.8,
        colsample_bytree=0.8,
        random_state=42,
    )
    model.fit(X_train, y_train)

    y_pred = model.predict(X_test)
    y_prob = model.predict_proba(X_test)[:, 1]

    print(classification_report(y_test, y_pred, target_names=["low_risk", "high_risk"]))
    auc = roc_auc_score(y_test, y_prob)
    print(f"AUROC: {auc:.4f} (target > 0.87)")

    os.makedirs(MODEL_DIR, exist_ok=True)
    model_path = os.path.join(MODEL_DIR, "dry_eye_risk.json")
    model.save_model(model_path)
    print(f"Model saved: {model_path}")

    return model


if __name__ == "__main__":
    train()
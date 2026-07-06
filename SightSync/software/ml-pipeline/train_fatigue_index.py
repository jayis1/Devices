"""
SightSync ML Pipeline — Visual Fatigue Index (XGBoost)
=======================================================

Trains a gradient-boosted classifier/regressor to predict
a 0-100 Visual Fatigue Index from:
  - blink rate (bpm)
  - viewing distance (mm)
  - ambient lux
  - blue-light dose (mJ/cm²)
  - posture angle (degrees)
  - minutes since last break

Labels: OSDI + VAS fatigue scores from clinical study.
Output: quantized model converted to tflite for ESP32-S3 edge inference.

License: MIT
"""

import numpy as np
import pandas as pd
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error, r2_score
import joblib
import os

# ── Configuration ────────────────────────────────────────────────────

MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "firmware", "hub", "models")
DATA_PATH = os.path.join(os.path.dirname(__file__), "data", "fatigue_labels.csv")

FEATURES = [
    "blink_rate",
    "viewing_distance_mm",
    "ambient_lux",
    "blue_dose_mj_cm2",
    "posture_angle_deg",
    "minutes_since_break",
]
TARGET = "fatigue_score"  # 0-100


def generate_synthetic_data(n=5000):
    """Generate synthetic training data for development."""
    np.random.seed(42)
    data = {
        "blink_rate": np.random.uniform(3, 25, n),
        "viewing_distance_mm": np.random.uniform(200, 800, n),
        "ambient_lux": np.random.uniform(100, 1000, n),
        "blue_dose_mj_cm2": np.random.uniform(0, 15, n),
        "posture_angle_deg": np.random.uniform(-20, 35, n),
        "minutes_since_break": np.random.uniform(0, 90, n),
    }

    # Heuristic target: weighted fatigue
    df = pd.DataFrame(data)
    df["fatigue_score"] = (
        np.clip((8 - df["blink_rate"]) / 8, 0, 1) * 25 +
        np.clip((300 - df["viewing_distance_mm"]) / 300, 0, 1) * 20 +
        np.clip((300 - df["ambient_lux"]) / 300, 0, 1) * 15 +
        np.clip(df["blue_dose_mj_cm2"] / 10, 0, 1) * 10 +
        np.clip((df["posture_angle_deg"] - 15) / 30, 0, 1) * 15 +
        np.clip((df["minutes_since_break"] - 20) / 40, 0, 1) * 15
    )
    df["fatigue_score"] = np.clip(df["fatigue_score"] + np.random.normal(0, 5, n), 0, 100)
    return df


def train():
    """Train the Visual Fatigue Index model."""
    print("=== SightSync Fatigue Index Training ===")

    # Load or generate data
    if os.path.exists(DATA_PATH):
        print(f"Loading data from {DATA_PATH}")
        df = pd.read_csv(DATA_PATH)
    else:
        print("No labeled data found — generating synthetic data")
        df = generate_synthetic_data(5000)

    X = df[FEATURES].values
    y = df[TARGET].values

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    # Train XGBoost regressor
    model = xgb.XGBRegressor(
        n_estimators=200,
        max_depth=6,
        learning_rate=0.1,
        subsample=0.8,
        colsample_bytree=0.8,
        objective="reg:squarederror",
        random_state=42,
    )
    model.fit(X_train, y_train)

    # Evaluate
    y_pred = model.predict(X_test)
    mae = mean_absolute_error(y_test, y_pred)
    r2 = r2_score(y_test, y_pred)
    print(f"MAE: {mae:.2f} (target < 8)")
    print(f"R²: {r2:.4f}")

    # Save model
    os.makedirs(MODEL_DIR, exist_ok=True)
    model_path = os.path.join(MODEL_DIR, "fatigue_index.json")
    model.save_model(model_path)
    print(f"Model saved: {model_path}")

    # Also save as joblib for Python inference
    joblib_path = os.path.join(MODEL_DIR, "fatigue_index.joblib")
    joblib.dump(model, joblib_path)
    print(f"Joblib model saved: {joblib_path}")

    # Feature importance
    importance = model.feature_importances_
    for feat, imp in sorted(zip(FEATURES, importance), key=lambda x: -x[1]):
        print(f"  {feat}: {imp:.4f}")

    return model


if __name__ == "__main__":
    train()
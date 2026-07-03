"""
DriveSync ML Pipeline — Risk Fusion Model

Fuses all sub-model outputs + OBD context into a unified
0-100 drowsiness risk score using LightGBM.

Inputs:
  - PERCLOS (from eye-closure CNN)
  - Blink rate (from eye-closure CNN)
  - Head-bob count (from head-pose CNN)
  - Steering drowsiness probability (from XGBoost)
  - Grip stability (from FDC2214)
  - HRV drowsiness probability (from LSTM)
  - Body sway amplitude (from belt IMU)
  - Vehicle speed (from OBD-II)
  - Throttle variance (from OBD-II)
  - Time since last break (from hub clock)

Output: 0-100 drowsiness risk score (updated every 5 seconds)

License: MIT
"""

import numpy as np
import os
from lightgbm import LGBMRegressor
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error, r2_score
import joblib

MODEL_SAVE_PATH = "./models/risk_fusion_lgbm.pkl"

FEATURE_NAMES = [
    "perclos",            # 0-1, fraction eyes >80% closed
    "blink_rate",         # blinks/min
    "head_bob_count",     # count in last min
    "steering_drowsy_prob", # 0-1 from XGBoost
    "grip_stability",     # std of grip
    "hrv_drowsy_prob",    # 0-1 from LSTM
    "body_sway_amp",      # milli-g
    "vehicle_speed",      # km/h
    "throttle_variance",  # 0-100²
    "time_since_break",   # seconds
]


# ─────────────────────────────────────────────────────────────────────
# Synthetic Data Generation
# ─────────────────────────────────────────────────────────────────────

def generate_training_data(n_samples=5000):
    """
    Generate synthetic multi-modal training data.
    In production, use real driving study data with expert-labeled drowsiness.
    """
    np.random.seed(42)

    X = np.zeros((n_samples, len(FEATURE_NAMES)))
    y = np.zeros(n_samples)

    for i in range(n_samples):
        # Drowsiness level (0-1)
        drowsiness = np.random.beta(2, 5)

        # Generate correlated features
        X[i, 0] = drowsiness * np.random.uniform(0.3, 0.5)  # PERCLOS
        X[i, 1] = max(0, 20 - drowsiness * 15 + np.random.normal(0, 3))  # Blink rate decreases
        X[i, 2] = drowsiness * np.random.poisson(5)  # Head-bob count
        X[i, 3] = drowsiness * np.random.uniform(0.6, 0.9)  # Steering prob
        X[i, 4] = max(0, 200 - drowsiness * 100 + np.random.normal(0, 30))
        X[i, 5] = drowsiness * np.random.uniform(0.5, 0.9)  # HRV prob
        X[i, 6] = drowsiness * np.random.uniform(200, 800)  # Body sway
        X[i, 7] = np.random.uniform(0, 120)  # Vehicle speed
        X[i, 8] = np.random.uniform(0, 500)  # Throttle variance
        X[i, 9] = np.random.uniform(0, 14400)  # Time since break (max 4 hours)

        # Target: 0-100 risk score
        y[i] = min(100, drowsiness * 100 + np.random.normal(0, 5))

    return X, y


# ─────────────────────────────────────────────────────────────────────
# Training
# ─────────────────────────────────────────────────────────────────────

def train():
    print("=" * 60)
    print("DriveSync — Risk Fusion Model Training (LightGBM)")
    print("=" * 60)

    X, y = generate_training_data()
    print(f"Dataset: {len(X)} samples")
    print(f"Features: {FEATURE_NAMES}")

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42
    )

    model = LGBMRegressor(
        n_estimators=200,
        max_depth=6,
        learning_rate=0.05,
        num_leaves=31,
        subsample=0.8,
        colsample_bytree=0.8,
        reg_alpha=0.1,
        reg_lambda=0.1,
    )

    model.fit(
        X_train, y_train,
        eval_set=[(X_test, y_test)],
        eval_metric="rmse",
    )

    # Evaluate
    y_pred = model.predict(X_test)
    mse = mean_squared_error(y_test, y_pred)
    r2 = r2_score(y_test, y_pred)

    print(f"\nRMSE: {np.sqrt(mse):.2f}")
    print(f"R²:   {r2:.4f}")

    # Feature importance
    print("\nFeature Importance:")
    importances = model.feature_importances_
    for name, imp in sorted(zip(FEATURE_NAMES, importances), key=lambda x: -x[1]):
        print(f"  {name:30s} {imp:.0f}")

    # Save model
    os.makedirs(os.path.dirname(MODEL_SAVE_PATH), exist_ok=True)
    joblib.dump(model, MODEL_SAVE_PATH)
    print(f"\nModel saved to {MODEL_SAVE_PATH}")

    # Export feature weights for edge deployment (simplified linear fusion)
    export_edge_fusion_weights(model)


def export_edge_fusion_weights(model):
    """Export simplified fusion weights for ESP32-S3 edge deployment."""
    importances = model.feature_importances_
    total = sum(importances)
    weights = [imp / total for imp in importances]

    weights_path = "./models/edge_fusion_weights.h"
    with open(weights_path, "w") as f:
        f.write("/* Auto-generated edge fusion weights */\n")
        f.write("/* Do not edit — regenerate with train_risk_fusion.py */\n\n")
        f.write("#ifndef EDGE_FUSION_WEIGHTS_H\n#define EDGE_FUSION_WEIGHTS_H\n\n")
        f.write("static const float fusion_weights[] = {\n")
        for name, w in zip(FEATURE_NAMES, weights):
            f.write(f"    {w:.6f}f,  /* {name} */\n")
        f.write("};\n\n")
        f.write(f"#define NUM_FUSION_FEATURES {len(weights)}\n\n")
        f.write("#endif\n")

    print(f"Edge fusion weights: {weights_path}")


if __name__ == "__main__":
    train()
"""
FlowGuard - Freeze Risk Prediction Model Training
Trains an XGBoost model for pipe freeze risk prediction.

Input: Weather forecast (7-day), pipe temperature history, home insulation model, occupancy pattern
Output: Freeze risk score (0-1) per pipe sensor location, updated hourly

Uses gradient-boosted decision trees (XGBoost) for:
- Fast inference (<1ms per prediction)
- Interpretable feature importance
- Handles missing weather data gracefully
- Good performance on tabular/time-series features

Copyright (c) 2026 jayis1 - MIT License
"""

import numpy as np
import pandas as pd
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import roc_auc_score, classification_report
from pathlib import Path
import argparse
import json

# ============================================================
# Feature Engineering
# ============================================================

FEATURE_COLUMNS = [
    # Current conditions (from pipe sensor)
    "pipe_temp_c",              # Current pipe surface temperature
    "pipe_temp_1h_ago",         # Temperature 1 hour ago
    "pipe_temp_6h_ago",         # Temperature 6 hours ago
    "pipe_temp_24h_ago",        # Temperature 24 hours ago
    "ambient_humidity",         # Ambient humidity near pipe

    # Weather forecast (from API)
    "forecast_temp_0h",         # Current outside temperature
    "forecast_temp_3h",         # 3-hour forecast temperature
    "forecast_temp_6h",         # 6-hour forecast temperature
    "forecast_temp_12h",        # 12-hour forecast temperature
    "forecast_temp_24h",       # 24-hour forecast temperature
    "forecast_temp_48h",       # 48-hour forecast temperature
    "forecast_wind_speed",      # Current wind speed (wind chill factor)
    "forecast_wind_3h",        # 3-hour forecast wind speed
    "forecast_snow_depth",      # Snow depth (insulation factor)
    "forecast_precip_prob",     # Precipitation probability

    # Home context
    "is_occupied",              # Is anyone home? (affects heating)
    "indoor_temp",              # Indoor temperature (from HVAC or thermostat)
    "pipe_location",            # 0=indoor, 1=garage, 2=crawlspace, 3=exterior wall, 4=basement
    "insulation_rating",        # 1-5 insulation quality
    "heat_trace_installed",     # Is heat trace on this pipe? (boolean)

    # Derived features
    "temp_rate_1h",             # Temperature change rate (°C/hour)
    "temp_rate_6h",             # Temperature change rate over 6 hours
    "wind_chill",               # Wind chill temperature
    "freeze_margin",             # How far above freezing (pipe_temp - 0)
    "hours_below_5c_24h",      # Hours pipe was below 5°C in last 24h
    "hours_below_0c_24h",      # Hours pipe was below 0°C in last 24h
]

TARGET_COLUMN = "froze_within_48h"  # Binary: did pipe freeze within 48 hours?


def engineer_features(df: pd.DataFrame) -> pd.DataFrame:
    """Create derived features from raw data."""
    # Temperature change rates
    df["temp_rate_1h"] = df["pipe_temp_c"] - df["pipe_temp_1h_ago"]
    df["temp_rate_6h"] = (df["pipe_temp_c"] - df["pipe_temp_6h_ago"]) / 6.0

    # Wind chill (simplified formula)
    df["wind_chill"] = (
        13.12 + 0.6215 * df["forecast_temp_0h"]
        - 11.37 * (df["forecast_wind_speed"] ** 0.16)
        + 0.3965 * df["forecast_temp_0h"] * (df["forecast_wind_speed"] ** 0.16)
    )

    # Freeze margin (how far above freezing)
    df["freeze_margin"] = df["pipe_temp_c"]

    # Hours below threshold (pre-computed in data pipeline)
    # hours_below_5c_24h and hours_below_0c_24h are already in features

    return df


# ============================================================
# Training
# ============================================================

def train_model(args):
    """Train the freeze risk prediction model."""

    print(f"Loading data from {args.data_dir}...")
    df = pd.read_csv(Path(args.data_dir) / "freeze_training_data.csv")

    # Engineer features
    df = engineer_features(df)

    # Split features and target
    X = df[FEATURE_COLUMNS].values
    y = df[TARGET_COLUMN].values

    # Train/test split (time-based: use last 20% as test)
    split_idx = int(len(X) * 0.8)
    X_train, X_test = X[:split_idx], X[split_idx:]
    y_train, y_test = y[:split_idx], y[split_idx:]

    print(f"Training set: {len(X_train)} samples ({y_train.sum()} freeze events)")
    print(f"Test set: {len(X_test)} samples ({y_test.sum()} freeze events)")

    # Handle class imbalance (freeze events are rare)
    scale_pos_weight = (len(y_train) - y_train.sum()) / (y_train.sum() + 1)

    # XGBoost model
    model = xgb.XGBClassifier(
        n_estimators=500,
        max_depth=6,
        learning_rate=0.05,
        subsample=0.8,
        colsample_bytree=0.8,
        min_child_weight=5,
        scale_pos_weight=scale_pos_weight,
        eval_metric="auc",
        early_stopping_rounds=50,
        random_state=42,
    )

    model.fit(
        X_train, y_train,
        eval_set=[(X_test, y_test)],
        verbose=True
    )

    # Evaluate
    y_pred_proba = model.predict_proba(X_test)[:, 1]
    y_pred = model.predict(X_test)

    auc = roc_auc_score(y_test, y_pred_proba)
    print(f"\nTest AUC: {auc:.4f}")
    print("\nClassification Report:")
    print(classification_report(y_test, y_pred, target_names=["No Freeze", "Freeze"]))

    # Feature importance
    importance = model.feature_importances_
    print("\nFeature Importance (top 10):")
    sorted_idx = np.argsort(importance)[::-1]
    for i in range(min(10, len(sorted_idx))):
        print(f"  {FEATURE_COLUMNS[sorted_idx[i]]}: {importance[sorted_idx[i]]:.4f}")

    # Save model
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    model.save_model(str(output_dir / "freeze_risk_model.json"))

    # Save feature columns for inference
    with open(output_dir / "feature_columns.json", "w") as f:
        json.dump(FEATURE_COLUMNS, f, indent=2)

    # Save model config for inference
    config = {
        "model_type": "xgboost",
        "num_features": len(FEATURE_COLUMNS),
        "freeze_threshold": args.freeze_threshold,
        "prediction_horizon_hours": 48,
        "update_interval_minutes": 60,
    }
    with open(output_dir / "model_config.json", "w") as f:
        json.dump(config, f, indent=2)

    print(f"\nModel saved to {output_dir}")
    print(f"Freeze threshold: {args.freeze_threshold} (risk > threshold → enable heat trace)")

    return model


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train FlowGuard freeze risk prediction model")
    parser.add_argument("--data_dir", type=str, default="data/freeze",
                        help="Directory containing training data CSV")
    parser.add_argument("--output_dir", type=str, default="models",
                        help="Output directory for saved models")
    parser.add_argument("--freeze_threshold", type=float, default=0.5,
                        help="Risk score threshold to enable heat trace (0-1)")

    args = parser.parse_args()
    model = train_model(args)
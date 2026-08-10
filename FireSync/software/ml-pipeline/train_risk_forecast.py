#!/usr/bin/env python3
"""
FireSync — RiskForecast Training Script

XGBoost regressor for 7-day fire risk forecasting.
Predicts fire risk score (0-100) from electrical load patterns,
ambient conditions, cooking frequency, and seasonal factors.
"""
from __future__ import annotations

import os
import sys

import numpy as np
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import roc_auc_score, mean_squared_error


def train_risk_forecast(epochs: int = 50) -> None:
    """Train XGBoost fire risk forecaster."""
    print("  Training RiskForecast (XGBoost Regressor)")

    n_features = 24
    # Production: load from NFIRS fire incident data + telemetry history
    # Placeholder: synthetic data
    n_samples = 10000
    X = np.random.randn(n_samples, n_features)
    # Make some features correlate with fire risk
    # Features: peak_kW, total_kWh, ambient_temp, humidity, cooking_events,
    #           stove_hours, stove_max_temp, panel_max_temp, panel_max_current,
    #           arcing_events, co_baseline, week_of_year, day_of_week, holiday,
    #           heating_degree_days, etc.
    y = (
        X[:, 0] * 0.3 +    # peak kW
        X[:, 1] * 0.2 +    # total kWh
        X[:, 3] * (-0.15) + # humidity (negative: low humidity = higher risk)
        X[:, 5] * 0.2 +    # stove hours
        X[:, 6] * 0.25 +   # stove max temp
        X[:, 7] * 0.2 +    # panel max temp
        X[:, 9] * 0.3 +    # arcing events
        np.random.randn(n_samples) * 0.1
    )
    # Convert to risk score 0-100
    y = np.clip((y - y.min()) / (y.max() - y.min()) * 100, 0, 100)
    y_binary = (y > 60).astype(int)  # High risk threshold

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42
    )
    _, _, y_bin_train, y_bin_test = train_test_split(
        X, y_binary, test_size=0.2, random_state=42
    )

    model = xgb.XGBRegressor(
        n_estimators=500,
        max_depth=6,
        learning_rate=0.01,
        subsample=0.8,
        colsample_bytree=0.8,
        reg_alpha=0.1,
        reg_lambda=1.0,
        random_state=42,
    )
    model.fit(X_train, y_train, eval_set=[(X_test, y_test)],
              verbose=False, early_stopping_rounds=20)

    # Evaluate
    y_pred = model.predict(X_test)
    rmse = np.sqrt(mean_squared_error(y_test, y_pred))
    y_bin_pred = (model.predict(X_test) > 60).astype(int)

    # AUC for binary classification
    from sklearn.metrics import roc_auc_score
    auc = roc_auc_score(y_bin_test, y_pred)

    print(f"\n  RiskForecast metrics:")
    print(f"  RMSE: {rmse:.2f} (risk score 0-100)")
    print(f"  AUC (high risk classifier): {auc:.3f}")
    print(f"  Target AUC: >0.85")

    # Feature importance (SHAP)
    print(f"\n  Top features (importance):")
    feature_names = [
        "peak_kW", "total_kWh", "ambient_temp", "humidity", "cooking_events",
        "stove_hours", "stove_max_temp", "panel_max_temp", "panel_max_current",
        "arcing_events", "co_baseline", "week_of_year", "day_of_week", "holiday",
        "heating_degree_days", "cooling_degree_days", "wind_speed", "precip_7d",
        "uv_index", "occupancy_hours", "door_open_count", "window_open_hours",
        "hvac_runtime", "previous_fire_events",
    ]
    importances = model.feature_importances_
    indices = np.argsort(importances)[::-1]
    for i in range(5):
        idx = indices[i]
        print(f"    {feature_names[idx]}: {importances[idx]:.3f}")

    # Save model
    os.makedirs("models", exist_ok=True)
    model.save_model("models/risk_forecast.json")
    print(f"\n  RiskForecast model saved to models/risk_forecast.json")


if __name__ == "__main__":
    train_risk_forecast()
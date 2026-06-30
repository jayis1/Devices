"""
AsthmaSync — Train Trigger Identifier (XGBoost + SHAP)
=======================================================
Identifies personal asthma triggers using gradient boosting
and SHAP (SHapley Additive exPlanations) for per-variable
attribution.

Architecture:
  - Input: hourly aggregated features (PM2.5, VOC, CO₂, temp, humidity,
           HRV, activity, time-of-day, day-of-week)
  - Target: binary (wheeze event within ±2 hours)
  - Model: XGBoost classifier (200 trees, depth 4)
  - Explanation: SHAP TreeExplainer for per-prediction attribution

License: MIT
"""

import numpy as np
import pandas as pd
import xgboost as xgb
import shap
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, roc_auc_score
import joblib
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# ── Feature columns ──────────────────────────────────────
FEATURE_COLS = [
    "pm25", "voc_index", "co2_ppm", "temp_c", "humidity_pct",
    "hr", "hrv_rmssd", "spo2", "activity",
    "hour_of_day", "day_of_week", "is_weekend",
    "pm25_3h_avg", "voc_3h_avg", "co2_3h_avg",
    "pm25_delta_1h", "voc_delta_1h", "temp_delta_1h",
]

TRIGGER_LABELS = {
    "pm25": "PM2.5 (fine particles)",
    "voc_index": "VOCs (volatile organic compounds)",
    "co2_ppm": "CO₂ (carbon dioxide)",
    "temp_c": "Temperature",
    "humidity_pct": "Humidity",
}


def generate_synthetic_data(n_samples=20000):
    """Generate synthetic hourly data with trigger-symptom correlations."""
    np.random.seed(42)

    data = pd.DataFrame()
    data["timestamp"] = pd.date_range("2024-01-01", periods=n_samples, freq="1h")
    data["hour_of_day"] = data["timestamp"].dt.hour
    data["day_of_week"] = data["timestamp"].dt.dayofweek
    data["is_weekend"] = (data["day_of_week"] >= 5).astype(int)

    # Environmental features (with diurnal patterns)
    data["pm25"] = 15 + 10 * np.sin(data["hour_of_day"] * np.pi / 12) + np.random.normal(0, 5, n_samples)
    data["pm25"] = np.clip(data["pm25"], 0, 100)
    data["voc_index"] = 150 + 50 * np.sin(data["hour_of_day"] * np.pi / 12 + 1) + np.random.normal(0, 30, n_samples)
    data["voc_index"] = np.clip(data["voc_index"], 0, 500)
    data["co2_ppm"] = 500 + 200 * np.sin(data["hour_of_day"] * np.pi / 12) + np.random.normal(0, 50, n_samples)
    data["co2_ppm"] = np.clip(data["co2_ppm"], 400, 5000)
    data["temp_c"] = 22 + 3 * np.sin(data["hour_of_day"] * np.pi / 12) + np.random.normal(0, 1, n_samples)
    data["humidity_pct"] = 45 + 10 * np.sin(data["hour_of_day"] * np.pi / 12 + 2) + np.random.normal(0, 3, n_samples)
    data["humidity_pct"] = np.clip(data["humidity_pct"], 10, 90)

    # Vitals
    data["hr"] = 70 + np.random.normal(0, 8, n_samples)
    data["hrv_rmssd"] = 35 + np.random.normal(0, 10, n_samples)
    data["spo2"] = 97 + np.random.normal(0, 1, n_samples)
    data["spo2"] = np.clip(data["spo2"], 90, 100)
    data["activity"] = np.random.choice([0, 1, 2, 3], n_samples, p=[0.5, 0.3, 0.15, 0.05])

    # Rolling averages
    data["pm25_3h_avg"] = data["pm25"].rolling(3, min_periods=1).mean()
    data["voc_3h_avg"] = data["voc_index"].rolling(3, min_periods=1).mean()
    data["co2_3h_avg"] = data["co2_ppm"].rolling(3, min_periods=1).mean()
    data["pm25_delta_1h"] = data["pm25"].diff().fillna(0)
    data["voc_delta_1h"] = data["voc_index"].diff().fillna(0)
    data["temp_delta_1h"] = data["temp_c"].diff().fillna(0)

    # Generate labels: wheeze when triggers are high
    # Weighted combination of trigger variables
    trigger_score = (
        (data["pm25"] > 35).astype(float) * 0.30 +
        (data["voc_index"] > 400).astype(float) * 0.25 +
        (data["co2_ppm"] > 1000).astype(float) * 0.15 +
        (data["temp_c"] > 26).astype(float) * 0.10 +
        (data["humidity_pct"] > 60).astype(float) * 0.10 +
        (data["hrv_rmssd"] < 25).astype(float) * 0.10
    )
    # Add noise
    trigger_score += np.random.normal(0, 0.1, n_samples)
    data["wheeze"] = (trigger_score > 0.4).astype(int)

    return data


def train_trigger_model():
    logger.info("Generating synthetic trigger data...")
    data = generate_synthetic_data(n_samples=50000)

    X = data[FEATURE_COLS].values
    y = data["wheeze"].values

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y)

    logger.info(f"Training set: {len(X_train)} samples ({sum(y_train)} positive)")
    logger.info(f"Test set: {len(X_test)} samples ({sum(y_test)} positive)")

    # Train XGBoost
    model = xgb.XGBClassifier(
        n_estimators=200,
        max_depth=4,
        learning_rate=0.1,
        subsample=0.8,
        colsample_bytree=0.8,
        reg_alpha=0.1,
        reg_lambda=1.0,
        scale_pos_weight=len(y_train[y_train == 0]) / max(len(y_train[y_train == 1]), 1),
        random_state=42,
        eval_metric="aucpr",
    )

    model.fit(
        X_train, y_train,
        eval_set=[(X_test, y_test)],
        verbose=False,
    )

    # Evaluate
    y_pred = model.predict(X_test)
    y_prob = model.predict_proba(X_test)[:, 1]
    auc = roc_auc_score(y_test, y_prob)

    logger.info(f"\n{classification_report(y_test, y_pred)}")
    logger.info(f"AUC-ROC: {auc:.4f}")

    # SHAP analysis
    logger.info("Computing SHAP values...")
    explainer = shap.TreeExplainer(model)
    shap_values = explainer.shap_values(X_test[:1000])

    # Feature importance
    importance = np.abs(shap_values).mean(axis=0)
    importance_df = pd.DataFrame({
        "feature": FEATURE_COLS,
        "importance": importance,
    }).sort_values("importance", ascending=False)

    logger.info("\nTrigger importance (SHAP):")
    for _, row in importance_df.iterrows():
        label = TRIGGER_LABELS.get(row["feature"], row["feature"])
        logger.info(f"  {label:40s} {row['importance']:.4f}")

    # Save model
    joblib.dump({
        "model": model,
        "feature_cols": FEATURE_COLS,
        "shap_explainer": explainer,
    }, "trigger_xgb.pkl")
    logger.info("Model saved: trigger_xgb.pkl")

    return model, explainer


if __name__ == "__main__":
    train_trigger_model()
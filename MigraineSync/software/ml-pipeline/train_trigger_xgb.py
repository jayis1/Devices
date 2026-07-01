"""
MigraineSync — Personal Trigger Identifier (XGBoost + SHAP)
============================================================
Trains an XGBoost classifier to identify which triggers contribute
to migraine onset for each individual patient. Uses SHAP values
for per-trigger attribution.

Input:  7-day feature matrix (aggregate features per day)
Output: Per-trigger contribution % for migraine prediction

License: MIT
"""

import os
import numpy as np
import pandas as pd
import xgboost as xgb
import shap
from sklearn.model_selection import train_test_split
from sklearn.metrics import roc_auc_score, classification_report
import joblib

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")
MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "models")
os.makedirs(MODEL_DIR, exist_ok=True)

# ── Daily aggregate features ─────────────────────────────
FEATURE_COLUMNS = [
    "avg_hrv", "hrv_decline_pct", "min_hrv",
    "pressure_variability", "max_pressure_drop",
    "avg_hydration", "min_hydration",
    "avg_light", "max_light",
    "avg_skin_temp", "skin_temp_slope",
    "avg_activity", "max_activity",
    "sleep_score",
]


def compute_daily_features(df: pd.DataFrame) -> pd.DataFrame:
    """Compute daily aggregate features from raw 5-min data."""
    df = df.copy()
    df["date"] = pd.to_datetime(df["timestamp"]).dt.date

    daily = df.groupby(["patient_id", "date"]).agg(
        avg_hrv=("hrv_rmssd", "mean"),
        min_hrv=("hrv_rmssd", "min"),
        avg_hydration=("hydration_ml", "mean"),
        min_hydration=("hydration_ml", "min"),
        avg_light=("light_lux", "mean"),
        max_light=("light_lux", "max"),
        avg_skin_temp=("skin_temp_c", "mean"),
        avg_activity=("activity", "mean"),
        max_activity=("activity", "max"),
        pressure_variability=("pressure_hpa", "std"),
        max_pressure_drop=("pressure_delta_3h", lambda x: abs(x.min())),
        migraine_today=("migraine_onset", "max"),
    ).reset_index()

    # Compute derived features
    baseline_hrv = df.groupby("patient_id")["hrv_rmssd"].transform("mean")
    daily["hrv_decline_pct"] = ((daily["avg_hrv"] - daily.groupby("patient_id")["avg_hrv"].transform("mean")) /
                                 daily.groupby("patient_id")["avg_hrv"].transform("mean")) * 100

    # Skin temp slope (last vs first reading of day)
    daily["skin_temp_slope"] = 0.0  # simplified

    # Sleep score from overnight HRV (simplified)
    daily["sleep_score"] = 50 + daily["avg_hrv"] * 0.5
    daily["sleep_score"] = daily["sleep_score"].clip(0, 100)

    # Target: migraine in next 2 days
    daily = daily.sort_values(["patient_id", "date"])
    daily["migraine_next_2d"] = daily.groupby("patient_id")["migraine_today"].shift(-1).fillna(0) | \
                                 daily.groupby("patient_id")["migraine_today"].shift(-2).fillna(0)
    daily["migraine_next_2d"] = daily["migraine_next_2d"].astype(int)

    return daily


def train():
    """Train XGBoost trigger identifier with SHAP."""
    print("Loading data...")
    data_path = os.path.join(DATA_DIR, "synthetic_migraine_data.csv")
    df = pd.read_csv(data_path)

    print("Computing daily features...")
    daily = compute_daily_features(df)

    # Prepare features and labels
    X = daily[FEATURE_COLUMNS].values
    y = daily["migraine_next_2d"].values

    print(f"Dataset: {len(X)} samples, positive: {y.mean():.1%}")

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    # Train XGBoost
    print("Training XGBoost...")
    model = xgb.XGBClassifier(
        n_estimators=200,
        max_depth=6,
        learning_rate=0.1,
        subsample=0.8,
        colsample_bytree=0.8,
        random_state=42,
        eval_metric="auc",
    )
    model.fit(X_train, y_train, eval_set=[(X_test, y_test)], verbose=False)

    # Evaluate
    y_pred = model.predict_proba(X_test)[:, 1]
    auc = roc_auc_score(y_test, y_pred)
    print(f"ROC AUC: {auc:.4f}")
    print(classification_report(y_test, model.predict(X_test),
                                target_names=["no migraine", "migraine"]))

    # SHAP analysis
    print("Computing SHAP values...")
    explainer = shap.TreeExplainer(model)
    shap_values = explainer.shap_values(X_test)

    # Feature importance (mean absolute SHAP)
    mean_shap = np.abs(shap_values).mean(axis=0)
    feature_importance = list(zip(FEATURE_COLUMNS, mean_shap))
    feature_importance.sort(key=lambda x: x[1], reverse=True)

    print("\nFeature importance (SHAP):")
    for feat, imp in feature_importance:
        print(f"  {feat:25s} {imp:.4f}")

    # Save model + explainer
    joblib.dump(model, os.path.join(MODEL_DIR, "trigger_xgb.pkl"))
    joblib.dump(explainer, os.path.join(MODEL_DIR, "trigger_shap_explainer.pkl"))
    np.save(os.path.join(MODEL_DIR, "trigger_feature_columns.npy"), FEATURE_COLUMNS)

    print(f"\nModel saved: {os.path.join(MODEL_DIR, 'trigger_xgb.pkl')}")
    print("Done!")


if __name__ == "__main__":
    train()
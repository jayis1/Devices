"""
MigraineSync — Sleep Quality Regressor (Gradient Boosted)
==========================================================
Trains a Gradient Boosted Regressor to predict sleep quality
(0-100) from overnight HRV features.

Input:  Overnight HRV features (RMSSD, SDNN, pNN50, mean HR, HR std)
Output: Sleep quality score (0-100)

License: MIT
"""

import os
import numpy as np
import pandas as pd
from sklearn.ensemble import GradientBoostingRegressor
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error, r2_score
import joblib

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")
MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "models")
os.makedirs(MODEL_DIR, exist_ok=True)


def compute_sleep_features(df: pd.DataFrame) -> pd.DataFrame:
    """Compute overnight HRV features → sleep quality score."""
    df = df.copy()
    df["timestamp"] = pd.to_datetime(df["timestamp"])
    df["date"] = df["timestamp"].dt.date
    df["hour"] = df["timestamp"].dt.hour

    # Filter overnight (22:00 - 07:00)
    overnight = df[(df["hour"] >= 22) | (df["hour"] < 7)].copy()

    # Group by patient + date
    sleep = overnight.groupby(["patient_id", "date"]).agg(
        rmssd_mean=("hrv_rmssd", "mean"),
        rmssd_std=("hrv_rmssd", "std"),
        rmssd_min=("hrv_rmssd", "min"),
        rmssd_max=("hrv_rmssd", "max"),
        hr_mean=("hrv_rmssd", lambda x: 60000 / x.mean() if x.mean() > 0 else 60),
        hr_std=("hrv_rmssd", lambda x: 60000 / x.std() if x.std() > 0 else 5),
        n_samples=("hrv_rmssd", "count"),
    ).reset_index()

    # pNN50: proportion of successive differences > 50ms
    def calc_pnn50(group):
        diffs = np.abs(np.diff(group))
        if len(diffs) == 0:
            return 0
        return np.mean(diffs > 50) * 100

    pnn50 = overnight.groupby(["patient_id", "date"])["hrv_rmssd"].apply(calc_pnn50)
    sleep["pnn50"] = sleep.set_index(["patient_id", "date"]).index.map(pnn50).fillna(0)

    # Sleep quality score (heuristic: higher HRV = better sleep)
    sleep["sleep_score"] = (
        50
        + sleep["rmssd_mean"] * 0.8
        + sleep["pnn50"] * 0.2
        - sleep["hr_std"] * 0.5
    ).clip(0, 100)

    return sleep


def train():
    print("Loading data...")
    data_path = os.path.join(DATA_DIR, "synthetic_migraine_data.csv")
    df = pd.read_csv(data_path)

    print("Computing sleep features...")
    sleep = compute_sleep_features(df)

    features = ["rmssd_mean", "rmssd_std", "rmssd_min", "rmssd_max",
                "hr_mean", "hr_std", "pnn50", "n_samples"]
    X = sleep[features].values
    y = sleep["sleep_score"].values

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    print("Training Gradient Boosted Regressor...")
    model = GradientBoostingRegressor(
        n_estimators=200, max_depth=5, learning_rate=0.1, random_state=42
    )
    model.fit(X_train, y_train)

    y_pred = model.predict(X_test)
    mae = mean_absolute_error(y_test, y_pred)
    r2 = r2_score(y_test, y_pred)
    print(f"MAE: {mae:.2f}, R²: {r2:.4f}")

    # Feature importance
    importances = list(zip(features, model.feature_importances_))
    importances.sort(key=lambda x: x[1], reverse=True)
    print("\nFeature importance:")
    for feat, imp in importances:
        print(f"  {feat:20s} {imp:.4f}")

    joblib.dump(model, os.path.join(MODEL_DIR, "sleep_quality_gbr.pkl"))
    np.save(os.path.join(MODEL_DIR, "sleep_features.npy"), features)
    print(f"\nModel saved: {os.path.join(MODEL_DIR, 'sleep_quality_gbr.pkl')}")
    print("Done!")


if __name__ == "__main__":
    train()
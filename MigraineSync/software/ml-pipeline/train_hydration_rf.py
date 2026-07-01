"""
MigraineSync — Hydration Pattern Classifier (Random Forest)
============================================================
Trains a Random Forest to classify hydration patterns as
adequate / low / dehydrated based on intake timing + volume.

Input:  Daily hydration features (intake, sip count, timing, deficit)
Output: 3-class: adequate / low / dehydrated

License: MIT
"""

import os
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report
import joblib

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")
MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "models")
os.makedirs(MODEL_DIR, exist_ok=True)

DAILY_GOAL_ML = 2000


def compute_hydration_features(df: pd.DataFrame) -> pd.DataFrame:
    """Compute daily hydration features."""
    df = df.copy()
    df["date"] = pd.to_datetime(df["timestamp"]).dt.date
    df["hour"] = pd.to_datetime(df["timestamp"]).dt.hour

    daily = df.groupby(["patient_id", "date"]).agg(
        total_intake=("hydration_ml", "max"),
        avg_intake=("hydration_ml", "mean"),
        n_samples=("hydration_ml", "count"),
    ).reset_index()

    # Estimate sip count from volume changes
    df_sorted = df.sort_values(["patient_id", "timestamp"])
    df_sorted["vol_delta"] = df_sorted.groupby("patient_id")["hydration_ml"].diff()
    sips = df_sorted[df_sorted["vol_delta"] > 5].groupby(["patient_id", "date"]).size()
    daily["sip_count"] = daily.set_index(["patient_id", "date"]).index.map(sips).fillna(0)

    # Timing features
    morning = df_sorted[(df_sorted["hour"] < 12)].groupby(["patient_id", "date"])["hydration_ml"].max()
    afternoon = df_sorted[(df_sorted["hour"] >= 12) & (df_sorted["hour"] < 18)].groupby(["patient_id", "date"])["hydration_ml"].max()
    evening = df_sorted[(df_sorted["hour"] >= 18)].groupby(["patient_id", "date"])["hydration_ml"].max()

    daily["morning_intake"] = daily.set_index(["patient_id", "date"]).index.map(morning).fillna(0)
    daily["afternoon_intake"] = daily.set_index(["patient_id", "date"]).index.map(afternoon).fillna(0)
    daily["evening_intake"] = daily.set_index(["patient_id", "date"]).index.map(evening).fillna(0)

    # Deficit
    daily["deficit_ml"] = DAILY_GOAL_ML - daily["total_intake"]
    daily["deficit_pct"] = (daily["deficit_ml"] / DAILY_GOAL_ML * 100).clip(-50, 100)

    # Classify: adequate / low / dehydrated
    daily["hydration_class"] = pd.cut(
        daily["total_intake"],
        bins=[-1, 800, 1500, float("inf")],
        labels=[2, 1, 0]  # 0=adequate, 1=low, 2=dehydrated
    ).astype(int)

    return daily


def train():
    print("Loading data...")
    data_path = os.path.join(DATA_DIR, "synthetic_migraine_data.csv")
    df = pd.read_csv(data_path)

    print("Computing hydration features...")
    daily = compute_hydration_features(df)

    features = ["total_intake", "avg_intake", "sip_count",
                "morning_intake", "afternoon_intake", "evening_intake",
                "deficit_ml", "deficit_pct"]
    X = daily[features].values
    y = daily["hydration_class"].values

    print(f"Dataset: {len(X)} samples")
    unique, counts = np.unique(y, return_counts=True)
    for u, c in zip(unique, counts):
        labels = {0: "adequate", 1: "low", 2: "dehydrated"}
        print(f"  {labels[u]}: {c} ({c/len(y):.1%})")

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    print("Training Random Forest...")
    model = RandomForestClassifier(n_estimators=100, max_depth=10, random_state=42)
    model.fit(X_train, y_train)

    y_pred = model.predict(X_test)
    print(classification_report(y_test, y_pred,
                                target_names=["adequate", "low", "dehydrated"]))

    # Feature importance
    importances = list(zip(features, model.feature_importances_))
    importances.sort(key=lambda x: x[1], reverse=True)
    print("\nFeature importance:")
    for feat, imp in importances:
        print(f"  {feat:20s} {imp:.4f}")

    joblib.dump(model, os.path.join(MODEL_DIR, "hydration_rf.pkl"))
    print(f"\nModel saved: {os.path.join(MODEL_DIR, 'hydration_rf.pkl')}")
    print("Done!")


if __name__ == "__main__":
    train()
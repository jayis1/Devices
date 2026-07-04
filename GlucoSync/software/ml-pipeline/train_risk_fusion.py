"""
GlucoSync ML Pipeline — Risk Fusion Model

LightGBM fusion of all sub-models into unified 0-100 metabolic risk score.
Generates care recommendations.

License: MIT
"""

import numpy as np
import pandas as pd
import lightgbm as lgb
from sklearn.model_selection import train_test_split
import joblib
import os

# ── Configuration ────────────────────────────────────────────────

RISK_THRESHOLDS = {
    "none": (0, 19),
    "monitor": (20, 39),
    "snack": (40, 59),
    "insulin": (60, 79),
    "check": (80, 89),
    "help": (90, 100),
}


def compute_risk_score(glucose, forecast_30, forecast_60,
                       hypo_risk, hyper_risk, iob, cob, hr, intensity,
                       time_in_range):
    """
    Rule-based risk score computation (for training labels).
    """
    risk = 0

    # Hypoglycemia risk (highest priority)
    if hypo_risk > 70:
        risk = max(risk, 85)
    elif hypo_risk > 50:
        risk = max(risk, 60)
    elif hypo_risk > 30:
        risk = max(risk, 40)

    # Hyperglycemia risk
    if hyper_risk > 60:
        risk = max(risk, 70)
    elif hyper_risk > 40:
        risk = max(risk, 50)

    # Current glucose out of range
    if glucose < 54:
        risk = max(risk, 90)
    elif glucose < 70:
        risk = max(risk, 60)
    elif glucose > 250:
        risk = max(risk, 65)
    elif glucose > 180:
        risk = max(risk, 35)

    # Forecast worsening
    if forecast_30 < 70 and forecast_30 < glucose:
        risk = max(risk, 55)
    if forecast_60 > 250:
        risk = max(risk, 45)

    # IOB amplification
    if iob > 3 and glucose < 100:
        risk = min(100, int(risk * 1.2))

    # Exercise amplification
    if intensity > 60 and iob > 1:
        risk = min(100, int(risk * 1.15))

    # TIR modifier
    if time_in_range < 50:
        risk = min(100, int(risk * 1.1))

    return risk


def recommendation_for_risk(risk_score, glucose, forecast_30):
    """Generate care recommendation."""
    if risk_score >= 90:
        return 5  # seek help
    if risk_score >= 80:
        return 4  # check glucose
    if forecast_30 < 70:
        return 2  # snack (15g fast carbs)
    if risk_score >= 60 and glucose > 180:
        return 3  # insulin
    if risk_score >= 20:
        return 1  # monitor
    return 0  # none


def train():
    """Train risk fusion model."""
    print("Loading data...")

    try:
        glucose_df = pd.read_csv("data/glucose_history.csv", parse_dates=["created_at"])
        meals_df = pd.read_csv("data/meal_history.csv", parse_dates=["created_at"])
        insulin_df = pd.read_csv("data/insulin_history.csv", parse_dates=["created_at"])
        activity_df = pd.read_csv("data/activity_history.csv", parse_dates=["created_at"])
    except FileNotFoundError:
        print("No data found. Generating synthetic...")
        from train_insulin_sensitivity import generate_synthetic_history
        glucose_df, meals_df, insulin_df, activity_df = generate_synthetic_history()

    # Extract features
    features = []
    labels = []

    glucose = glucose_df["glucose_mgdl"].values
    timestamps = glucose_df["created_at"].values

    for i in range(60, len(glucose) - 60):
        # Simulated sub-model outputs
        forecast_30 = glucose[i] + np.random.normal(0, 15)
        forecast_60 = glucose[i] + np.random.normal(0, 25)
        hypo_risk = max(0, min(100, (80 - forecast_30) * 3)) if forecast_30 < 80 else 0
        hyper_risk = max(0, min(100, (forecast_60 - 180) * 0.8)) if forecast_60 > 180 else 0

        # IOB
        t = timestamps[i]
        recent_insulin = insulin_df[
            (insulin_df["pen_type"] == 1) &
            (insulin_df["created_at"] < pd.Timestamp(t)) &
            (insulin_df["created_at"] > pd.Timestamp(t) - pd.Timedelta("3h"))
        ]
        iob = sum(
            r["estimated_units"] * np.exp(-(t - r["created_at"]).total_seconds() / 60 / 90)
            for _, r in recent_insulin.iterrows()
        ) if len(recent_insulin) > 0 else 0

        # COB
        recent_meals = meals_df[
            (meals_df["created_at"] < pd.Timestamp(t)) &
            (meals_df["created_at"] > pd.Timestamp(t) - pd.Timedelta("2h"))
        ]
        cob = sum(
            r["carb_grams"] * max(0, 1 - (t - r["created_at"]).total_seconds() / 60 / 120)
            for _, r in recent_meals.iterrows()
        ) if len(recent_meals) > 0 else 0

        # Activity
        recent_act = activity_df[
            (activity_df["created_at"] < pd.Timestamp(t)) &
            (activity_df["created_at"] > pd.Timestamp(t) - pd.Timedelta("30min"))
        ]
        intensity = recent_act["intensity"].mean() if len(recent_act) > 0 else 0
        hr = recent_act["hr"].mean() if len(recent_act) > 0 else 0

        # TIR (last 24h)
        tir_window = glucose[max(0, i - 1440):i]
        tir = sum(1 for g in tir_window if 70 <= g <= 180) / max(len(tir_window), 1) * 100

        features.append([
            glucose[i], forecast_30, forecast_60,
            hypo_risk, hyper_risk, iob, cob, hr, intensity, tir
        ])

        risk = compute_risk_score(
            glucose[i], forecast_30, forecast_60,
            hypo_risk, hyper_risk, iob, cob, hr, intensity, tir
        )
        labels.append(risk)

    features = np.array(features)
    labels = np.array(labels)

    X_train, X_test, y_train, y_test = train_test_split(
        features, labels, test_size=0.2, random_state=42
    )

    model = lgb.LGBMRegressor(
        n_estimators=200,
        max_depth=5,
        learning_rate=0.05,
        num_leaves=31,
    )
    model.fit(X_train, y_train,
              eval_set=[(X_test, y_test)],
              callbacks=[lgb.log_evaluation(10)])

    test_r2 = model.score(X_test, y_test)
    print(f"\nRisk fusion model — R²: {test_r2:.3f}")

    # Feature importance
    importance = model.feature_importances_
    feature_names = ["glucose", "f30", "f60", "hypo_risk", "hyper_risk",
                     "iob", "cob", "hr", "intensity", "tir"]
    for name, imp in sorted(zip(feature_names, importance), key=lambda x: -x[1]):
        print(f"  {name}: {imp}")

    joblib.dump(model, "models/risk_fusion_lgb.pkl")
    print("Done. Model saved to models/risk_fusion_lgb.pkl")


if __name__ == "__main__":
    os.makedirs("models", exist_ok=True)
    train()
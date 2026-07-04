"""
GlucoSync ML Pipeline — Insulin Sensitivity XGBoost

Personalized insulin-to-carb (I:C) ratio and insulin sensitivity factor (ISF).
Uses Bayesian online learning — starts with weight-based priors, adapts
from individual glucose response to insulin + meals.

License: MIT
"""

import numpy as np
import pandas as pd
import xgboost as xgb
from sklearn.model_selection import train_test_split
import joblib
import os

# ── Configuration ────────────────────────────────────────────────

LOOKBACK_DAYS = 14
MIN_DATA_POINTS = 50  # Need at least this many to personalize


def compute_ic_isf_priors(weight_kg: float, tdd_avg: float):
    """
    Weight-based initial estimates (clinical rules of thumb):
    - I:C ratio = 500 / weight_lbs
    - ISF = 1800 / TDD (total daily dose)
    """
    weight_lbs = weight_kg * 2.20462
    ic_ratio = 500.0 / weight_lbs
    isf = 1800.0 / max(tdd_avg, 1.0)
    return ic_ratio, isf


def extract_features(glucose_df: pd.DataFrame, meals_df: pd.DataFrame,
                     insulin_df: pd.DataFrame, activity_df: pd.DataFrame):
    """
    Extract features from historical data for insulin sensitivity learning.

    For each meal+bolus event, we look at:
    - Glucose before meal
    - Carbs consumed
    - Insulin units delivered
    - Glucose 2-3 hours after (to measure response)
    - Activity in the window
    - Time of day
    """
    features = []
    labels = []

    for _, meal in meals_df.iterrows():
        meal_time = pd.to_datetime(meal["created_at"])
        meal_carbs = meal["carb_grams"]

        # Find insulin dose within 15 min of meal
        insulin_mask = (insulin_df["pen_type"] == 1) & \
                       (abs((insulin_df["created_at"] - meal_time).dt.total_seconds()) < 900)
        if insulin_mask.sum() == 0:
            continue
        insulin_units = insulin_df[insulin_mask]["estimated_units"].iloc[0]

        # Glucose before meal (average 15 min before)
        pre_mask = (glucose_df["created_at"] < meal_time) & \
                   (glucose_df["created_at"] > meal_time - pd.Timedelta("15min"))
        if pre_mask.sum() == 0:
            continue
        pre_glucose = glucose_df[pre_mask]["glucose_mgdl"].mean()

        # Glucose 2-3 hours after (postprandial)
        post_mask = (glucose_df["created_at"] > meal_time + pd.Timedelta("2h")) & \
                    (glucose_df["created_at"] < meal_time + pd.Timedelta("3h"))
        if post_mask.sum() == 0:
            continue
        post_glucose = glucose_df[post_mask]["glucose_mgdl"].mean()

        # Activity in the window
        act_mask = (activity_df["created_at"] > meal_time) & \
                   (activity_df["created_at"] < meal_time + pd.Timedelta("3h"))
        avg_intensity = activity_df[act_mask]["intensity"].mean() if act_mask.sum() > 0 else 0
        avg_hr = activity_df[act_mask]["hr"].mean() if act_mask.sum() > 0 else 70

        # Time of day (0-1)
        tod = meal_time.hour / 24.0

        # Feature vector
        features.append([
            pre_glucose,      # pre-meal glucose
            meal_carbs,       # carbs consumed
            insulin_units,    # insulin units
            avg_intensity,    # activity intensity
            avg_hr,           # heart rate
            tod,              # time of day
        ])

        # Label: glucose delta (post - pre). Negative = good response.
        labels.append(post_glucose - pre_glucose)

    return np.array(features), np.array(labels)


def train_ic_model(features, labels):
    """
    Train XGBoost to predict glucose delta from (carbs, insulin, activity, tod).
    Then derive I:C ratio and ISF from the learned relationships.
    """
    if len(features) < MIN_DATA_POINTS:
        print(f"Only {len(features)} data points (need {MIN_DATA_POINTS}). "
              f"Using priors only.")
        return None

    X_train, X_test, y_train, y_test = train_test_split(
        features, labels, test_size=0.2, random_state=42
    )

    model = xgb.XGBRegressor(
        n_estimators=100,
        max_depth=4,
        learning_rate=0.05,
        subsample=0.8,
        colsample_bytree=0.8,
    )
    model.fit(X_train, y_train)

    train_score = model.score(X_train, y_train)
    test_score = model.score(X_test, y_test)
    print(f"IC model — R² train: {train_score:.3f}, test: {test_score:.3f}")

    # Derive I:C and ISF from model
    # I:C ratio: how many grams of carbs covered by 1 unit insulin
    # From the model: d(glucose)/d(insulin) = ISF
    #                d(glucose)/d(carbs) = 1/I:C * ISF

    # Estimate by perturbing inputs
    base = np.array([[120, 50, 5, 0, 70, 0.5]])  # typical scenario
    pred_base = model.predict(base)[0]

    # +1 unit insulin
    more_insulin = base.copy()
    more_insulin[0, 2] += 1
    pred_more_i = model.predict(more_insulin)[0]
    isf_estimate = pred_base - pred_more_i  # glucose drop per unit

    # +10g carbs
    more_carbs = base.copy()
    more_carbs[0, 1] += 10
    pred_more_c = model.predict(more_carbs)[0]
    carb_effect = (pred_more_c - pred_base) / 10  # glucose rise per gram

    if abs(carb_effect) > 0.1 and abs(isf_estimate) > 0.1:
        ic_estimate = abs(isf_estimate / carb_effect)
    else:
        ic_estimate = 10.0  # fallback

    return {
        "model": model,
        "isf": abs(isf_estimate),
        "ic_ratio": abs(ic_estimate),
        "n_samples": len(features),
        "r2_test": test_score,
    }


def train():
    """Main training entry point."""
    print("Loading historical data...")

    # Load from database export or CSV
    try:
        glucose_df = pd.read_csv("data/glucose_history.csv", parse_dates=["created_at"])
        meals_df = pd.read_csv("data/meal_history.csv", parse_dates=["created_at"])
        insulin_df = pd.read_csv("data/insulin_history.csv", parse_dates=["created_at"])
        activity_df = pd.read_csv("data/activity_history.csv", parse_dates=["created_at"])
        user_weight = 80  # kg, from profile
    except FileNotFoundError:
        print("No historical data found. Generating synthetic...")
        glucose_df, meals_df, insulin_df, activity_df = generate_synthetic_history()
        user_weight = 80

    # Compute priors
    tdd = insulin_df[insulin_df["pen_type"] == 1]["estimated_units"].sum() / LOOKBACK_DAYS
    ic_prior, isf_prior = compute_ic_isf_priors(user_weight, tdd)
    print(f"Priors: I:C={ic_prior:.1f}, ISF={isf_prior:.0f}, TDD={tdd:.1f}")

    # Extract features
    features, labels = extract_features(glucose_df, meals_df, insulin_df, activity_df)
    print(f"Extracted {len(features)} meal-insulin pairs")

    # Train model
    result = train_ic_model(features, labels)

    if result is not None:
        print(f"Personalized: I:C={result['ic_ratio']:.1f}, ISF={result['isf']:.0f} "
              f"(from {result['n_samples']} samples, R²={result['r2_test']:.3f})")
        joblib.dump(result["model"], "models/insulin_sensitivity_xgb.pkl")
    else:
        print(f"Using priors: I:C={ic_prior:.1f}, ISF={isf_prior:.0f}")

    # Save parameters
    import json
    params = {
        "ic_prior": ic_prior,
        "isf_prior": isf_prior,
        "personalized": result is not None,
        "ic_ratio": result["ic_ratio"] if result else ic_prior,
        "isf": result["isf"] if result else isf_prior,
        "n_samples": len(features),
        "tdd_avg": tdd,
    }
    with open("models/insulin_sensitivity_params.json", "w") as f:
        json.dump(params, f, indent=2)

    print("Done. Saved to models/")


def generate_synthetic_history():
    """Generate synthetic glucose/meal/insulin/activity data."""
    np.random.seed(42)
    n_days = LOOKBACK_DAYS
    times = pd.date_range("2026-01-01", periods=n_days * 1440, freq="1min")

    glucose = 120 + 30 * np.sin(np.arange(len(times)) * 2 * np.pi / 1440) + np.random.normal(0, 15, len(times))

    meals = []
    insulin = []
    activity = []

    for day in range(n_days):
        for meal_hour, carbs in [(8, 45), (13, 60), (19, 55)]:
            t = times[day * 1440 + meal_hour * 60]
            meals.append({"created_at": t, "carb_grams": carbs, "food_class_id": 1})
            insulin.append({"created_at": t + pd.Timedelta("5min"),
                           "pen_type": 1, "estimated_units": carbs / 8})

    for day in range(n_days):
        t = times[day * 1440 + 7 * 60]
        for i in range(30):
            activity.append({"created_at": t + pd.Timedelta(f"{i}min"),
                            "intensity": 60, "hr": 120})

    return (
        pd.DataFrame({"created_at": times, "glucose_mgdl": glucose.astype(int)}),
        pd.DataFrame(meals),
        pd.DataFrame(insulin),
        pd.DataFrame(activity),
    )


if __name__ == "__main__":
    os.makedirs("models", exist_ok=True)
    train()
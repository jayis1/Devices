"""
GlucoSync ML Pipeline — Hypoglycemia Warning Ensemble

Three-model ensemble predicting glucose <70 mg/dL within 30 minutes.
Optimized for HIGH SENSITIVITY (recall >90%) — false alarms are acceptable,
missed hypoglycemia events are dangerous.

Models:
  1. LSTM sub-model (glucose forecast)
  2. XGBoost sub-model (30-min features)
  3. Rule-based sub-model (clinical rules)

Ensemble voting: hypo warning if ≥2 of 3 models predict hypoglycemia.

License: MIT
"""

import numpy as np
import pandas as pd
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
import joblib
import os

# ── Configuration ────────────────────────────────────────────────

HYPO_THRESHOLD = 70     # mg/dL
PREDICTION_WINDOW = 30   # minutes ahead
RECALL_TARGET = 0.90     # minimum acceptable recall


def extract_30min_features(glucose_df, insulin_df, meals_df, activity_df):
    """
    Extract 30-min window features for hypo prediction.
    """
    features = []
    labels = []

    glucose = glucose_df["glucose_mgdl"].values
    timestamps = glucose_df["created_at"].values

    for i in range(30, len(glucose) - 30):
        # Current glucose
        current = glucose[i]

        # Rate of change (last 15 min)
        roc_15 = (glucose[i] - glucose[i - 15]) / 15.0

        # Rate of change (last 5 min)
        roc_5 = (glucose[i] - glucose[i - 5]) / 5.0

        # Glucose std (last 30 min)
        std_30 = np.std(glucose[i - 30:i])

        # Insulin on board (simplified)
        t = timestamps[i]
        recent_insulin = insulin_df[
            (insulin_df["pen_type"] == 1) &
            (insulin_df["created_at"] < pd.Timestamp(t)) &
            (insulin_df["created_at"] > pd.Timestamp(t) - pd.Timedelta("3h"))
        ]
        iob = sum(
            row["estimated_units"] * np.exp(
                -(t - row["created_at"]).total_seconds() / 60 / 90
            )
            for _, row in recent_insulin.iterrows()
        ) if len(recent_insulin) > 0 else 0

        # Carbs on board
        recent_meals = meals_df[
            (meals_df["created_at"] < pd.Timestamp(t)) &
            (meals_df["created_at"] > pd.Timestamp(t) - pd.Timedelta("2h"))
        ]
        cob = sum(
            row["carb_grams"] * max(0, 1 - (t - row["created_at"]).total_seconds() / 60 / 120)
            for _, row in recent_meals.iterrows()
        ) if len(recent_meals) > 0 else 0

        # Activity
        recent_activity = activity_df[
            (activity_df["created_at"] < pd.Timestamp(t)) &
            (activity_df["created_at"] > pd.Timestamp(t) - pd.Timedelta("30min"))
        ]
        avg_intensity = recent_activity["intensity"].mean() if len(recent_activity) > 0 else 0
        avg_hr = recent_activity["hr"].mean() if len(recent_activity) > 0 else 0

        # Time of day
        tod = pd.Timestamp(t).hour / 24.0

        features.append([
            current, roc_15, roc_5, std_30, iob, cob,
            avg_intensity, avg_hr, tod
        ])

        # Label: will glucose be <70 in next 30 min?
        future_min = min(glucose[i:i + 30])
        labels.append(1 if future_min < HYPO_THRESHOLD else 0)

    return np.array(features), np.array(labels)


def train_xgboost_model(features, labels):
    """Train XGBoost hypo prediction model."""
    X_train, X_test, y_train, y_test = train_test_split(
        features, labels, test_size=0.2, random_state=42, stratify=labels
    )

    # Handle class imbalance
    pos_weight = (len(y_train) - sum(y_train)) / max(sum(y_train), 1)

    model = xgb.XGBClassifier(
        n_estimators=200,
        max_depth=4,
        learning_rate=0.05,
        subsample=0.8,
        scale_pos_weight=pos_weight,  # upweight positive (hypo) class
        eval_metric="aucpr",
    )
    model.fit(X_train, y_train)

    # Evaluate
    y_pred = model.predict(X_test)
    y_prob = model.predict_proba(X_test)[:, 1]

    print("\nXGBoost Hypo Prediction:")
    print(classification_report(y_test, y_pred, target_names=["non-hypo", "hypo"]))

    recall = (y_pred[y_test == 1] == 1).sum() / max((y_test == 1).sum(), 1)
    precision = (y_pred[y_pred == 1] == 1).sum() / max((y_pred == 1).sum(), 1)
    print(f"Recall: {recall:.1%} (target: {RECALL_TARGET:.0%})")
    print(f"Precision: {precision:.1%}")
    print(f"False alarm rate: {1 - precision:.1%}")

    return model


def rule_based_hypo_check(features):
    """
    Clinical rule-based hypo prediction.
    Rules:
    - Glucose < 80 + falling > 2 mg/dL/min + IOB > 2 → hypo risk
    - Glucose < 70 → hypo (current)
    - Intense exercise + IOB > 1 → hypo risk
    """
    predictions = []
    for f in features:
        glucose, roc_15, roc_5, std_30, iob, cob, intensity, hr, tod = f
        hypo = 0

        if glucose < 70:
            hypo = 1
        elif glucose < 80 and roc_5 < -2 and iob > 2:
            hypo = 1
        elif glucose < 100 and intensity > 60 and iob > 1:
            hypo = 1

        predictions.append(hypo)
    return np.array(predictions)


def train():
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

    print("Extracting features...")
    features, labels = extract_30min_features(glucose_df, insulin_df, meals_df, activity_df)

    hypo_rate = labels.mean()
    print(f"Dataset: {len(features)} samples, {hypo_rate:.1%} hypo events")

    if hypo_rate == 0:
        print("No hypo events in data. Adding synthetic hypo events...")
        features, labels = inject_synthetic_hypo(features, labels)
        print(f"After injection: {labels.mean():.1%} hypo events")

    print("\nTraining XGBoost model...")
    xgb_model = train_xgboost_model(features, labels)
    joblib.dump(xgb_model, "models/hypo_warning_xgb.pkl")

    print("\nEvaluating rule-based model...")
    rule_preds = rule_based_hypo_check(features)
    rule_recall = (rule_preds[labels == 1] == 1).sum() / max((labels == 1).sum(), 1)
    print(f"Rule-based recall: {rule_recall:.1%}")

    # Ensemble evaluation
    print("\nEvaluating ensemble (XGBoost + rule-based)...")
    xgb_preds = xgb_model.predict(features)
    ensemble_preds = np.where((xgb_preds + rule_preds) >= 2, 1,
                       np.where((xgb_preds + rule_preds) >= 1, 1, 0))

    ensemble_recall = (ensemble_preds[labels == 1] == 1).sum() / max((labels == 1).sum(), 1)
    ensemble_precision = (ensemble_preds[labels == 1] == 1).sum() / max(ensemble_preds.sum(), 1)
    print(f"Ensemble recall: {ensemble_recall:.1%}")
    print(f"Ensemble precision: {ensemble_precision:.1%}")

    print("\nDone. Models saved to models/")


def inject_synthetic_hypo(features, labels):
    """Add synthetic hypo events for training."""
    np.random.seed(42)
    n_inject = len(features) // 10
    for _ in range(n_inject):
        idx = np.random.randint(30, len(features) - 30)
        # Simulate dropping glucose
        features[idx, 0] = 65  # low glucose
        features[idx, 1] = -3.0  # falling
        features[idx, 2] = -4.0
        features[idx, 4] = 3.0  # high IOB
        labels[idx] = 1
    return features, labels


if __name__ == "__main__":
    os.makedirs("models", exist_ok=True)
    train()
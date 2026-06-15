"""
MedSync - Adherence Prediction Model Training
Trains an XGBoost model for medication adherence prediction.

Input: 24 features including historical adherence, time of day,
       day of week, weather, activity level, room occupancy, etc.
Output: Probability of adherence (0-1) for each upcoming dose

Used by the hub to dynamically adjust reminder strategies.

Copyright (c) 2026 jayis1 - MIT License
"""

import numpy as np
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import roc_auc_score, classification_report
import joblib
from pathlib import Path
import argparse
import json

# ============================================================
# Feature Engineering
# ============================================================

FEATURE_NAMES = [
    # Time features
    "hour_of_day",           # 0-23
    "minute_of_hour",        # 0-59
    "day_of_week",           # 0-6 (Mon-Sun)
    "is_weekend",            # 0/1
    "time_since_last_dose_min",  # Minutes since last dose
    "doses_today",           # How many doses taken today

    # Historical adherence features
    "adherence_last_7_days",  # % adherence over last 7 days
    "adherence_last_30_days", # % adherence over last 30 days
    "streak_days",           # Current streak of perfect adherence days
    "missed_last_dose",      # 0/1 if last dose was missed

    # Medication features
    "pill_count",            # Number of pills per dose (1-10)
    "requires_food",         # 0/1 if medication must be taken with food
    "medication_complexity", # Number of different medications (1-8)

    # Activity features
    "steps_last_hour",       # Steps in last hour from wearable
    "activity_level",        # 0=still, 1=walking, 2=running, 3=sleeping
    "sleep_hours_last_night", # Hours of sleep last night
    "room_occupancy",        # 0/1 if patient is near pill station

    # Vital sign features
    "heart_rate_avg",        # Average heart rate last hour
    "spo2_avg",              # Average SpO2 last hour
    "skin_temp_avg",         # Average skin temperature last hour

    # Environmental features
    "room_temp",             # Room temperature °C
    "room_humidity",         # Room humidity %
    "is_holiday",            # 0/1
    "weather_condition",     # 0=sunny, 1=cloudy, 2=rainy, 3=snowy
]

# ============================================================
# Synthetic Data Generator (for demonstration)
# ============================================================

def generate_synthetic_data(n_samples=10000, seed=42):
    """Generate synthetic training data for adherence prediction."""
    np.random.seed(seed)

    X = np.zeros((n_samples, 24))
    y = np.zeros(n_samples)

    for i in range(n_samples):
        # Time features
        X[i, 0] = np.random.randint(0, 24)      # hour_of_day
        X[i, 1] = np.random.randint(0, 60)      # minute_of_hour
        X[i, 2] = np.random.randint(0, 7)        # day_of_week
        X[i, 3] = 1 if X[i, 2] >= 5 else 0       # is_weekend
        X[i, 4] = np.random.exponential(300)      # time_since_last_dose
        X[i, 5] = np.random.randint(0, 5)        # doses_today

        # Historical adherence
        X[i, 6] = np.random.uniform(0.5, 1.0)    # adherence_last_7
        X[i, 7] = np.random.uniform(0.6, 1.0)    # adherence_last_30
        X[i, 8] = np.random.randint(0, 30)        # streak_days
        X[i, 9] = np.random.choice([0, 1], p=[0.8, 0.2])  # missed_last

        # Medication features
        X[i, 10] = np.random.randint(1, 5)        # pill_count
        X[i, 11] = np.random.choice([0, 1])        # requires_food
        X[i, 12] = np.random.randint(1, 8)         # complexity

        # Activity features
        X[i, 13] = np.random.randint(0, 2000)      # steps_last_hour
        X[i, 14] = np.random.randint(0, 4)          # activity_level
        X[i, 15] = np.random.uniform(4, 10)         # sleep_hours
        X[i, 16] = np.random.choice([0, 1])         # room_occupancy

        # Vitals
        X[i, 17] = np.random.normal(72, 10)          # heart_rate
        X[i, 18] = np.random.normal(96, 2)            # spo2
        X[i, 19] = np.random.normal(33, 0.5)          # skin_temp

        # Environmental
        X[i, 20] = np.random.normal(22, 3)            # room_temp
        X[i, 21] = np.random.normal(45, 10)           # room_humidity
        X[i, 22] = np.random.choice([0, 1], p=[0.95, 0.05])  # is_holiday
        X[i, 23] = np.random.randint(0, 4)            # weather

        # Generate label based on features
        # Higher adherence probability if: morning, high historical adherence,
        # not weekend, not missed last dose, active, near pill station
        adherence_prob = 0.7  # Base probability
        adherence_prob += 0.05 if 7 <= X[i, 0] <= 9 else 0      # Morning dose
        adherence_prob -= 0.03 if X[i, 0] >= 22 else 0          # Late night dose
        adherence_prob += 0.1 * X[i, 6]                           # Recent adherence
        adherence_prob += 0.05 * min(X[i, 8] / 7, 1)             # Streak bonus
        adherence_prob -= 0.15 if X[i, 9] else 0                  # Missed last dose penalty
        adherence_prob += 0.05 if X[i, 16] else 0                 # Near pill station
        adherence_prob -= 0.03 * X[i, 10] / 5                     # More pills = harder
        adherence_prob -= 0.05 if X[i, 3] else 0                  # Weekend penalty
        adherence_prob -= 0.1 if X[i, 22] else 0                   # Holiday penalty
        adherence_prob -= 0.05 if X[i, 15] < 6 else 0             # Poor sleep penalty

        adherence_prob = np.clip(adherence_prob, 0, 1)
        y[i] = np.random.random() < adherence_prob

    return X, y


# ============================================================
# Training
# ============================================================

def train_model(args):
    """Train the adherence prediction model."""

    print("Generating training data...")
    X, y = generate_synthetic_data(n_samples=50000)

    # Split data
    X_train, X_val, y_train, y_val = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y
    )

    print(f"Training samples: {len(X_train)}")
    print(f"Validation samples: {len(X_val)}")
    print(f"Positive rate (adherent): {y_train.mean():.3f}")

    # XGBoost model
    params = {
        'objective': 'binary:logistic',
        'eval_metric': 'auc',
        'max_depth': 6,
        'learning_rate': 0.1,
        'n_estimators': 200,
        'min_child_weight': 10,
        'subsample': 0.8,
        'colsample_bytree': 0.8,
        'reg_alpha': 0.1,
        'reg_lambda': 1.0,
        'random_state': 42,
    }

    model = xgb.XGBClassifier(**params)

    print("\nTraining XGBoost model...")
    model.fit(
        X_train, y_train,
        eval_set=[(X_val, y_val)],
        verbose=True
    )

    # Evaluate
    y_pred_proba = model.predict_proba(X_val)[:, 1]
    y_pred = model.predict(X_val)

    auc = roc_auc_score(y_val, y_pred_proba)
    print(f"\nValidation AUC: {auc:.4f}")
    print("\nClassification Report:")
    print(classification_report(y_val, y_pred, target_names=["missed", "adherent"]))

    # Feature importance
    importance = model.feature_importances_
    print("\nFeature Importance (top 10):")
    sorted_idx = np.argsort(importance)[::-1]
    for i in range(min(10, len(sorted_idx))):
        idx = sorted_idx[i]
        print(f"  {FEATURE_NAMES[idx]}: {importance[idx]:.4f}")

    # Save model
    model_path = args.output_dir / "adherence_predictor.joblib"
    joblib.dump(model, model_path)
    print(f"\nModel saved to {model_path}")

    # Save feature names
    features_path = args.output_dir / "feature_names.json"
    with open(features_path, 'w') as f:
        json.dump(FEATURE_NAMES, f, indent=2)

    return model, auc


# ============================================================
# Main
# ============================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train MedSync adherence predictor")
    parser.add_argument("--output_dir", type=str, default="models",
                        help="Output directory for saved model")
    parser.add_argument("--synthetic", action="store_true",
                        help="Use synthetic data (for development)")

    args = parser.parse_args()
    args.output_dir = Path(args.output_dir)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    model, auc = train_model(args)

    print(f"\nTraining complete. AUC: {auc:.4f}")
    print("Use this model in medsync/schedule_engine.py for personalized reminders.")
"""
SpinalAge Regressor Training Script
Biological spinal age estimation from 90-day posture + EMG + curvature data

Architecture: LightGBM gradient-boosted regression
Input: 24 features (posture stats, EMG features, curvature metrics)
Output: Biological spinal age (years)
"""

import numpy as np
import lightgbm as lgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error, r2_score
import json
import os

# Features (24)
FEATURE_NAMES = [
    "avg_posture_score", "std_posture_score", "pct_time_neutral",
    "pct_time_forward_head", "pct_time_slouching", "pct_time_kyphotic",
    "avg_forward_tilt", "max_forward_tilt", "avg_lateral_tilt",
    "spine_angle_variability", "emg_asymmetry_avg", "emg_asymmetry_max",
    "emg_fatigue_avg", "co_contraction_ratio", "sitting_hours_avg",
    "movement_frequency", "correction_response_rate", "hrv_avg",
    "cervical_curvature", "thoracic_curvature", "lumbar_curvature",
    "curvature_progression", "scoliosis_risk", "days_monitored",
]
NUM_FEATURES = len(FEATURE_NAMES)


def generate_synthetic_data(n_samples=5000):
    """Generate synthetic 90-day posture data with spinal age labels"""
    np.random.seed(42)

    X = np.zeros((n_samples, NUM_FEATURES))
    y = np.zeros(n_samples)

    for i in range(n_samples):
        chronological_age = np.random.randint(20, 70)

        # Generate posture metrics
        avg_score = np.random.uniform(50, 95)
        std_score = np.random.uniform(5, 25)
        pct_neutral = avg_score / 100 * 80 + np.random.normal(0, 5)
        pct_forward = (100 - avg_score) * 0.3 + np.random.normal(0, 3)
        pct_slouch = (100 - avg_score) * 0.25 + np.random.normal(0, 3)
        pct_kyphotic = (100 - avg_score) * 0.15 + np.random.normal(0, 2)
        avg_tilt = 15 + (100 - avg_score) * 0.3 + np.random.normal(0, 2)
        max_tilt = avg_tilt + np.random.uniform(5, 20)
        lateral_tilt = abs(np.random.normal(3, 2))
        angle_var = np.random.uniform(3, 15)
        emg_asym = 10 + (100 - avg_score) * 0.2 + np.random.normal(0, 3)
        emg_asym_max = emg_asym + np.random.uniform(5, 20)
        emg_fatigue = np.random.uniform(10, 40)
        co_contraction = np.random.uniform(0.15, 0.45)
        sitting_hours = np.random.uniform(4, 14)
        movement_freq = np.random.uniform(5, 30)
        response_rate = np.random.uniform(0.3, 0.9)
        hrv = np.random.uniform(20, 60)
        cervical_c = np.random.uniform(20, 45)
        thoracic_c = np.random.uniform(20, 45)
        lumbar_c = np.random.uniform(30, 60)
        progression = np.random.uniform(-0.5, 0.5)
        scoliosis_r = np.random.uniform(0, 30)
        days = np.random.randint(60, 120)

        X[i] = [
            avg_score, std_score, pct_neutral, pct_forward, pct_slouch,
            pct_kyphotic, avg_tilt, max_tilt, lateral_tilt, angle_var,
            emg_asym, emg_asym_max, emg_fatigue, co_contraction,
            sitting_hours, movement_freq, response_rate, hrv,
            cervical_c, thoracic_c, lumbar_c, progression,
            scoliosis_r, days
        ]

        # Spinal age: base = chronological, modified by posture quality
        age_delta = 0
        age_delta += (100 - avg_score) * 0.15  # Poor posture adds age
        age_delta += pct_forward * 0.1
        age_delta += pct_slouch * 0.1
        age_delta += emg_asym * 0.05
        age_delta += emg_fatigue * 0.05
        age_delta += (sitting_hours - 8) * 0.3
        age_delta -= movement_freq * 0.1
        age_delta -= response_rate * 3
        age_delta += abs(progression) * 5
        age_delta += scoliosis_r * 0.1

        spinal_age = chronological_age + age_delta + np.random.normal(0, 2)
        y[i] = max(18, min(90, spinal_age))

    return X, y


def train_model(save_dir: str = "models"):
    """Train SpinalAge LightGBM regressor"""

    print("Generating synthetic training data...")
    X, y = generate_synthetic_data(n_samples=5000)
    print(f"Data: {X.shape}, Labels: {y.shape}")

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42
    )

    train_data = lgb.Dataset(X_train, label=y_train, feature_name=FEATURE_NAMES)
    val_data = lgb.Dataset(X_test, label=y_test, reference=train_data, feature_name=FEATURE_NAMES)

    params = {
        "objective": "regression",
        "metric": "mae",
        "num_leaves": 31,
        "learning_rate": 0.05,
        "feature_fraction": 0.8,
        "bagging_fraction": 0.8,
        "bagging_freq": 5,
        "verbose": -1,
    }

    model = lgb.train(
        params,
        train_data,
        num_boost_round=500,
        valid_sets=[val_data],
        callbacks=[lgb.early_stopping(20), lgb.log_evaluation(50)],
    )

    # Evaluate
    y_pred = model.predict(X_test)
    mae = mean_absolute_error(y_test, y_pred)
    r2 = r2_score(y_test, y_pred)

    print(f"\nMAE: {mae:.2f} years")
    print(f"R²: {r2:.4f}")

    # Save model
    os.makedirs(save_dir, exist_ok=True)
    model.save_model(os.path.join(save_dir, "spinal_age_regressor.txt"))

    # Feature importance
    importance = model.feature_importance()
    importance_dict = dict(zip(FEATURE_NAMES, importance))
    print("\nTop 10 features:")
    for name, imp in sorted(importance_dict.items(), key=lambda x: -x[1])[:10]:
        print(f"  {name}: {imp:.0f}")

    with open(os.path.join(save_dir, "spinal_age_importance.json"), "w") as f:
        json.dump(importance_dict, f, indent=2)

    return model


if __name__ == "__main__":
    train_model()
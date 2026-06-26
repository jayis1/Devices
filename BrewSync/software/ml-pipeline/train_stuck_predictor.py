"""
BrewSync ML Pipeline — Stuck Fermentation Predictor

Random Forest classifier that predicts stuck fermentations 72 hours in advance.
Uses CO2 evolution rate, temperature gradient, pH trend, and SG trajectory.

Features (per 12h window):
- sg_slope: linear slope of SG readings
- sg_r2: R² of SG fit (plateau = low R²)
- co2_mean: mean CO2 concentration
- co2_slope: slope of CO2 trend
- co2_peak: peak CO2 in window
- temp_mean: mean temperature
- temp_std: temperature standard deviation
- ph_mean: mean pH
- ph_slope: slope of pH

Target: 1 = stuck within 72h, 0 = healthy fermentation
"""

import numpy as np
from sklearn.ensemble import RandomForestClassifier, GradientBoostingClassifier
from sklearn.model_selection import cross_val_score, StratifiedKFold
from sklearn.preprocessing import StandardScaler
from sklearn.pipeline import Pipeline
import joblib
from pathlib import Path


def extract_features(window_sg, window_co2, window_temp, window_ph):
    """Extract features from a 12-hour sliding window of sensor data."""
    features = {}

    # SG features
    if len(window_sg) > 1:
        sg_slope = np.polyfit(np.arange(len(window_sg)), window_sg, 1)[0]
        sg_r2 = 1 - np.sum((window_sg - np.polyval(np.polyfit(np.arange(len(window_sg)), window_sg, 1),
                                                      np.arange(len(window_sg))))**2) / \
                max(np.sum((window_sg - np.mean(window_sg))**2), 1e-10)
    else:
        sg_slope = 0
        sg_r2 = 1.0

    features["sg_slope"] = sg_slope
    features["sg_r2"] = sg_r2
    features["sg_mean"] = np.mean(window_sg)
    features["sg_std"] = np.std(window_sg)

    # CO2 features
    features["co2_mean"] = np.mean(window_co2)
    features["co2_std"] = np.std(window_co2)
    features["co2_peak"] = np.max(window_co2)
    features["co2_min"] = np.min(window_co2)
    if len(window_co2) > 1:
        features["co2_slope"] = np.polyfit(np.arange(len(window_co2)), window_co2, 1)[0]
    else:
        features["co2_slope"] = 0

    # Temperature features
    features["temp_mean"] = np.mean(window_temp)
    features["temp_std"] = np.std(window_temp)
    features["temp_min"] = np.min(window_temp)
    features["temp_max"] = np.max(window_temp)
    features["temp_range"] = features["temp_max"] - features["temp_min"]

    # pH features
    features["ph_mean"] = np.mean(window_ph)
    features["ph_std"] = np.std(window_ph)
    if len(window_ph) > 1:
        features["ph_slope"] = np.polyfit(np.arange(len(window_ph)), window_ph, 1)[0]
    else:
        features["ph_slope"] = 0

    # Interaction features
    features["co2_temp_ratio"] = features["co2_mean"] / max(features["temp_mean"], 0.1)
    features["sg_co2_product"] = abs(features["sg_slope"]) * features["co2_mean"]

    return features


def generate_training_data(n_samples: int = 5000, seed: int = 42):
    """Generate synthetic training data with known stuck/healthy labels."""
    np.random.seed(seed)
    X = []
    y = []

    for i in range(n_samples):
        # Determine if this is a stuck fermentation
        is_stuck = np.random.random() < 0.15  # 15% stuck rate

        n_readings = 36  # 12 hours at 5-min intervals

        if is_stuck:
            # Stuck: SG plateaus, CO2 drops, temperature may be low
            sg_start = 1.040 + np.random.random() * 0.080
            sg_end = sg_start - (0.005 + np.random.random() * 0.015)  # Minimal drop
            sg = np.linspace(sg_start, sg_end, n_readings) + np.random.normal(0, 0.001, n_readings)

            co2_base = 200 + np.random.random() * 400  # Low CO2
            co2 = co2_base + np.random.normal(0, 50, n_readings)
            co2 = np.maximum(co2, 0)  # No negative CO2

            temp_base = 14 + np.random.random() * 4  # Cold (stuck cause)
            temp = temp_base + np.random.normal(0, 0.5, n_readings)

            ph_base = 4.8 + np.random.random() * 0.6  # High pH (stuck)
            ph = ph_base + np.random.normal(0, 0.05, n_readings)

        else:
            # Healthy: SG drops steadily, CO2 is active, temp is right
            sg_start = 1.040 + np.random.random() * 0.080
            sg_end = sg_start - (0.020 + np.random.random() * 0.060)  # Good attenuation
            sg = np.linspace(sg_start, sg_end, n_readings) + np.random.normal(0, 0.0005, n_readings)

            co2_peak = 1500 + np.random.random() * 3000
            t_norm = np.linspace(0, 1, n_readings)
            co2 = co2_peak * np.exp(-2 * (t_norm - 0.3)**2) + np.random.normal(0, 100, n_readings)
            co2 = np.maximum(co2, 0)

            temp_base = 18 + np.random.random() * 6  # Good fermentation temp
            temp = temp_base + np.random.normal(0, 0.3, n_readings)

            ph_base = 4.0 + np.random.random() * 0.3  # Normal pH
            ph = ph_base + np.random.normal(0, 0.02, n_readings)

        # Extract features
        features = extract_features(sg, co2, temp, ph)
        X.append(list(features.values()))
        y.append(1 if is_stuck else 0)

    return np.array(X), np.array(y)


def train_stuck_predictor(
    output_dir: str = "models",
    cross_validate: bool = True,
):
    """Train the stuck fermentation prediction model."""
    import os
    os.makedirs(output_dir, exist_ok=True)

    print("Generating training data...")
    X, y = generate_training_data(n_samples=10000)

    feature_names = [
        "sg_slope", "sg_r2", "sg_mean", "sg_std",
        "co2_mean", "co2_std", "co2_peak", "co2_min", "co2_slope",
        "temp_mean", "temp_std", "temp_min", "temp_max", "temp_range",
        "ph_mean", "ph_std", "ph_slope",
        "co2_temp_ratio", "sg_co2_product",
    ]

    # Model pipeline
    model = Pipeline([
        ("scaler", StandardScaler()),
        ("classifier", GradientBoostingClassifier(
            n_estimators=200,
            max_depth=5,
            learning_rate=0.1,
            min_samples_leaf=10,
            subsample=0.8,
            random_state=42,
        )),
    ])

    if cross_validate:
        print("Running cross-validation...")
        cv = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)
        scores = cross_val_score(model, X, y, cv=cv, scoring="f1")
        print(f"Cross-validation F1: {scores.mean():.3f} ± {scores.std():.3f}")
        print(f"Per-fold: {scores}")

    print("Training final model...")
    model.fit(X, y)

    # Save model
    model_path = f"{output_dir}/stuck_predictor.joblib"
    joblib.dump(model, model_path)
    print(f"Model saved to {model_path}")

    # Feature importance
    clf = model.named_steps["classifier"]
    importances = clf.feature_importances_
    print("\nFeature importances:")
    for name, imp in sorted(zip(feature_names, importances), key=lambda x: -x[1]):
        print(f"  {name}: {imp:.4f}")

    return model


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", default="models")
    parser.add_argument("--cross-validate", action="store_true")
    args = parser.parse_args()

    train_stuck_predictor(output_dir=args.output_dir, cross_validate=args.cross_validate)
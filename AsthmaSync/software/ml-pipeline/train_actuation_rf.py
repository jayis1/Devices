"""
AsthmaSync — Train Inhaler Actuation Classifier (Random Forest)
=================================================================
Classifies accelerometer data from the Inhaler Tag into 4 classes:
  0: static (no movement)
  1: actuation (MDI press)
  2: pocket_shake (jostling)
  3: drop (fallen on floor)

Input: 12 statistical features extracted from 300ms accel window
  peak_mag, mean_mag, std_mag, peak_jerk, mean_jerk, std_jerk,
  spectral_centroid, spectral_entropy, duration_s,
  x_range, y_range, z_range

License: MIT
"""

import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
import joblib
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

FEATURE_COLS = [
    "peak_mag", "mean_mag", "std_mag",
    "peak_jerk", "mean_jerk", "std_jerk",
    "spectral_centroid", "spectral_entropy", "duration_s",
    "x_range", "y_range", "z_range",
]

CLASS_NAMES = ["static", "actuation", "pocket_shake", "drop"]


def generate_synthetic_data(n_samples=5000):
    """Generate synthetic accelerometer features for 4 classes."""
    np.random.seed(42)

    data = []

    for cls in range(4):
        for _ in range(n_samples):
            if cls == 0:  # static
                features = [
                    np.random.uniform(0.9, 1.1),   # peak_mag
                    np.random.uniform(0.95, 1.05),  # mean_mag
                    np.random.uniform(0.01, 0.05),  # std_mag
                    np.random.uniform(0.01, 0.1),  # peak_jerk
                    np.random.uniform(0.01, 0.05),  # mean_jerk
                    np.random.uniform(0.01, 0.05),  # std_jerk
                    np.random.uniform(0.5, 2.0),    # spectral_centroid
                    np.random.uniform(0.01, 0.1),  # spectral_entropy
                    np.random.uniform(0.25, 0.35),  # duration_s
                    np.random.uniform(0.05, 0.15),  # x_range
                    np.random.uniform(0.05, 0.15),  # y_range
                    np.random.uniform(0.05, 0.15),  # z_range
                ]
            elif cls == 1:  # actuation (MDI press)
                features = [
                    np.random.uniform(3.0, 8.0),    # peak_mag (sharp impulse)
                    np.random.uniform(1.5, 3.0),    # mean_mag
                    np.random.uniform(1.0, 3.0),    # std_mag
                    np.random.uniform(5.0, 15.0),   # peak_jerk (high)
                    np.random.uniform(2.0, 6.0),    # mean_jerk
                    np.random.uniform(1.0, 4.0),    # std_jerk
                    np.random.uniform(5.0, 15.0),   # spectral_centroid
                    np.random.uniform(0.3, 0.8),    # spectral_entropy
                    np.random.uniform(0.08, 0.20),  # duration_s (short)
                    np.random.uniform(0.5, 2.0),    # x_range
                    np.random.uniform(0.5, 2.0),    # y_range
                    np.random.uniform(2.0, 6.0),    # z_range (dominant)
                ]
            elif cls == 2:  # pocket_shake
                features = [
                    np.random.uniform(2.0, 4.0),    # peak_mag (moderate)
                    np.random.uniform(1.5, 2.5),    # mean_mag
                    np.random.uniform(0.5, 1.5),    # std_mag
                    np.random.uniform(1.0, 4.0),    # peak_jerk
                    np.random.uniform(0.5, 2.0),    # mean_jerk
                    np.random.uniform(0.3, 1.0),    # std_jerk
                    np.random.uniform(2.0, 8.0),    # spectral_centroid
                    np.random.uniform(0.5, 1.0),    # spectral_entropy (broadband)
                    np.random.uniform(0.20, 0.35),  # duration_s (longer)
                    np.random.uniform(1.0, 3.0),    # x_range (multi-axis)
                    np.random.uniform(1.0, 3.0),    # y_range
                    np.random.uniform(0.5, 2.0),    # z_range
                ]
            elif cls == 3:  # drop
                features = [
                    np.random.uniform(10.0, 16.0),  # peak_mag (very high)
                    np.random.uniform(3.0, 6.0),    # mean_mag
                    np.random.uniform(3.0, 6.0),    # std_mag
                    np.random.uniform(10.0, 20.0),  # peak_jerk (very high)
                    np.random.uniform(5.0, 10.0),   # mean_jerk
                    np.random.uniform(3.0, 7.0),    # std_jerk
                    np.random.uniform(10.0, 20.0),  # spectral_centroid (broadband)
                    np.random.uniform(0.7, 1.2),    # spectral_entropy
                    np.random.uniform(0.05, 0.10),  # duration_s (very short)
                    np.random.uniform(3.0, 8.0),    # x_range (broadband)
                    np.random.uniform(3.0, 8.0),    # y_range
                    np.random.uniform(3.0, 8.0),    # z_range
                ]
            data.append(features + [cls])

    return np.array(data)


def train_actuation_rf():
    logger.info("Generating synthetic actuation data...")
    data = generate_synthetic_data(n_samples=5000)

    df = pd.DataFrame(data, columns=FEATURE_COLS + ["label"])
    X = df[FEATURE_COLS].values
    y = df["label"].values

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y)

    logger.info(f"Training: {len(X_train)} samples, Test: {len(X_test)} samples")

    # Train Random Forest
    model = RandomForestClassifier(
        n_estimators=100,
        max_depth=8,
        min_samples_leaf=5,
        class_weight="balanced",
        random_state=42,
    )
    model.fit(X_train, y_train)

    # Evaluate
    y_pred = model.predict(X_test)
    logger.info(f"\n{classification_report(y_test, y_pred, target_names=CLASS_NAMES)}")
    logger.info(f"Confusion matrix:\n{confusion_matrix(y_test, y_pred)}")

    # Feature importance
    importance = model.feature_importances_
    logger.info("\nFeature importance:")
    for feat, imp in sorted(zip(FEATURE_COLS, importance), key=lambda x: -x[1]):
        logger.info(f"  {feat:25s} {imp:.4f}")

    # Save model
    joblib.dump({
        "model": model,
        "features": FEATURE_COLS,
        "classes": CLASS_NAMES,
    }, "actuation_rf.pkl")
    logger.info("Model saved: actuation_rf.pkl")

    # Export to TFLite (for edge inference on Hub)
    # from sklearn-porter import Porter
    # porter = Porter(model, language='c')
    # output = porter.export(embedded=True)
    # with open('actuation_rf.c', 'w') as f: f.write(output)

    return model


if __name__ == "__main__":
    train_actuation_rf()
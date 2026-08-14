"""
MuscleImbalance XGBoost Training Script
Bilateral EMG asymmetry classification (6 classes)

Input: 8-channel EMG features (RMS, median freq, co-contraction, asymmetry, fatigue)
Output: 6-class muscle imbalance classification
"""

import numpy as np
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, accuracy_score
from sklearn.preprocessing import LabelEncoder
import json
import os

# Classes
IMBALANCE_CLASSES = [
    "None", "Left Upper Trap", "Right Upper Trap",
    "Left Erector", "Right Erector", "Bilateral Core"
]
NUM_CLASSES = len(IMBALANCE_CLASSES)

# Features per window (24 features)
# 8× RMS + 4× pair asymmetry + 4× median freq ratio + 4× co-contraction + 4× fatigue
NUM_FEATURES = 24


def generate_synthetic_data(n_samples=5000):
    """Generate synthetic EMG feature data for training"""
    np.random.seed(42)

    X = np.zeros((n_samples, NUM_FEATURES))
    y = np.zeros(n_samples, dtype=int)

    samples_per_class = n_samples // NUM_CLASSES

    for class_idx in range(NUM_CLASSES):
        start = class_idx * samples_per_class
        end = start + samples_per_class

        # Base RMS values (normalized 0-1)
        rms = np.random.uniform(0.1, 0.5, (samples_per_class, 8))

        if class_idx == 0:  # None — balanced
            pass
        elif class_idx == 1:  # Left Upper Trap
            rms[:, 0] += 0.3  # Left upper trap overactive
        elif class_idx == 2:  # Right Upper Trap
            rms[:, 1] += 0.3
        elif class_idx == 3:  # Left Erector
            rms[:, 2] += 0.3
        elif class_idx == 4:  # Right Erector
            rms[:, 3] += 0.3
        elif class_idx == 5:  # Bilateral Core
            rms[:, 6] += 0.2
            rms[:, 7] += 0.2

        rms = np.clip(rms, 0, 1)

        # Compute features
        for i in range(samples_per_class):
            features = []
            # 8× RMS
            features.extend(rms[i])
            # 4× pair asymmetry
            for pair in range(4):
                left = rms[i, pair * 2]
                right = rms[i, pair * 2 + 1]
                asym = abs(left - right) / (left + right + 0.001) * 100
                features.append(asym)
            # 4× median freq ratio
            for pair in range(4):
                features.append(np.random.uniform(0.8, 1.2))
            # 4× co-contraction ratio
            for pair in range(4):
                features.append(np.random.uniform(0.1, 0.5))
            # 4× fatigue index
            for pair in range(4):
                features.append(np.random.uniform(0, 30))

            X[start + i] = features
            y[start + i] = class_idx

    return X, y


def train_model(save_dir: str = "models"):
    """Train MuscleImbalance XGBoost classifier"""

    print("Generating synthetic EMG training data...")
    X, y = generate_synthetic_data(n_samples=6000)
    print(f"Data: {X.shape}, Labels: {y.shape}")

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, stratify=y, random_state=42
    )

    # XGBoost model
    model = xgb.XGBClassifier(
        n_estimators=200,
        max_depth=6,
        learning_rate=0.1,
        subsample=0.8,
        colsample_bytree=0.8,
        objective="multi:softprob",
        num_class=NUM_CLASSES,
        random_state=42,
    )

    model.fit(
        X_train, y_train,
        eval_set=[(X_test, y_test)],
        verbose=True,
    )

    # Evaluate
    y_pred = model.predict(X_test)
    accuracy = accuracy_score(y_test, y_pred)
    print(f"\nAccuracy: {accuracy:.4f}")
    print("\nClassification Report:")
    print(classification_report(y_test, y_pred, target_names=IMBALANCE_CLASSES))

    # Save model
    os.makedirs(save_dir, exist_ok=True)
    model.save_model(os.path.join(save_dir, "muscle_imbalance_xgb.json"))

    # Feature importance
    importance = model.feature_importances_
    feature_names = [
        "RMS_L_Trap", "RMS_R_Trap", "RMS_L_Erector", "RMS_R_Erector",
        "RMS_L_SCM", "RMS_R_SCM", "RMS_L_Rectus", "RMS_R_Rectus",
        "Asym_Trap", "Asym_Erector", "Asym_SCM", "Asym_Rectus",
        "MFR_Trap", "MFR_Erector", "MFR_SCM", "MFR_Rectus",
        "CoCon_Trap", "CoCon_Erector", "CoCon_SCM", "CoCon_Rectus",
        "Fatigue_Trap", "Fatigue_Erector", "Fatigue_SCM", "Fatigue_Rectus",
    ]

    importance_dict = dict(zip(feature_names, importance))
    print("\nFeature Importance:")
    for name, imp in sorted(importance_dict.items(), key=lambda x: -x[1])[:10]:
        print(f"  {name}: {imp:.4f}")

    with open(os.path.join(save_dir, "muscle_imbalance_importance.json"), "w") as f:
        json.dump(importance_dict, f, indent=2)

    return model


if __name__ == "__main__":
    train_model()
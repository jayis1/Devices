"""
JointSync ML Pipeline — Gait Loading Asymmetry (XGBoost)

Analyzes bilateral IMU data to detect gait asymmetry and loading imbalance.

License: MIT
"""

import numpy as np
from xgboost import XGBClassifier
from sklearn.metrics import classification_report, accuracy_score
from sklearn.model_selection import train_test_split
import os
import joblib

# Features per gait cycle:
# - Left/right step count ratio
# - Left/right impact magnitude ratio
# - Left/right stance duration ratio
# - Left/right swing duration ratio
# - Cadence
# - Stride length variance
# - Left/right ROM ratio
# - Double support time
# - Walk speed estimate
# - Asymmetry index (composite)

NUM_FEATURES = 10

def generate_synthetic_gait_data(n=3000):
    """Generate synthetic gait analysis data."""
    np.random.seed(42)
    X = np.zeros((n, NUM_FEATURES))
    y = np.zeros(n, dtype=int)  # 0=symmetric, 1=mild asymmetry, 2=significant

    for i in range(n):
        cls = i % 3
        y[i] = cls

        if cls == 0:  # Symmetric
            X[i, 0] = np.random.uniform(0.95, 1.05)  # Step ratio
            X[i, 1] = np.random.uniform(0.90, 1.10)  # Impact ratio
            X[i, 2] = np.random.uniform(0.95, 1.05)  # Stance ratio
            X[i, 3] = np.random.uniform(0.95, 1.05)  # Swing ratio
            X[i, 4] = np.random.uniform(90, 120)     # Cadence
            X[i, 5] = np.random.uniform(0.05, 0.15)  # Stride variance
            X[i, 6] = np.random.uniform(0.90, 1.10)  # ROM ratio
            X[i, 7] = np.random.uniform(10, 20)      # Double support %
            X[i, 8] = np.random.uniform(1.0, 1.4)    # Walk speed m/s
            X[i, 9] = np.random.uniform(0, 5)        # Asymmetry index
        elif cls == 1:  # Mild
            X[i, 0] = np.random.uniform(0.80, 0.95)
            X[i, 1] = np.random.uniform(0.75, 0.90)
            X[i, 2] = np.random.uniform(0.85, 0.95)
            X[i, 3] = np.random.uniform(1.05, 1.20)
            X[i, 4] = np.random.uniform(80, 100)
            X[i, 5] = np.random.uniform(0.15, 0.30)
            X[i, 6] = np.random.uniform(0.80, 0.90)
            X[i, 7] = np.random.uniform(15, 30)
            X[i, 8] = np.random.uniform(0.7, 1.0)
            X[i, 9] = np.random.uniform(5, 15)
        else:  # Significant
            X[i, 0] = np.random.uniform(0.60, 0.80)
            X[i, 1] = np.random.uniform(0.50, 0.75)
            X[i, 2] = np.random.uniform(0.70, 0.85)
            X[i, 3] = np.random.uniform(1.20, 1.50)
            X[i, 4] = np.random.uniform(70, 90)
            X[i, 5] = np.random.uniform(0.30, 0.50)
            X[i, 6] = np.random.uniform(0.60, 0.80)
            X[i, 7] = np.random.uniform(25, 45)
            X[i, 8] = np.random.uniform(0.5, 0.8)
            X[i, 9] = np.random.uniform(15, 40)

    return X, y

def train_gait_xgboost():
    print("=== JointSync Gait Loading XGBoost ===")

    X, y = generate_synthetic_gait_data(3000)
    X_train, X_val, y_train, y_val = train_test_split(X, y, test_size=0.2, random_state=42)

    model = XGBClassifier(
        n_estimators=200,
        max_depth=6,
        learning_rate=0.1,
        subsample=0.8,
        colsample_bytree=0.8,
        use_label_encoder=False,
        eval_metric='mlogloss',
    )

    model.fit(X_train, y_train,
              eval_set=[(X_val, y_val)],
              verbose=True)

    y_pred = model.predict(X_val)
    acc = accuracy_score(y_val, y_pred)
    print(f"\nValidation accuracy: {acc:.4f}")
    print(classification_report(y_val, y_pred, target_names=["symmetric", "mild", "significant"]))

    # Feature importance
    feature_names = ["step_ratio", "impact_ratio", "stance_ratio", "swing_ratio",
                     "cadence", "stride_var", "rom_ratio", "double_support",
                     "walk_speed", "asymmetry_index"]
    importances = model.feature_importances_
    for name, imp in sorted(zip(feature_names, importances), key=lambda x: -x[1]):
        print(f"  {name}: {imp:.4f}")

    # Save
    os.makedirs("models", exist_ok=True)
    joblib.dump(model, "models/gait_xgboost.pkl")
    print("Saved: models/gait_xgboost.pkl")

    return model

if __name__ == "__main__":
    train_gait_xgboost()
    print("\nGait XGBoost training complete!")
#!/usr/bin/env python3
"""
EchoSync — AlertPriority Training Script
XGBoost model for sound event priority classification & false-positive reduction.

Input: Sound class, confidence, context (temp, humidity, time-of-day), duration
Output: Priority (0=info, 1=important, 2=emergency) + false-positive probability

The model reduces false positives by learning contextual patterns:
- TV/music producing doorbell-like sounds → low priority
- Smoke alarm during cooking with high confidence → keep emergency
- Door knock when no one home → lower priority
"""
import argparse
import os
import numpy as np
import pandas as pd
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, f1_score, accuracy_score
import pickle


def generate_training_data(n_samples=10000):
    """Generate synthetic training data for AlertPriority model."""
    np.random.seed(42)

    data = []
    for _ in range(n_samples):
        sound_class = np.random.randint(0, 20)
        confidence = np.random.randint(50, 100)
        hour = np.random.randint(0, 24)
        day_of_week = np.random.randint(0, 7)
        duration_ms = np.random.randint(100, 10000)
        temp_c = np.random.uniform(15, 35)
        humidity_pct = np.random.uniform(20, 80)
        context_is_tv = np.random.random() < 0.1  # 10% are TV false positives
        context_is_music = np.random.random() < 0.05

        # Determine true priority
        if sound_class <= 3:  # Emergency classes
            if context_is_tv and confidence < 75:
                priority = 0  # False positive, downgrade
            else:
                priority = 2  # Emergency
        elif sound_class <= 8:  # Important classes
            if context_is_music and confidence < 70:
                priority = 0
            else:
                priority = 1
        else:
            priority = 0  # Info

        data.append({
            "sound_class": sound_class,
            "confidence": confidence,
            "hour": hour,
            "day_of_week": day_of_week,
            "duration_ms": duration_ms,
            "temp_c": temp_c,
            "humidity_pct": humidity_pct,
            "context_is_tv": int(context_is_tv),
            "context_is_music": int(context_is_music),
            "priority": priority,
        })

    return pd.DataFrame(data)


def train_model(args):
    print("Generating training data...")
    df = generate_training_data(n_samples=args.n_samples)

    # Features
    feature_cols = ["sound_class", "confidence", "hour", "day_of_week",
                    "duration_ms", "temp_c", "humidity_pct",
                    "context_is_tv", "context_is_music"]
    X = df[feature_cols].values
    y = df["priority"].values

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y
    )

    print(f"Train: {len(X_train)}, Test: {len(X_test)}")
    print(f"Class distribution: {np.bincount(y_train)}")

    # XGBoost model
    model = xgb.XGBClassifier(
        n_estimators=200,
        max_depth=6,
        learning_rate=0.1,
        subsample=0.8,
        colsample_bytree=0.8,
        objective="multi:softprob",
        num_class=3,
        random_state=42,
    )

    print("Training AlertPriority XGBoost...")
    model.fit(X_train, y_train)

    # Evaluate
    y_pred = model.predict(X_test)
    acc = accuracy_score(y_test, y_pred)
    f1 = f1_score(y_test, y_pred, average="macro")

    print(f"\n=== AlertPriority Results ===")
    print(f"Accuracy: {acc:.4f}")
    print(f"F1 (macro): {f1:.4f}")
    print("\nClassification Report:")
    print(classification_report(y_test, y_pred,
          target_names=["Info", "Important", "Emergency"]))

    # Feature importance
    importance = model.feature_importances_
    for name, imp in sorted(zip(feature_cols, importance), key=lambda x: -x[1]):
        print(f"  {name}: {imp:.4f}")

    # Save model
    os.makedirs(args.output, exist_ok=True)
    with open(os.path.join(args.output, "alert_priority.pkl"), "wb") as f:
        pickle.dump(model, f)
    print(f"\nModel saved to {args.output}/alert_priority.pkl")

    return model


def main():
    parser = argparse.ArgumentParser(description="Train AlertPriority XGBoost")
    parser.add_argument("--output", default="./models")
    parser.add_argument("--n-samples", type=int, default=10000)
    args = parser.parse_args()
    train_model(args)


if __name__ == "__main__":
    main()
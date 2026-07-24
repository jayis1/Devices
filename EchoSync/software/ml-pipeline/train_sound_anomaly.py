#!/usr/bin/env python3
"""
EchoSync — SoundAnomaly Training Script
Isolation Forest for unknown/unusual sound detection.

Detects sounds that are anomalous compared to the household's normal
sound patterns — useful for identifying unknown sounds that may be
important but aren't in the 20-class SoundNet vocabulary.
"""
import argparse
import os
import numpy as np
import pickle
from sklearn.ensemble import IsolationForest
from sklearn.metrics import classification_report, accuracy_score


def generate_training_data(n_samples=5000):
    """Generate sound feature data with anomalies."""
    np.random.seed(42)

    # Normal sound events (feature vectors: class, confidence, hour, duration, db_spl)
    normal = np.column_stack([
        np.random.randint(4, 20, n_samples),  # sound class (4-19)
        np.random.randint(70, 100, n_samples),  # confidence
        np.random.randint(0, 24, n_samples),  # hour
        np.random.randint(100, 5000, n_samples),  # duration ms
        np.random.randint(30, 80, n_samples),  # dB SPL
    ])

    # Anomalous events (unusual patterns)
    n_anomalies = n_samples // 20  # 5% anomalies
    anomalies = np.column_stack([
        np.random.randint(0, 20, n_anomalies),  # any class
        np.random.randint(30, 60, n_anomalies),  # low confidence
        np.random.choice([2, 3, 4, 23], n_anomalies),  # unusual hours
        np.random.randint(10000, 60000, n_anomalies),  # very long duration
        np.random.randint(80, 120, n_anomalies),  # very loud
    ])

    X = np.vstack([normal, anomalies])
    y = np.concatenate([np.ones(n_samples), -1 * np.ones(n_anomalies)])  # 1=normal, -1=anomaly

    return X, y


def train_model(args):
    print("Generating training data...")
    X, y = generate_training_data(n_samples=args.n_samples)

    print(f"Training data: {X.shape}, normal: {sum(y==1)}, anomaly: {sum(y==-1)}")

    # Isolation Forest
    model = IsolationForest(
        n_estimators=200,
        contamination=0.05,  # 5% expected anomalies
        random_state=42,
    )

    print("Training SoundAnomaly Isolation Forest...")
    model.fit(X)

    # Evaluate
    y_pred = model.predict(X)
    acc = accuracy_score(y, y_pred)
    detection_rate = np.mean(y_pred[y == -1] == -1)  # True positive rate for anomalies
    false_alarm_rate = np.mean(y_pred[y == 1] == -1)  # False positive rate

    print(f"\n=== SoundAnomaly Results ===")
    print(f"Accuracy: {acc:.4f}")
    print(f"Anomaly detection rate: {detection_rate:.4f} (target: >85%)")
    print(f"False alarm rate: {false_alarm_rate:.4f}")
    print(classification_report(y, y_pred, target_names=["Anomaly", "Normal"]))

    # Save model
    os.makedirs(args.output, exist_ok=True)
    with open(os.path.join(args.output, "sound_anomaly.pkl"), "wb") as f:
        pickle.dump(model, f)
    print(f"\nModel saved to {args.output}/sound_anomaly.pkl")


def main():
    parser = argparse.ArgumentParser(description="Train SoundAnomaly Isolation Forest")
    parser.add_argument("--output", default="./models")
    parser.add_argument("--n-samples", type=int, default=5000)
    args = parser.parse_args()
    train_model(args)


if __name__ == "__main__":
    main()
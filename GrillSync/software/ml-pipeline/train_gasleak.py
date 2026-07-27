"""
GrillSync — GasLeakNet Training Script
XGBoost classifier for gas leak pattern classification (leak vs normal cooking).

Input:  Gas sensor features (concentration, rate of change, duration, pattern)
Output: Binary classification (0 = normal, 1 = gas leak)
"""
import argparse
import numpy as np
import pandas as pd
from xgboost import XGBClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, roc_auc_score, confusion_matrix


def extract_features(gas_readings):
    """Extract features from gas sensor time series."""
    readings = np.array(gas_readings)
    features = {
        "mean": np.mean(readings),
        "max": np.max(readings),
        "min": np.min(readings),
        "std": np.std(readings),
        "rate_of_change": np.mean(np.diff(readings)),
        "max_gradient": np.max(np.abs(np.diff(readings))) if len(readings) > 1 else 0,
        "duration_above_500": np.sum(readings > 500),
        "duration_above_1000": np.sum(readings > 1000),
        "duration_above_2000": np.sum(readings > 2000),
        "lel_pct_max": (np.max(readings) / 21000) * 100,
        "lel_pct_mean": (np.mean(readings) / 21000) * 100,
        "rising_rate": np.mean(np.diff(readings)[np.diff(readings) > 0])
                        if np.any(np.diff(readings) > 0) else 0,
        "samples_to_threshold": np.argmax(readings > 2100) if np.any(readings > 2100) else len(readings),
    }
    return features


def train_model(data_path, output_path):
    """Train GasLeakNet XGBoost classifier."""
    print("Generating synthetic gas leak training data...")
    data = generate_synthetic_data(n_samples=10000)

    # Extract features
    feature_names = list(data[0]["features"].keys())
    X = np.array([[d["features"][k] for k in feature_names] for d in data])
    y = np.array([d["label"] for d in data])

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, stratify=y)

    # Handle class imbalance
    pos_count = np.sum(y_train == 1)
    neg_count = np.sum(y_train == 0)
    scale_pos = neg_count / max(pos_count, 1)

    model = XGBClassifier(
        n_estimators=200,
        max_depth=6,
        learning_rate=0.1,
        scale_pos_weight=scale_pos,
        subsample=0.8,
        colsample_bytree=0.8,
        eval_metric="auc",
        use_label_encoder=False,
    )

    print(f"Training GasLeakNet on {len(X_train)} samples ({pos_count} positive)")
    model.fit(X_train, y_train, eval_set=[(X_test, y_test)], verbose=True)

    # Evaluate
    y_pred = model.predict(X_test)
    y_proba = model.predict_proba(X_test)[:, 1]

    print("\n=== GasLeakNet Evaluation ===")
    print(classification_report(y_test, y_pred, target_names=["Normal", "Gas Leak"]))
    print(f"AUC-ROC: {roc_auc_score(y_test, y_proba):.4f}")
    print(f"Confusion Matrix:\n{confusion_matrix(y_test, y_pred)}")

    # Save model
    model.save_model(output_path)
    print(f"\nModel saved to {output_path}")

    # Feature importance
    importance = model.feature_importances_
    for name, imp in sorted(zip(feature_names, importance), key=lambda x: -x[1]):
        print(f"  {name}: {imp:.4f}")


def generate_synthetic_data(n_samples=10000):
    """Generate synthetic gas sensor data (normal + leak patterns)."""
    data = []
    for i in range(n_samples):
        is_leak = np.random.random() < 0.05  # 5% positive
        if is_leak:
            # Gas leak pattern: rapid rise, sustained high concentration
            duration = np.random.randint(30, 100)
            baseline = np.random.uniform(50, 200)
            peak = np.random.uniform(2100, 8000)
            rise_time = np.random.randint(5, 20)
            readings = np.concatenate([
                np.full(10, baseline),
                np.linspace(baseline, peak, rise_time),
                np.full(duration - 10 - rise_time, peak + np.random.normal(0, 100, duration - 10 - rise_time)),
            ])
        else:
            # Normal cooking: low, stable gas levels
            duration = np.random.randint(30, 200)
            baseline = np.random.uniform(20, 150)
            readings = baseline + np.random.normal(0, 20, duration)
            readings = np.clip(readings, 0, 500)

        features = extract_features(readings)
        data.append({"features": features, "label": int(is_leak)})
    return data


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train GasLeakNet")
    parser.add_argument("--data", default="/data/gas-events")
    parser.add_argument("--output", default="models/gasleak_v1.json")
    args = parser.parse_args()
    train_model(args.data, args.output)
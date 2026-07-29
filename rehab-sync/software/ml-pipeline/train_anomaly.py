"""
RehabSync — AnomalyIF Training Script

Isolation Forest for exercise anomaly / compensation pattern detection.
Detects compensatory movement patterns and regression in exercise form
that deviate from the patient's established clean-form baseline.

Input: Per-rep joint angle trajectory + force profile (flattened)
Output: Anomaly score (0-1, higher = more anomalous)

Unsupervised: trained on clean form data only, anomalies detected as
outliers at inference time.

Usage:
  python train_anomaly.py --data /data/clean_form_data
"""
import argparse
import numpy as np
from sklearn.ensemble import IsolationForest
from sklearn.metrics import classification_report, roc_auc_score
import pickle


def load_data(data_path):
    """Load clean form data for training, and anomalous data for evaluation.

    Clean data: per-rep joint angle trajectories + force profiles
    from exercises performed with correct form (therapist-verified).

    X_clean: (N_clean, D) — flattened per-rep features
    X_anomaly: (N_anomaly, D) — reps with known compensation patterns
    """
    data = np.load(f"{data_path}/anomaly_data.npz")
    X_clean = data["X_clean"]
    X_anomaly = data["X_anomaly"]
    feature_names = data["feature_names"]
    return X_clean, X_anomaly, feature_names


def extract_features(joint_angles, force):
    """Extract per-rep features from joint angle trajectory + force.

    Features:
    - Peak joint angle
    - Mean joint angle during rep
    - Rep duration (samples)
    - Angular velocity (mean, max)
    - Force peak, mean, total
    - Symmetry index (left/right)
    - Smoothness (jerk metric)
    """
    features = []

    # Joint angle features
    for rep in joint_angles:
        peak = np.max(rep)
        mean = np.mean(rep)
        duration = len(rep)
        velocity = np.mean(np.abs(np.diff(rep)))
        max_velocity = np.max(np.abs(np.diff(rep)))
        # Jerk (3rd derivative)
        if len(rep) > 3:
            jerk = np.mean(np.abs(np.diff(np.diff(np.diff(rep)))))
        else:
            jerk = 0
        features.extend([peak, mean, duration, velocity, max_velocity, jerk])

    # Force features
    for f in force:
        peak_f = np.max(f) if len(f) > 0 else 0
        mean_f = np.mean(f) if len(f) > 0 else 0
        total_f = np.sum(f) if len(f) > 0 else 0
        features.extend([peak_f, mean_f, total_f])

    return np.array(features, dtype=np.float32)


def train(args):
    X_clean, X_anomaly, feature_names = load_data(args.data)

    print(f"Clean samples: {len(X_clean)}")
    print(f"Anomaly samples: {len(X_anomaly)}")
    print(f"Feature dimension: {X_clean.shape[1]}")

    # Train Isolation Forest on clean data only
    iso_forest = IsolationForest(
        n_estimators=256,
        contamination=0.05,  # expected anomaly ratio
        max_samples="auto",
        random_state=42,
        n_jobs=-1,
    )

    iso_forest.fit(X_clean)

    # Evaluate on clean + anomaly data
    clean_scores = -iso_forest.score_samples(X_clean)  # higher = more anomalous
    anomaly_scores = -iso_forest.score_samples(X_anomaly)

    print(f"\nClean data anomaly scores: mean={clean_scores.mean():.4f} std={clean_scores.std():.4f}")
    print(f"Anomaly data scores: mean={anomaly_scores.mean():.4f} std={anomaly_scores.std():.4f}")

    # Binary predictions
    clean_pred = iso_forest.predict(X_clean)  # 1 = inlier, -1 = outlier
    anomaly_pred = iso_forest.predict(X_anomaly)

    y_true = np.concatenate([np.zeros(len(X_clean)), np.ones(len(X_anomaly))])
    y_pred = np.concatenate([clean_pred == -1, anomaly_pred == -1]).astype(int)

    print(f"\nDetection Results:")
    print(f"  True Positive Rate (anomaly detected): {(anomaly_pred == -1).mean():.4f}")
    print(f"  False Positive Rate (clean flagged):   {(clean_pred == -1).mean():.4f}")

    # AUC-ROC
    y_scores = np.concatenate([clean_scores, anomaly_scores])
    auc = roc_auc_score(y_true, y_scores)
    print(f"  AUC-ROC: {auc:.4f}")

    # Threshold analysis
    for threshold in [0.3, 0.4, 0.5, 0.6, 0.7]:
        tpr = (anomaly_scores > threshold).mean()
        fpr = (clean_scores > threshold).mean()
        print(f"  Threshold {threshold:.1f}: TPR={tpr:.3f} FPR={fpr:.3f}")

    # Save model
    with open(f"{args.output}/anomaly_if.pkl", "wb") as f:
        pickle.dump(iso_forest, f)
    print(f"\nModel saved to {args.output}/anomaly_if.pkl")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train AnomalyIF")
    parser.add_argument("--data", type=str, required=True)
    parser.add_argument("--output", type=str, default="models")
    args = parser.parse_args()

    import os
    os.makedirs(args.output, exist_ok=True)
    train(args)
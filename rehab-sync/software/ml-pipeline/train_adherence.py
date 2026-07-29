"""
RehabSync — AdherenceRF Training Script

Random Forest for 7-day adherence risk prediction.
Predicts the probability of a patient dropping out of their exercise
regimen within the next 7 days based on recent behavior patterns.

Input: 7-day features (session frequency, duration, completion rate,
       time-of-day patterns, form score trends, device usage)
Output: Adherence risk score (0-1), 7-day dropout probability

Usage:
  python train_adherence.py --data /data/adherence_records
"""
import argparse
import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import (accuracy_score, precision_score, recall_score,
                             roc_auc_score, classification_report)
from sklearn.model_selection import cross_val_score
import pickle


def load_data(data_path):
    """Load adherence records.

    Features (per patient, 7-day window):
    - sessions_completed_7d: int
    - sessions_missed_7d: int
    - avg_duration_min: float
    - completion_rate: float (0-1)
    - avg_form_score: float (0-100)
    - form_score_trend: float (slope)
    - avg_time_of_day: float (hour)
    - time_of_day_variance: float
    - days_since_last_session: int
    - total_reps_7d: int
    - device_usage_hours: float
    - age: int
    - condition_encoded: int
    - weeks_since_surgery: int

    Label: 1 if patient dropped out within next 7 days, 0 otherwise
    """
    data = np.load(f"{data_path}/adherence.npz")
    X_train = data["X_train"]
    y_train = data["y_train"]
    X_val = data["X_val"]
    y_val = data["y_val"]
    feature_names = data["feature_names"]
    return X_train, y_train, X_val, y_val, feature_names


def train(args):
    X_train, y_train, X_val, y_val, feature_names = load_data(args.data)

    print(f"Training set: {len(X_train)} samples, {len(feature_names)} features")
    print(f"Validation set: {len(X_val)} samples")
    print(f"Dropout rate (train): {y_train.mean():.3f}")
    print(f"Dropout rate (val): {y_val.mean():.3f}")
    print(f"\nFeatures: {list(feature_names)}")

    # Train Random Forest
    rf = RandomForestClassifier(
        n_estimators=500,
        max_depth=8,
        min_samples_leaf=20,
        class_weight="balanced",
        random_state=42,
        n_jobs=-1,
    )

    # Cross-validation
    cv_scores = cross_val_score(rf, X_train, y_train, cv=5, scoring="roc_auc")
    print(f"\n5-fold CV AUC: {cv_scores.mean():.4f} ± {cv_scores.std():.4f}")

    # Train on full training set
    rf.fit(X_train, y_train)

    # Validate
    y_pred = rf.predict(X_val)
    y_proba = rf.predict_proba(X_val)[:, 1]

    print(f"\nValidation Results:")
    print(f"  Accuracy:  {accuracy_score(y_val, y_pred):.4f}")
    print(f"  Precision: {precision_score(y_val, y_pred):.4f}")
    print(f"  Recall:    {recall_score(y_val, y_pred):.4f}")
    print(f"  AUC-ROC:   {roc_auc_score(y_val, y_proba):.4f}")
    print(f"\n{classification_report(y_val, y_pred, target_names=['adherent', 'dropout'])}")

    # Feature importance
    importances = rf.feature_importances_
    indices = np.argsort(importances)[::-1]
    print(f"\nFeature Importances:")
    for i in indices[:10]:
        print(f"  {feature_names[i]:30s} {importances[i]:.4f}")

    # Save model
    with open(f"{args.output}/adherence_rf.pkl", "wb") as f:
        pickle.dump(rf, f)
    print(f"\nModel saved to {args.output}/adherence_rf.pkl")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train AdherenceRF")
    parser.add_argument("--data", type=str, required=True)
    parser.add_argument("--output", type=str, default="models")
    args = parser.parse_args()

    import os
    os.makedirs(args.output, exist_ok=True)
    train(args)
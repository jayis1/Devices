"""
MenoSync — BoneRisk XGBoost Training Script

XGBoost (300 trees) for osteoporosis risk forecasting from
30-day activity load + sleep quality + demographics.

Input:  30-day features: [weight_bearing_min, steps_avg, sleep_quality_avg,
        night_sweat_count, age, bmi, family_history, calcium_intake,
        vitamin_d, hot_flash_count, hrv_avg, activity_var]
        → 12 features
Output: 30-day osteoporosis risk score 0-100
        (aligned with FRAX score categories: low <20, moderate 20-40, high >40)

Usage:
  python train_bonerisk.py --data /data/bone_dataset --epochs 300
"""
import argparse
import numpy as np
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import roc_auc_score, accuracy_score
from sklearn.preprocessing import StandardScaler


def load_data(data_path):
    """Load bone risk dataset.

    Expected format: .npz with X (N, 12) and y (N,) float32 (0-100 risk score)
    Features: [weight_bearing_min, steps_avg, sleep_quality_avg,
              night_sweat_count, age, bmi, family_history (0/1),
              calcium_intake_mg, vitamin_d_iu, hot_flash_count,
              hrv_avg, activity_var]
    Target: osteoporosis risk score 0-100 (aligned with FRAX)
    Source: Activity data + DXA scan results from 1,200 women
    """
    data = np.load(f"{data_path}/bone_risk.npz")
    X = data["X"].astype(np.float32)
    y = data["y"].astype(np.float32)

    # Binary classification: risk > 20 = at-risk
    y_binary = (y > 20).astype(np.int32)
    return X, y_binary, y


def train(args):
    print("Training BoneRisk XGBoost")

    X, y_binary, y_continuous = load_data(args.data)
    X_train, X_val, y_train, y_val = train_test_split(
        X, y_binary, test_size=0.2, random_state=42, stratify=y_binary)

    scaler = StandardScaler()
    X_train = scaler.fit_transform(X_train)
    X_val = scaler.transform(X_val)

    model = xgb.XGBClassifier(
        n_estimators=300,
        max_depth=6,
        learning_rate=0.05,
        subsample=0.8,
        colsample_bytree=0.8,
        min_child_weight=3,
        reg_alpha=0.1,
        reg_lambda=1.0,
        random_state=42,
        eval_metric="auc",
    )

    model.fit(X_train, y_train, eval_set=[(X_val, y_val)], verbose=True)

    y_pred = model.predict(X_val)
    y_prob = model.predict_proba(X_val)[:, 1]
    acc = accuracy_score(y_val, y_pred)
    auc = roc_auc_score(y_val, y_prob)

    print(f"\nValidation Results:")
    print(f"  Accuracy: {acc:.4f}")
    print(f"  AUC:      {auc:.4f}")

    # Feature importance
    feature_names = [
        "weight_bearing_min", "steps_avg", "sleep_quality_avg",
        "night_sweat_count", "age", "bmi", "family_history",
        "calcium_intake_mg", "vitamin_d_iu", "hot_flash_count",
        "hrv_avg", "activity_var"
    ]
    importances = model.feature_importances_
    print(f"\nFeature Importances:")
    for name, imp in sorted(zip(feature_names, importances), key=lambda x: -x[1]):
        print(f"  {name:25s}: {imp:.4f}")

    model.save_model(f"{args.output}/bone_risk_model.json")
    print(f"\nModel saved to {args.output}/bone_risk_model.json")
    print(f"Done. AUC: {auc:.4f}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", type=str, required=True)
    parser.add_argument("--epochs", type=int, default=300)
    parser.add_argument("--output", type=str, default="models")
    args = parser.parse_args()
    import os
    os.makedirs(args.output, exist_ok=True)
    train(args)
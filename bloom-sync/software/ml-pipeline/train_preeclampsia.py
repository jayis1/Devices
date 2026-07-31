"""
BloomSync — PreeclampsiaRF Training Script

XGBoost (200 trees) for postpartum preeclampsia detection from
6-hour HR, SpO₂, and skin temperature patterns.

Input:  6h × 6 features at 1-min intervals → 360×6
        (HR_mean, HR_std, SpO2_min, SpO2_std, temp_mean, HRV_mean)
        + trend features (HR_slope, SpO2_slope, temp_slope)
        = 9 aggregated features
Output: 2-class (normal / preeclampsia)
Deployment: Cloud (feature extraction on Hub, inference in cloud)

Usage:
  python train_preeclampsia.py --data /data/preeclampsia_dataset --epochs 300
"""
import argparse
import numpy as np
import xgboost as xgb
from sklearn.metrics import classification_report, roc_auc_score
from sklearn.model_selection import StratifiedKFold


class PreeclampsiaFeatureExtractor:
    """Extract aggregated features from 6h vitals window.

    Raw: (N, 360, 6) — 6h × 1-min × [HR, SpO2, skin_temp, HRV, activity, steps]
    Features (9):
      - HR_mean, HR_std, HR_slope (rising HR is key indicator)
      - SpO2_min, SpO2_std (low SpO₂)
      - temp_mean, temp_slope (thermoregulatory instability)
      - HRV_mean (low HRV = cardiovascular stress)
      - HR_SpO2_ratio (elevated HR with low SpO₂ = high risk)
    """
    def extract(self, X):
        """X: (N, 360, 6) → features (N, 9)"""
        hr = X[:, :, 0]
        spo2 = X[:, :, 1]
        temp = X[:, :, 2]
        hrv = X[:, :, 3]

        features = np.column_stack([
            hr.mean(axis=1),
            hr.std(axis=1),
            self._slope(hr),
            spo2.min(axis=1),
            spo2.std(axis=1),
            temp.mean(axis=1),
            self._slope(temp),
            hrv.mean(axis=1),
            hr.mean(axis=1) / (spo2.min(axis=1) + 1),  # HR/SpO₂ ratio
        ])
        return features

    @staticmethod
    def _slope(x):
        """Linear regression slope for each sample."""
        n = x.shape[1]
        t = np.arange(n)
        t_mean = t.mean()
        t_std = t.std()
        if t_std < 1e-8:
            return np.zeros(x.shape[0])
        return ((x - x.mean(axis=1, keepdims=True)) * (t - t_mean)).sum(axis=1) / \
               (n * t_std ** 2)


def train(args):
    print("Training PreeclampsiaRF (XGBoost)")

    # Load data
    train_data = np.load(f"{args.data}/train.npz")
    val_data = np.load(f"{args.data}/val.npz")
    X_train_raw = train_data["X"].astype(np.float32)  # (N, 360, 6)
    y_train = train_data["y"].astype(np.int64)
    X_val_raw = val_data["X"].astype(np.float32)
    y_val = val_data["y"].astype(np.int64)

    # Extract features
    extractor = PreeclampsiaFeatureExtractor()
    X_train = extractor.extract(X_train_raw)
    X_val = extractor.extract(X_val_raw)

    print(f"Train: {X_train.shape}, Val: {X_val.shape}")
    print(f"Train pos: {y_train.sum()}, Val pos: {y_val.sum()}")

    # XGBoost with class weighting
    scale_pos = (len(y_train) - y_train.sum()) / max(y_train.sum(), 1)
    model = xgb.XGBClassifier(
        n_estimators=200,
        max_depth=5,
        learning_rate=0.1,
        scale_pos_weight=scale_pos,
        eval_metric="aucpr",
        random_state=42,
    )

    model.fit(X_train, y_train, eval_set=[(X_val, y_val)], verbose=False)

    # Evaluate
    y_pred = model.predict(X_val)
    y_proba = model.predict_proba(X_val)[:, 1]
    print("\nClassification Report:")
    print(classification_report(y_val, y_pred, target_names=["normal", "preeclampsia"]))
    print(f"AUC-ROC: {roc_auc_score(y_val, y_proba):.4f}")

    # Feature importance
    feature_names = ["HR_mean", "HR_std", "HR_slope", "SpO2_min", "SpO2_std",
                     "temp_mean", "temp_slope", "HRV_mean", "HR_SpO2_ratio"]
    importances = model.feature_importances_
    print("\nFeature Importances:")
    for name, imp in sorted(zip(feature_names, importances), key=lambda x: -x[1]):
        print(f"  {name}: {imp:.4f}")

    # Cross-validation
    print("\n5-Fold Stratified CV:")
    X_all = np.vstack([X_train, X_val])
    y_all = np.concatenate([y_train, y_val])
    cv = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)
    cv_scores = []
    for fold, (tr, te) in enumerate(cv.split(X_all, y_all)):
        m = xgb.XGBClassifier(n_estimators=200, max_depth=5, learning_rate=0.1,
                              scale_pos_weight=scale_pos, eval_metric="aucpr")
        m.fit(X_all[tr], y_all[tr])
        pred = m.predict(X_all[te])
        tp = ((pred == 1) & (y_all[te] == 1)).sum()
        fn = ((pred == 0) & (y_all[te] == 1)).sum()
        sensitivity = tp / max(tp + fn, 1)
        cv_scores.append(sensitivity)
        print(f"  Fold {fold+1}: sensitivity={sensitivity:.4f}")
    print(f"  Mean: {np.mean(cv_scores):.4f} ± {np.std(cv_scores):.4f}")

    # Save
    import os
    os.makedirs(args.output, exist_ok=True)
    model.save_model(f"{args.output}/preeclampsia_rf.json")
    print(f"\nModel saved to {args.output}/preeclampsia_rf.json")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", type=str, required=True)
    parser.add_argument("--epochs", type=int, default=300)
    parser.add_argument("--output", type=str, default="models")
    args = parser.parse_args()
    train(args)
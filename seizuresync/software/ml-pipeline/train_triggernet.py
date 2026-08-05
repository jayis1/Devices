"""
SeizureSync — Model 5: TriggerNet (seizure trigger attribution)
XGBoost classifier + SHAP for per-trigger attribution.
Identifies patient-specific seizure triggers from multi-day features.
SPDX-License-Identifier: MIT
"""
import numpy as np
import xgboost as xgb
import shap
import argparse
import json


TRIGGER_NAMES = [
    "sleep_deprivation", "stress", "missed_medication",
    "alcohol", "menstrual_cycle", "weather_change",
    "fever", "flashing_lights", "hypoglycemia",
]


def train(args):
    """Train XGBoost to predict seizure occurrence from daily features."""
    # Stub data: 30 days × 9 features → seizure (1) or not (0)
    n = 500
    X = np.random.rand(n, 9).astype(np.float32)
    y = (X[:, 0] > 0.7).astype(int)   # sleep deprivation > 0.7 → seizure

    model = xgb.XGBClassifier(n_estimators=100, max_depth=4,
                             learning_rate=0.1, eval_metric='logloss')
    model.fit(X, y)

    # SHAP attribution
    explainer = shap.TreeExplainer(model)
    shap_vals = explainer.shap_values(X)

    # Print mean |SHAP| per trigger
    mean_shap = np.mean(np.abs(shap_vals), axis=0)
    for name, val in sorted(zip(TRIGGER_NAMES, mean_shap), reverse=True):
        print(f"  {name}: {val:.4f}")

    model.save_model("models/triggernet_v1.json")
    print("Saved models/triggernet_v1.json")


if __name__ == "__main__":
    train(argparse.ArgumentParser().parse_args())
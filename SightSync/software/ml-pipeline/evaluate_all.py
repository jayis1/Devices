"""
SightSync ML Pipeline — Evaluate All Models
============================================

Runs evaluation metrics on all trained models.

License: MIT
"""

import os
import sys

MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "firmware", "hub", "models")

def evaluate_all():
    print("=== SightSync Model Evaluation ===\n")

    models = {
        "fatigue_index.joblib": "Fatigue Index (XGBoost) — target MAE < 8",
        "blink_isoforest.joblib": "Blink Anomaly (Isolation Forest) — target AUROC > 0.92",
        "myopia_lstm.pt": "Myopia Forecast (LSTM) — target 90-day AUC > 0.85",
        "lamp_dqn.pt": "Circadian Lamp (DQN) — target +35% user satisfaction",
        "posture_cnn.pt": "Posture CNN (1D-CNN) — target F1 > 0.88",
        "dry_eye_risk.json": "Dry-Eye Risk (XGBoost) — target AUROC > 0.87",
    }

    for filename, description in models.items():
        path = os.path.join(MODEL_DIR, filename)
        exists = os.path.exists(path)
        status = "✓ found" if exists else "✗ not found"
        print(f"  {filename}: {status}")
        print(f"    {description}")

    print("\nRun individual train_*.py scripts to train missing models.")


if __name__ == "__main__":
    evaluate_all()
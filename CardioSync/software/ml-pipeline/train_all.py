#!/usr/bin/env python3
"""
train_all.py — Train all CardioSync ML models in sequence

Runs all 7 model training scripts in order.

License: MIT
"""
import subprocess
import sys
import os

MODELS = [
    ("AFib CNN", "train_afib_cnn.py"),
    ("BP Trend LSTM", "train_bp_trend_lstm.py"),
    ("Stroke Risk XGBoost", "train_stroke_risk_xgb.py"),
    ("Sleep Apnea LSTM", "train_sleep_apnea_lstm.py"),
    ("POTS Detector", "train_pots_detector.py"),
]

def main():
    print("=" * 60)
    print("CardioSync ML Pipeline — Train All Models")
    print("=" * 60)

    os.makedirs("models", exist_ok=True)

    for name, script in MODELS:
        print(f"\n{'─' * 60}")
        print(f"Training: {name}")
        print(f"{'─' * 60}")

        result = subprocess.run(
            [sys.executable, script],
            cwd=os.path.dirname(os.path.abspath(__file__))
        )

        if result.returncode != 0:
            print(f"ERROR: {name} training failed (exit code {result.returncode})")
            sys.exit(1)

    print(f"\n{'=' * 60}")
    print("All models trained successfully!")
    print(f"{'=' * 60}")

    # Convert models to TFLite
    print("\nConverting models to TFLite...")
    subprocess.run(
        [sys.executable, "convert_models.py"],
        cwd=os.path.dirname(os.path.abspath(__file__))
    )

    print("\nDone! Models are in models/ directory.")
    print("Edge models (C arrays) are in firmware/hub/models/")

if __name__ == "__main__":
    main()
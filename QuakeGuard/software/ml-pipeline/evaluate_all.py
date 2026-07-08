#!/usr/bin/env python3
"""
QuakeGuard ML Pipeline Evaluation Script

Evaluates all models and produces a summary report.

License: MIT
"""
import os
from pathlib import Path


def evaluate_pwave_cnn():
    """Evaluate P-wave/S-wave CNN."""
    print("\n[1/5] P-Wave/S-Wave CNN")
    model_path = "models/p_wave_cnn/p_wave_cnn.tflite"
    if not Path(model_path).exists():
        print(f"  Model not found: {model_path}")
        print("  Run: python train_pwave_cnn.py")
        return
    try:
        import tensorflow as tf
        import numpy as np
        interpreter = tf.lite.Interpreter(model_path=model_path)
        interpreter.allocate_tensors()
        details = interpreter.get_input_details()
        print(f"  Input shape: {details[0]['shape']}")
        print(f"  Input dtype: {details[0]['dtype']}")
        details = interpreter.get_output_details()
        print(f"  Output shape: {details[0]['shape']}")
        print(f"  Model size: {os.path.getsize(model_path)} bytes")
    except Exception as e:
        print(f"  Error: {e}")


def evaluate_structural_autoencoder():
    """Evaluate structural health autoencoder."""
    print("\n[2/5] Structural Health Autoencoder")
    model_path = "models/structural_autoencoder/structural_autoencoder.tflite"
    if not Path(model_path).exists():
        print(f"  Model not found: {model_path}")
        return
    print(f"  Model size: {os.path.getsize(model_path)} bytes")


def evaluate_damage_severity():
    """Evaluate damage severity classifier."""
    print("\n[3/5] Damage Severity Classifier")
    model_path = "models/damage_severity/damage_severity.json"
    if not Path(model_path).exists():
        print(f"  Model not found: {model_path}")
        return
    try:
        import xgboost as xgb
        model = xgb.XGBClassifier()
        model.load_model(model_path)
        print(f"  Model loaded: {model.n_estimators} trees")
    except Exception as e:
        print(f"  Error: {e}")


def evaluate_aftershock_risk():
    """Evaluate aftershock risk LSTM."""
    print("\n[4/5] Aftershock Risk LSTM")
    model_path = "models/aftershock_risk/aftershock_risk.tflite"
    if not Path(model_path).exists():
        print(f"  Model not found: {model_path}")
        return
    print(f"  Model size: {os.path.getsize(model_path)} bytes")


def evaluate_magnitude():
    """Evaluate magnitude estimation CNN."""
    print("\n[5/5] Magnitude Estimation CNN")
    model_path = "models/magnitude_estimation/magnitude_estimation.tflite"
    if not Path(model_path).exists():
        print(f"  Model not found: {model_path}")
        return
    print(f"  Model size: {os.path.getsize(model_path)} bytes")


def main():
    print("=" * 60)
    print("QuakeGuard ML Pipeline Evaluation Summary")
    print("=" * 60)

    evaluate_pwave_cnn()
    evaluate_structural_autoencoder()
    evaluate_damage_severity()
    evaluate_aftershock_risk()
    evaluate_magnitude()

    print("\n" + "=" * 60)
    print("All evaluations complete.")
    print("=" * 60)


if __name__ == "__main__":
    main()
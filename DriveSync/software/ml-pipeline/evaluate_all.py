"""
DriveSync ML Pipeline — Evaluate All Models

Runs evaluation on all trained models and prints a summary report.

License: MIT
"""

import os
import json
from datetime import datetime

MODELS = {
    "eye_closure_cnn": {
        "script": "train_drowsiness_cnn.py",
        "model_path": "./models/eye_closure_cnn.pt",
        "tflite_path": "./models/eye_closure_cnn_int8.tflite",
    },
    "headpose_cnn": {
        "script": "train_headpose_cnn.py",
        "model_path": "./models/headpose_cnn.pt",
        "tflite_path": "./models/headpose_cnn_int8.tflite",
    },
    "steering_xgboost": {
        "script": "train_steering_xgboost.py",
        "model_path": "./models/steering_xgboost.json",
    },
    "hrv_drowsiness_lstm": {
        "script": "train_hrv_drowsiness_lstm.py",
        "model_path": "./models/hrv_drowsiness_lstm.pt",
        "tflite_path": "./models/hrv_drowsiness_lstm.tflite",
    },
    "risk_fusion": {
        "script": "train_risk_fusion.py",
        "model_path": "./models/risk_fusion_lgbm.pkl",
    },
}


def evaluate_all():
    print("=" * 60)
    print("DriveSync — Model Evaluation Summary")
    print("=" * 60)

    results = {}

    for name, config in MODELS.items():
        print(f"\n{'─' * 40}")
        print(f"Model: {name}")

        model_path = config["model_path"]
        tflite_path = config.get("tflite_path")

        model_exists = os.path.exists(model_path)
        tflite_exists = tflite_path and os.path.exists(tflite_path)

        model_size = os.path.getsize(model_path) if model_exists else 0
        tflite_size = os.path.getsize(tflite_path) if (tflite_path and tflite_exists) else 0

        print(f"  PyTorch model: {'✓' if model_exists else '✗'} ({model_size} bytes)")
        if tflite_path:
            print(f"  TFLite model:  {'✓' if tflite_exists else '✗'} ({tflite_size} bytes)")

        results[name] = {
            "model_exists": model_exists,
            "model_size_bytes": model_size,
            "tflite_exists": tflite_exists,
            "tflite_size_bytes": tflite_size,
        }

    # Summary
    print(f"\n{'=' * 40}")
    print("Summary:")
    total_size = sum(r["tflite_size_bytes"] or r["model_size_bytes"]
                     for r in results.values())
    print(f"  Total model size: {total_size} bytes")
    print(f"  Models trained: {sum(1 for r in results.values() if r['model_exists'])}/{len(MODELS)}")

    # Edge deployment target
    edge_budget = 200000  # 200 KB for ESP32-S3 flash
    print(f"  Edge budget: {edge_budget} bytes")
    print(f"  Fits on ESP32-S3: {'✓' if total_size < edge_budget else '✗'}")

    # Save report
    report = {
        "timestamp": datetime.utcnow().isoformat(),
        "models": results,
        "total_size_bytes": total_size,
        "edge_target": "ESP32-S3",
        "edge_budget_bytes": edge_budget,
    }

    with open("./models/evaluation_report.json", "w") as f:
        json.dump(report, f, indent=2)
    print(f"\nReport saved to ./models/evaluation_report.json")


if __name__ == "__main__":
    evaluate_all()
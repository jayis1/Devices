#!/usr/bin/env python3
"""
SightSync — Model Deployment Script

Packages trained ML models and copies them to the firmware directory
for edge deployment on the ESP32-S3 hub.

License: MIT
"""

import os
import shutil
import sys

ML_DIR = os.path.join(os.path.dirname(__file__), "..", "software", "ml-pipeline")
FIRMWARE_MODELS_DIR = os.path.join(os.path.dirname(__file__), "..", "firmware", "hub", "models")

MODELS = {
    "fatigue_index.joblib": "fatigue_index_model.h",
    "blink_isoforest.joblib": "blink_anomaly_model.h",
    "posture_cnn.pt": "posture_cnn.onnx",
    "myopia_lstm.pt": "myopia_lstm.onnx",
    "lamp_dqn.pt": "lamp_dqn_policy.json",
    "dry_eye_risk.json": "dry_eye_risk_model.h",
}

def deploy():
    print("=== SightSync Model Deployment ===")
    os.makedirs(FIRMWARE_MODELS_DIR, exist_ok=True)

    for src, dst in MODELS.items():
        src_path = os.path.join(ML_DIR, src)
        dst_path = os.path.join(FIRMWARE_MODELS_DIR, dst)

        if os.path.exists(src_path):
            shutil.copy2(src_path, dst_path)
            print(f"  ✓ {src} → {dst_path}")
        else:
            print(f"  ✗ {src} not found — run training first")

    print(f"\nModels deployed to: {FIRMWARE_MODELS_DIR}")
    print("Rebuild hub firmware to include updated models.")

if __name__ == "__main__":
    deploy()
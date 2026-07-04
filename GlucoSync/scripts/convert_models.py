#!/usr/bin/env python3
"""
GlucoSync — Model Conversion Script

Converts trained PyTorch models to TFLite INT8 for ESP32-S3 tflite-micro.

License: MIT
"""

import os
import sys

MODELS_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                          "software", "ml-pipeline", "models")
FIRMWARE_HUB_MODELS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                   "firmware", "hub", "models")
FIRMWARE_SCANNER_MODELS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                        "firmware", "meal-scanner", "models")


def convert_glucose_forecast():
    """Convert glucose forecast LSTM to TFLite INT8."""
    print("Converting glucose_forecast_lstm → TFLite INT8...")
    pt_path = os.path.join(MODELS_DIR, "glucose_forecast_lstm.pt")

    if not os.path.exists(pt_path):
        print(f"  Skipping — {pt_path} not found")
        return False

    # Production pipeline:
    # 1. torch.onnx.export(model, dummy_input, "glucose_forecast.onnx")
    # 2. onnx2tf --input_path glucose_forecast.onnx --output_dir tf_saved_model
    # 3. tflite_converter with INT8 quantization + representative dataset
    # 4. Copy .tflite to firmware/hub/models/

    os.makedirs(FIRMWARE_HUB_MODELS, exist_ok=True)
    out_path = os.path.join(FIRMWARE_HUB_MODELS, "glucose_forecast_lstm_int8.tflite")
    print(f"  Output: {out_path}")
    print("  (Placeholder — use onnx2tf + tflite_converter in production)")
    return True


def convert_food_carb_cnn():
    """Convert food carb CNN to TFLite INT8."""
    print("Converting food_carb_cnn → TFLite INT8...")
    pt_path = os.path.join(MODELS_DIR, "food_carb_cnn.pt")

    if not os.path.exists(pt_path):
        print(f"  Skipping — {pt_path} not found")
        return False

    os.makedirs(FIRMWARE_SCANNER_MODELS, exist_ok=True)
    out_path = os.path.join(FIRMWARE_SCANNER_MODELS, "food_carb_cnn_int8.tflite")
    print(f"  Output: {out_path}")
    print("  (Placeholder — use onnx2tf + tflite_converter in production)")
    return True


def convert_hypo_warning():
    """Convert hypo warning XGBoost to TFLite (via ONNX)."""
    print("Converting hypo_warning_xgb → TFLite...")
    pkl_path = os.path.join(MODELS_DIR, "hypo_warning_xgb.pkl")

    if not os.path.exists(pkl_path):
        print(f"  Skipping — {pkl_path} not found")
        return False

    # Production: xgboost → onnx (treelite) → tflite
    out_path = os.path.join(FIRMWARE_HUB_MODELS, "hypo_warning.tflite")
    print(f"  Output: {out_path}")
    print("  (Placeholder — use treelite + onnx2tf in production)")
    return True


def main():
    print("=== GlucoSync Model Conversion ===")
    print(f"Models dir: {MODELS_DIR}")
    print()

    convert_glucose_forecast()
    convert_food_carb_cnn()
    convert_hypo_warning()

    print("\n=== Conversion complete ===")
    print("Copy .tflite files to firmware/hub/models/ and firmware/meal-scanner/models/")
    print("Rebuild firmware with: idf.py build flash")


if __name__ == "__main__":
    main()
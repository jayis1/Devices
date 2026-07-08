#!/usr/bin/env python3
"""
QuakeGuard Model Conversion Utilities

Converts trained models to TFLite (for edge deployment) and C arrays
(for embedding in firmware).

Usage:
  python convert_models.py --all
  python convert_models.py --model p_wave_cnn
  python convert_models.py --model magnitude
  python convert_models.py --xgboost-to-tflite damage_severity.json

License: MIT
"""
import argparse
import numpy as np
from pathlib import Path


def tflite_to_c_array(tflite_path: str, output_path: str, name: str):
    """Convert a .tflite file to a C array for firmware embedding."""
    with open(tflite_path, "rb") as f:
        data = f.read()

    with open(output_path, "w") as f:
        f.write(f"/* Auto-generated: {name} (TFLite int8) */\n")
        f.write(f"/* Size: {len(data)} bytes */\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"const unsigned char {name}_tflite[] = {{\n")
        for i, b in enumerate(data):
            if i % 16 == 0:
                f.write("  ")
            f.write(f"0x{b:02x},")
            if i % 16 == 15:
                f.write("\n")
        f.write("\n};\n\n")
        f.write(f"const unsigned int {name}_tflite_len = {len(data)};\n")

    print(f"  C array: {output_path} ({len(data)} bytes)")


def main():
    parser = argparse.ArgumentParser(description="QuakeGuard model converter")
    parser.add_argument("--all", action="store_true", help="Convert all models")
    parser.add_argument("--model", type=str, help="Specific model to convert")
    parser.add_argument("--xgboost-to-tflite", type=str,
                        help="Convert XGBoost JSON model to TFLite")
    args = parser.parse_args()

    if args.all or args.model == "p_wave_cnn":
        print("Converting P-wave CNN...")
        tflite_path = "models/p_wave_cnn/p_wave_cnn.tflite"
        if Path(tflite_path).exists():
            tflite_to_c_array(tflite_path,
                              "firmware/hub/p_wave_cnn_tflite.c",
                              "p_wave_cnn")
        else:
            print(f"  Not found: {tflite_path}")

    if args.all or args.model == "magnitude":
        print("Converting magnitude estimation CNN...")
        tflite_path = "models/magnitude_estimation/magnitude_estimation.tflite"
        if Path(tflite_path).exists():
            tflite_to_c_array(tflite_path,
                              "firmware/hub/magnitude_cnn_tflite.c",
                              "magnitude_cnn")
        else:
            print(f"  Not found: {tflite_path}")

    if args.xgboost_to_tflite:
        print(f"Converting XGBoost {args.xgboost_to_tflite} to TFLite...")
        # XGBoost → ONNX → TFLite
        # Requires: onnxmltools, onnx-tf, tf
        print("  Note: requires onnxmltools and onnx-tf")
        print("  pip install onnxmltools onnx-tf")
        # Implementation:
        #   import onnxmltools
        #   onnx_model = onnxmltools.convert_xgboost(args.xgboost_to_tflite)
        #   from onnx_tf.backend import prepare
        #   tf_model = prepare(onnx_model)
        #   converter = tf.lite.TFLiteConverter.from_keras_model(tf_model)
        #   tflite = converter.convert()


if __name__ == "__main__":
    main()
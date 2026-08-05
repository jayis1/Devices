"""
SeizureSync — Export trained models to TensorFlow Lite (int8 quantized)
for on-device inference on ESP32-S3 / nRF52840.
SPDX-License-Identifier: MIT
"""
import tensorflow as tf
import numpy as np
import os


def export_to_tflite(h5_path, output_path, input_shape, representative_data=None):
    """Export a Keras .h5 model to int8 quantized .tflite."""
    model = tf.keras.models.load_model(h5_path)

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    # Representative dataset for quantization calibration
    if representative_data is not None:
        converter.representative_dataset = lambda: representative_data
    else:
        # Default: random representative data
        def rep_data():
            for _ in range(100):
                yield [np.random.randn(1, *input_shape).astype(np.float32)]
        converter.representative_dataset = rep_data

    tflite_model = converter.convert()
    with open(output_path, "wb") as f:
        f.write(tflite_model)
    print(f"Exported {h5_path} → {output_path} ({len(tflite_model)} bytes)")


if __name__ == "__main__":
    os.makedirs("models", exist_ok=True)

    # SeizureNet → seizurenet_v1.tflite (ESP32-S3 band)
    if os.path.exists("models/seizurennet_v1.h5"):
        export_to_tflite("models/seizurennet_v1.h5",
                         "models/seizurennet_v1.tflite",
                         (4000, 3))

    # SUDEPNet → sudepnet_v1.tflite (ESP32-S3 hub)
    if os.path.exists("models/sudepnet_v1.h5"):
        export_to_tflite("models/sudepnet_v1.h5",
                         "models/sudepnet_v1.tflite",
                         (7500, 2))

    # AuraNet → auranet_v1.tflite (nRF52840 patch, optional)
    if os.path.exists("models/auranet_v1.h5"):
        export_to_tflite("models/auranet_v1.h5",
                         "models/auranet_v1.tflite",
                         (600, 3))
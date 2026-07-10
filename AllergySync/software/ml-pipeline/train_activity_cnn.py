"""
AllergySync — Activity Classifier (TinyCNN for nRF52840)
========================================================
6-class activity classifier for the wearable tag.
Input: 6-axis IMU (accel_x/y/z, gyro_x/y/z) at 50 Hz, 1s windows
Output: [static, walking, running, indoor, outdoor, commuting]

Architecture:
  Input(300, 6) → Conv1D(16, k=5) → ReLU → MaxPool(2)
  → Conv1D(32, k=3) → ReLU → MaxPool(2)
  → Flatten → Dense(32) → ReLU → Dense(6) → Softmax

~8K parameters, quantized to int8 for nRF52840 (~8KB)
"""

import tensorflow as tf
import numpy as np

WINDOW_SIZE = 300  # 6 seconds at 50 Hz × 6 axes
N_AXIS = 6
N_CLASSES = 6

def build_activity_cnn():
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(WINDOW_SIZE, N_AXIS)),
        tf.keras.layers.Conv1D(16, kernel_size=5, activation="relu"),
        tf.keras.layers.MaxPool1D(2),
        tf.keras.layers.Conv1D(32, kernel_size=3, activation="relu"),
        tf.keras.layers.MaxPool1D(2),
        tf.keras.layers.Flatten(),
        tf.keras.layers.Dense(32, activation="relu"),
        tf.keras.layers.Dense(N_CLASSES, activation="softmax"),
    ])
    model.compile(optimizer="adam",
                  loss="sparse_categorical_crossentropy",
                  metrics=["accuracy"])
    model.summary()
    return model


def generate_synthetic_activity_data(n_samples=5000):
    """Generate synthetic IMU data for activity classes."""
    rng = np.random.default_rng(42)
    X = np.zeros((n_samples, WINDOW_SIZE, N_AXIS), dtype=np.float32)
    y = np.zeros(n_samples, dtype=np.int32)

    # Activity templates
    templates = {
        0: {"accel_mag": (1.0, 0.2), "freq": 0.1, "label": "static"},
        1: {"accel_mag": (2.0, 0.5), "freq": 2.0, "label": "walking"},
        2: {"accel_mag": (4.0, 1.0), "freq": 3.5, "label": "running"},
        3: {"accel_mag": (1.2, 0.3), "freq": 0.5, "label": "indoor"},
        4: {"accel_mag": (1.8, 0.4), "freq": 1.5, "label": "outdoor"},
        5: {"accel_mag": (1.5, 0.3), "freq": 1.0, "label": "commuting"},
    }

    for i in range(n_samples):
        cls = i % N_CLASSES
        t = templates[cls]
        for j in range(WINDOW_SIZE):
            t_sec = j / 50.0
            mag_mean, mag_std = t["accel_mag"]
            # Sine wave at characteristic frequency + noise
            for axis in range(3):
                X[i, j, axis] = (mag_mean * np.sin(2 * np.pi * t["freq"] * t_sec)
                                + rng.normal(0, mag_std))
            for axis in range(3, 6):
                X[i, j, axis] = rng.normal(0, mag_std * 0.5)
        y[i] = cls

    idx = rng.permutation(n_samples)
    return X[idx], y[idx]


def export_tflite_int8(model, output_path="activity_cnn_int8.tflite"):
    """Export as int8 quantized TFLite model for nRF52840."""
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_model = converter.convert()
    with open(output_path, "wb") as f:
        f.write(tflite_model)
    print(f"Exported int8 TFLite: {output_path} ({len(tflite_model)} bytes)")


if __name__ == "__main__":
    model = build_activity_cnn()
    X, y = generate_synthetic_activity_data(5000)
    split = int(0.8 * len(X))
    model.fit(X[:split], y[:split], validation_data=(X[split:], y[split:]),
              epochs=20, batch_size=32, verbose=1)
    loss, acc = model.evaluate(X[split:], y[split:], verbose=0)
    print(f"Validation accuracy: {acc:.4f}")
    export_tflite_int8(model)
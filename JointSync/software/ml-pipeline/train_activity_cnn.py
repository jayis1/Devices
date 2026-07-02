"""
JointSync ML Pipeline — Activity Classifier (TinyCNN for nRF52840)

Trains a tiny CNN for 6-class activity classification from 3-second IMU windows.
Classes: rest, walk, climb, sit, run, cycle

License: MIT
"""

import numpy as np
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import os

# ─────────────────────────────────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────────────────────────────────

WINDOW_SAMPLES = 300  # 3 sec × 100 Hz
NUM_CHANNELS = 6     # ax, ay, az, gx, gy, gz
NUM_CLASSES = 6
EPOCHS = 30
BATCH_SIZE = 64

CLASS_NAMES = ["rest", "walk", "climb", "sit", "run", "cycle"]

# ─────────────────────────────────────────────────────────────────────
# Model (TinyCNN — designed for nRF52840 with tflite-micro)
# ─────────────────────────────────────────────────────────────────────

def build_activity_cnn():
    """Build a tiny 1D CNN for activity classification."""
    model = keras.Sequential([
        layers.Input(shape=(WINDOW_SAMPLES, NUM_CHANNELS)),
        layers.Conv1D(16, 5, activation='relu', padding='same'),
        layers.MaxPooling1D(4),
        layers.Conv1D(8, 3, activation='relu', padding='same'),
        layers.MaxPooling1D(4),
        layers.Flatten(),
        layers.Dense(16, activation='relu'),
        layers.Dense(NUM_CLASSES, activation='softmax'),
    ])
    model.compile(optimizer='adam', loss='sparse_categorical_crossentropy', metrics=['accuracy'])
    return model

# ─────────────────────────────────────────────────────────────────────
# Synthetic Data
# ─────────────────────────────────────────────────────────────────────

def generate_synthetic_data(n=5000):
    """Generate synthetic activity data."""
    np.random.seed(42)
    X = np.zeros((n, WINDOW_SAMPLES, NUM_CHANNELS))
    y = np.zeros(n, dtype=int)

    for i in range(n):
        cls = i % NUM_CLASSES
        y[i] = cls

        for t in range(WINDOW_SAMPLES):
            if cls == 0:  # rest
                X[i, t, 0] = 0 + np.random.normal(0, 50)
                X[i, t, 1] = 0 + np.random.normal(0, 50)
                X[i, t, 2] = 1000 + np.random.normal(0, 50)  # gravity
                X[i, t, 3:6] = np.random.normal(0, 5, 3)
            elif cls == 1:  # walk
                freq = 2.0  # 2 Hz step frequency
                X[i, t, 0] = 200 * np.sin(2 * np.pi * freq * t / 100)
                X[i, t, 1] = np.random.normal(0, 100)
                X[i, t, 2] = 1000 + 100 * np.sin(2 * np.pi * freq * t / 100)
                X[i, t, 3] = 50 * np.sin(2 * np.pi * freq * t / 100)
                X[i, t, 4] = np.random.normal(0, 20)
                X[i, t, 5] = 30 * np.sin(2 * np.pi * freq * t / 100)
            elif cls == 2:  # climb
                freq = 1.5
                X[i, t, 0] = 300 * np.sin(2 * np.pi * freq * t / 100)
                X[i, t, 1] = 200 * np.sin(2 * np.pi * freq * t / 100 + np.pi/4)
                X[i, t, 2] = 1000 + 200 * np.sin(2 * np.pi * freq * t / 100)
                X[i, t, 3:6] = np.random.normal(0, 30, 3)
            elif cls == 3:  # sit
                X[i, t, 0] = np.random.normal(0, 30)
                X[i, t, 1] = np.random.normal(0, 30)
                X[i, t, 2] = 1000 + np.random.normal(0, 30)
                X[i, t, 3:6] = np.random.normal(0, 3, 3)
            elif cls == 4:  # run
                freq = 3.0
                X[i, t, 0] = 800 * np.sin(2 * np.pi * freq * t / 100)
                X[i, t, 1] = 400 * np.sin(2 * np.pi * freq * t / 100 + np.pi/3)
                X[i, t, 2] = 1000 + 400 * np.sin(2 * np.pi * freq * t / 100)
                X[i, t, 3:6] = np.random.normal(0, 80, 3)
            elif cls == 5:  # cycle
                freq = 1.0
                X[i, t, 0] = 100 * np.sin(2 * np.pi * freq * t / 100)
                X[i, t, 1] = np.random.normal(0, 50)
                X[i, t, 2] = 1000 + np.random.normal(0, 50)
                X[i, t, 3] = 100 * np.sin(2 * np.pi * freq * t / 100)
                X[i, t, 4:6] = np.random.normal(0, 20, 2)

    return X, y

# ─────────────────────────────────────────────────────────────────────
# Train
# ─────────────────────────────────────────────────────────────────────

def train_activity_cnn():
    print("=== JointSync Activity CNN Training ===")

    X, y = generate_synthetic_data(5000)
    split = int(0.8 * len(X))
    X_train, X_val = X[:split], X[split:]
    y_train, y_val = y[:split], y[split:]

    model = build_activity_cnn()
    model.summary()

    model.fit(X_train, y_train, epochs=EPOCHS, batch_size=BATCH_SIZE,
              validation_data=(X_val, y_val), verbose=1)

    val_loss, val_acc = model.evaluate(X_val, y_val)
    print(f"Validation accuracy: {val_acc:.4f}")

    # Quantize
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]

    def representative_dataset():
        for i in range(100):
            yield [X_train[i:i+1].astype(np.float32)]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_types = [tf.int8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    quantized = converter.convert()

    os.makedirs("models", exist_ok=True)
    with open("models/activity_cnn_quant.tflite", "wb") as f:
        f.write(quantized)

    print(f"Quantized model: {len(quantized)} bytes ({len(quantized)/1024:.1f} KB)")
    print("Saved: models/activity_cnn_quant.tflite")

    # Generate C header
    with open("models/activity_cnn_model.h", "w") as f:
        f.write("/* Auto-generated activity CNN model */\n")
        f.write("#ifndef ACTIVITY_CNN_MODEL_H\n#define ACTIVITY_CNN_MODEL_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("const unsigned char activity_cnn_model_data[] = {\n")
        for i in range(0, len(quantized), 12):
            chunk = quantized[i:i+12]
            f.write("  " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
        f.write("};\n\n")
        f.write(f"const unsigned int activity_cnn_model_len = {len(quantized)};\n\n")
        f.write("#endif\n")

    print(f"C header: models/activity_cnn_model.h")

    return model

if __name__ == "__main__":
    train_activity_cnn()
    print("\nActivity CNN training complete!")
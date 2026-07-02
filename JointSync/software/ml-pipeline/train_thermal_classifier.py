"""
JointSync ML Pipeline — Thermal Swelling Classifier (MobileNetV3-Small)

Classifies 32×24 thermal images into 4 swelling grades (0-3).

License: MIT
"""

import numpy as np
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import os

NUM_CLASSES = 4  # swelling grade 0-3
EPOCHS = 50
BATCH_SIZE = 32

def build_thermal_classifier():
    """Build a MobileNetV3-Small-based classifier for 32×24 thermal images."""
    # Expand 32×24×1 to 96×72×3 by interpolation (for MobileNetV3)
    inputs = layers.Input(shape=(32, 24, 1))

    # Upsample to at least 32×32 for MobileNetV3
    x = layers.UpSampling2D(size=(3, 4))(inputs)  # 96×96×1
    x = layers.Concatenate()([x, x, x])  # 96×96×3 (grayscale to 3-channel)

    # MobileNetV3-Small backbone
    base = keras.applications.MobileNetV3Small(
        input_shape=(96, 96, 3),
        include_top=False,
        weights=None,
        minimalistic=True,
    )
    x = base(x)
    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dropout(0.2)(x)
    x = layers.Dense(32, activation='relu')(x)
    outputs = layers.Dense(NUM_CLASSES, activation='softmax')(x)

    model = keras.Model(inputs, outputs)
    model.compile(optimizer='adam', loss='sparse_categorical_crossentropy', metrics=['accuracy'])
    return model

def generate_synthetic_thermal_data(n=2000):
    """Generate synthetic thermal scan data with swelling labels."""
    np.random.seed(42)
    X = np.zeros((n, 32, 24, 1))
    y = np.zeros(n, dtype=int)

    for i in range(n):
        grade = i % NUM_CLASSES
        y[i] = grade

        # Base temperature ~32°C, range varies by grade
        base_temp = 30.0 + grade * 1.5
        hotspot_temp = base_temp + grade * 2.0

        # Generate thermal field
        for r in range(32):
            for c in range(24):
                # Gaussian hotspot in center
                dist = np.sqrt((r - 16) ** 2 + (c - 12) ** 2)
                temp = base_temp + (hotspot_temp - base_temp) * np.exp(-dist ** 2 / 50)
                temp += np.random.normal(0, 0.5)
                X[i, r, c, 0] = temp

    # Normalize to [0, 1]
    X = (X - 25.0) / 15.0
    X = np.clip(X, 0, 1)

    return X, y

def train_thermal_classifier():
    print("=== JointSync Thermal Swelling Classifier ===")

    X, y = generate_synthetic_thermal_data(2000)
    split = int(0.8 * len(X))
    X_train, X_val = X[:split], X[split:]
    y_train, y_val = y[:split], y[split:]

    model = build_thermal_classifier()
    model.summary()

    model.fit(X_train, y_train, epochs=EPOCHS, batch_size=BATCH_SIZE,
              validation_data=(X_val, y_val), verbose=1)

    val_loss, val_acc = model.evaluate(X_val, y_val)
    print(f"Validation accuracy: {val_acc:.4f}")

    # Save
    os.makedirs("models", exist_ok=True)
    model.save("models/thermal_classifier.h5")

    # Export ONNX
    try:
        import torch
        dummy = torch.randn(1, 32, 24, 1)
        print("ONNX export skipped (use tf.lite for edge deployment)")
    except:
        pass

    # TFLite for edge
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    tflite_model = converter.convert()
    with open("models/thermal_classifier.tflite", "wb") as f:
        f.write(tflite_model)
    print(f"TFLite model: {len(tflite_model)} bytes ({len(tflite_model)/1024:.1f} KB)")

    return model

if __name__ == "__main__":
    train_thermal_classifier()
    print("\nThermal classifier training complete!")
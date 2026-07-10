"""
AllergySync — PollenNet Model Definition
=========================================
1D-CNN for pollen type classification from PM size distribution.

Input:  [batch, 8] — 8 PM size bins from SPS30 sensor
Output: [batch, 6] — probabilities for 6 pollen classes
        (birch, grass, ragweed, oak, pine, mold)

Architecture:
  Input(8) → Conv1D(16, k=3) → ReLU → Conv1D(32, k=3) → ReLU
  → Flatten → Dense(64) → ReLU → Dense(6) → Softmax

Total params: ~4,500 (fits in 32KB TFLM arena on ESP32-S3)

Exported as tflite-micro model for on-device inference.
"""

import tensorflow as tf
import numpy as np

def build_pollennet():
    """Build the PollenNet 1D-CNN model."""
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(8, 1), name="pm_bins"),
        tf.keras.layers.Conv1D(16, kernel_size=3, padding="same",
                              activation="relu", name="conv1"),
        tf.keras.layers.Conv1D(32, kernel_size=3, padding="same",
                              activation="relu", name="conv2"),
        tf.keras.layers.Flatten(name="flatten"),
        tf.keras.layers.Dense(64, activation="relu", name="dense1"),
        tf.keras.layers.Dense(6, activation="softmax", name="output"),
    ])

    model.compile(
        optimizer="adam",
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"]
    )

    model.summary()
    return model


def export_tflite(model, output_path="pollennet_model.tflite"):
    """Export model as TensorFlow Lite for microcontrollers."""
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.target_spec.supported_ops = [
        tf.lite.OpsSet.TFLITE_BUILTINS,
    ]
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_types = [tf.float16]

    tflite_model = converter.convert()

    with open(output_path, "wb") as f:
        f.write(tflite_model)

    print(f"Exported TFLite model: {output_path} ({len(tflite_model)} bytes)")

    # Also export as C header for embedding in firmware
    header_path = output_path.replace(".tflite", "_model.h")
    with open(header_path, "w") as f:
        f.write("/* Auto-generated PollenNet model data */\n")
        f.write("#include <stdint.h>\n\n")
        f.write("alignas(16) const unsigned char pollennet_model_data[] = {\n")
        for i in range(0, len(tflite_model), 12):
            chunk = tflite_model[i:i+12]
            f.write("  " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
        f.write("};\n")
        f.write(f"const unsigned int pollennet_model_data_len = {len(tflite_model)};\n")

    print(f"Exported C header: {header_path}")
    return tflite_model


def generate_synthetic_training_data(n_samples=10000):
    """
    Generate synthetic training data for PollenNet.
    In production, this would be replaced with real PM data labeled
    by a co-located reference pollen trap (Hirst-type).

    Pollen size signatures (approximate):
      Birch:   ~22 µm → high count_10, moderate count_05
      Grass:   ~25-30 µm → high count_10, high count_05
      Ragweed: ~15-20 µm → moderate count_10, high count_05, high count_03
      Oak:     ~28 µm → high count_10, moderate count_05
      Pine:    ~50 µm → very high count_10, low count_05
      Mold:    ~3-10 µm → very high count_03, moderate count_05
    """
    rng = np.random.default_rng(42)

    # Base templates for each pollen type
    templates = {
        0: {"count_03": (500, 200), "count_05": (300, 150), "count_10": (50, 30),
            "pm1": (5, 3), "pm2_5": (8, 4), "pm4": (12, 5), "pm10": (15, 7)},  # birch
        1: {"count_03": (600, 200), "count_05": (400, 150), "count_10": (80, 40),
            "pm1": (6, 3), "pm2_5": (10, 4), "pm4": (15, 5), "pm10": (20, 7)},  # grass
        2: {"count_03": (800, 200), "count_05": (500, 150), "count_10": (40, 20),
            "pm1": (4, 2), "pm2_5": (7, 3), "pm4": (10, 4), "pm10": (12, 5)},  # ragweed
        3: {"count_03": (550, 200), "count_05": (350, 150), "count_10": (60, 30),
            "pm1": (5, 3), "pm2_5": (9, 4), "pm4": (13, 5), "pm10": (18, 7)},  # oak
        4: {"count_03": (400, 200), "count_05": (200, 100), "count_10": (120, 50),
            "pm1": (8, 4), "pm2_5": (15, 5), "pm4": (25, 8), "pm10": (40, 12)}, # pine
        5: {"count_03": (1200, 300), "count_05": (300, 100), "count_10": (20, 10),
            "pm1": (3, 2), "pm2_5": (5, 2), "pm4": (7, 3), "pm10": (8, 3)},   # mold
    }

    X = []
    y = []

    for label, tmpl in templates.items():
        n = n_samples // len(templates)
        for _ in range(n):
            sample = []
            for key in ["count_03", "count_05", "count_10", "pm1", "pm2_5", "pm4", "pm10"]:
                mean, std = tmpl[key]
                val = max(0, rng.normal(mean, std))
                sample.append(val)
            # 8th feature: ratio count_10 / count_03
            ratio = sample[2] / (sample[0] + 1)
            sample.append(ratio)

            X.append(sample)
            y.append(label)

    X = np.array(X, dtype=np.float32)
    y = np.array(y, dtype=np.int32)

    # Shuffle
    idx = rng.permutation(len(X))
    X, y = X[idx], y[idx]

    return X, y


if __name__ == "__main__":
    # Build model
    model = build_pollennet()

    # Generate synthetic training data
    X, y = generate_synthetic_training_data(10000)
    print(f"Training data: X={X.shape}, y={y.shape}")

    # Split
    split = int(0.8 * len(X))
    X_train, X_val = X[:split], X[split:]
    y_train, y_val = y[:split], y[split:]

    # Train
    history = model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=50,
        batch_size=32,
        verbose=1
    )

    # Evaluate
    loss, acc = model.evaluate(X_val, y_val, verbose=0)
    print(f"Validation accuracy: {acc:.4f}")

    # Export for tflite-micro
    export_tflite(model, "pollennet_model.tflite")
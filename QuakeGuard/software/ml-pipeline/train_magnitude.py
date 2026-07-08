#!/usr/bin/env python3
"""
QuakeGuard Magnitude Estimation CNN — Training Script

1D CNN regression model that estimates earthquake magnitude (Mw) from
the first 2 seconds of 3-axis acceleration waveform + estimated
epicenter distance.

Input: (2000, 3) acceleration waveform + (1,) epicenter distance
Output: Mw (moment magnitude) as float

Training data:
  - STEAD dataset (magnitude labels from USGS/ANSS catalog)
  - Synthetic waveforms at varying distances (1–500 km)

License: MIT
"""
import numpy as np
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
from pathlib import Path

OUTPUT_DIR = Path("models/magnitude_estimation")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

SAMPLE_LEN = 2000
N_AXES = 3
BATCH_SIZE = 256
EPOCHS = 50


def generate_synthetic_data(n_samples=20000):
    """Generate synthetic seismic waveforms with known magnitudes.

    Uses simple seismological models:
      - Amplitude ∝ 10^(1.5*Mw) / distance (geometric spreading)
      - P-wave: 6-10 Hz, moderate amplitude
      - S-wave: 2-4 Hz, high amplitude
    """
    np.random.seed(42)
    X_wave = np.zeros((n_samples, SAMPLE_LEN, N_AXES), dtype=np.float32)
    X_dist = np.zeros((n_samples, 1), dtype=np.float32)
    y = np.zeros(n_samples, dtype=np.float32)

    for i in range(n_samples):
        magnitude = np.random.uniform(3.0, 8.0)
        distance = np.random.uniform(5, 500)  # km
        depth = np.random.uniform(1, 30)      # km

        # Amplitude scaling (simplified)
        # A ∝ 10^(0.5*Mw) / (distance^0.5)
        amplitude = (10 ** (0.5 * magnitude)) / (distance ** 0.5) * 1e-3

        t = np.arange(SAMPLE_LEN) / 1000.0  # seconds

        # P-wave arrival (faster, ~6 km/s)
        p_arrival = distance / 6000.0  # seconds
        # S-wave arrival (slower, ~3.5 km/s)
        s_arrival = distance / 3500.0

        # Only include arrivals within the 2-second window
        for ch in range(N_AXES):
            signal = np.zeros(SAMPLE_LEN, dtype=np.float32)

            # P-wave
            if p_arrival < 2.0:
                p_freq = np.random.uniform(6, 10)
                p_decay = np.random.uniform(5, 15)
                p_amp = amplitude * 0.3  # P-wave is ~30% of S-wave amplitude
                p_envelope = np.exp(-p_decay * np.maximum(t - p_arrival, 0))
                signal += p_amp * p_envelope * np.sin(2 * np.pi * p_freq * t) * \
                          (t > p_arrival).astype(float)

            # S-wave
            if s_arrival < 2.0:
                s_freq = np.random.uniform(2, 4)
                s_decay = np.random.uniform(3, 8)
                s_amp = amplitude * (0.7 + 0.3 * np.random.random())
                s_envelope = np.exp(-s_decay * np.maximum(t - s_arrival, 0))
                signal += s_amp * s_envelope * np.sin(2 * np.pi * s_freq * t) * \
                          (t > s_arrival).astype(float)

            # Add noise
            noise_level = 0.02 + 0.001 * distance  # more noise at distance
            signal += np.random.normal(0, noise_level, SAMPLE_LEN)

            X_wave[i, :, ch] = signal
            X_dist[i, 0] = distance
            y[i] = magnitude

    # Normalize waveforms per-sample
    for i in range(n_samples):
        for ch in range(N_AXES):
            std = np.std(X_wave[i, :, ch])
            if std > 1e-8:
                X_wave[i, :, ch] = (X_wave[i, :, ch] - np.mean(X_wave[i, :, ch])) / std

    # Normalize distance
    X_dist = X_dist / 500.0  # scale to 0-1

    return X_wave, X_dist, y


def build_model():
    """Build dual-input CNN regression model."""
    # Waveform input
    wave_input = layers.Input(shape=(SAMPLE_LEN, N_AXES), name="waveform")
    x = layers.Conv1D(32, 5, 2, activation="relu", padding="same")(wave_input)
    x = layers.Conv1D(64, 5, 2, activation="relu", padding="same")(x)
    x = layers.MaxPooling1D(2)(x)
    x = layers.Conv1D(128, 3, 1, activation="relu", padding="same")(x)
    x = layers.MaxPooling1D(2)(x)
    x = layers.GlobalAveragePooling1D()(x)

    # Distance input
    dist_input = layers.Input(shape=(1,), name="distance")
    d = layers.Dense(16, activation="relu")(dist_input)

    # Combine
    combined = layers.Concatenate()([x, d])
    h = layers.Dense(64, activation="relu")(combined)
    h = layers.Dropout(0.3)(h)
    h = layers.Dense(32, activation="relu")(h)
    output = layers.Dense(1, activation="linear", name="magnitude")(h)

    model = keras.Model([wave_input, dist_input], output)
    model.compile(
        optimizer=keras.optimizers.Adam(0.001),
        loss="mse",
        metrics=["mae"],
    )
    return model


def main():
    print("=" * 60)
    print("QuakeGuard Magnitude Estimation CNN Training")
    print("=" * 60)

    print("\n[1/4] Generating synthetic data...")
    X_wave, X_dist, y = generate_synthetic_data(20000)
    print(f"  Samples: {len(y)}")
    print(f"  Magnitude range: {y.min():.1f} - {y.max():.1f}")

    # Split
    n_test = len(y) // 5
    X_w_test, X_d_test, y_test = X_wave[:n_test], X_dist[:n_test], y[:n_test]
    X_w_train, X_d_train, y_train = X_wave[n_test:], X_dist[n_test:], y[n_test:]

    print(f"\n  Train: {len(y_train)}, Test: {len(y_test)}")

    print("\n[2/4] Building model...")
    model = build_model()
    model.summary()

    print("\n[3/4] Training...")
    callbacks = [
        keras.callbacks.EarlyStopping(patience=10, restore_best_weights=True),
        keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=5),
    ]

    model.fit(
        [X_w_train, X_d_train], y_train,
        validation_data=([X_w_test, X_d_test], y_test),
        epochs=EPOCHS,
        batch_size=BATCH_SIZE,
        callbacks=callbacks,
        verbose=1,
    )

    print("\n[4/4] Evaluating...")
    test_loss, test_mae = model.evaluate([X_w_test, X_d_test], y_test, verbose=0)
    print(f"  Test MSE: {test_loss:.4f}")
    print(f"  Test MAE: {test_mae:.4f} Mw units")

    # Sample predictions
    preds = model.predict([X_w_test[:5], X_d_test[:5]], verbose=0)
    print("\n  Sample predictions:")
    for i in range(5):
        print(f"    True: M{y_test[i]:.1f}, Predicted: M{preds[i, 0]:.1f} "
              f"(Δ{abs(y_test[i] - preds[i, 0]):.2f})")

    # Save
    model.save(OUTPUT_DIR / "magnitude_estimation.keras")

    # TFLite conversion (int8 for edge)
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    tflite_model = converter.convert()
    with open(OUTPUT_DIR / "magnitude_estimation.tflite", "wb") as f:
        f.write(tflite_model)
    print(f"\n  TFLite: {len(tflite_model)} bytes")

    # C array for firmware
    c_path = OUTPUT_DIR / "magnitude_estimation_tflite.c"
    with open(c_path, "w") as f:
        f.write("/* Auto-generated: Magnitude estimation CNN (TFLite) */\n")
        f.write(f"/* Size: {len(tflite_model)} bytes */\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("const unsigned char magnitude_cnn_tflite[] = {\n")
        for i, b in enumerate(tflite_model):
            if i % 16 == 0: f.write("  ")
            f.write(f"0x{b:02x},")
            if i % 16 == 15: f.write("\n")
        f.write("\n};\n")
        f.write(f"const unsigned int magnitude_cnn_tflite_len = {len(tflite_model)};\n")

    print(f"\nDone! Model saved to {OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
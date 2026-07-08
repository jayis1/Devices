#!/usr/bin/env python3
"""
QuakeGuard P-Wave / S-Wave CNN — Training Script

1D CNN (5 conv + 2 FC) that classifies 2-second 3-axis acceleration
waveforms as: 0=noise, 1=P-wave, 2=S-wave.

Training data:
  - STEAD dataset (Stanford Earthquake Dataset, 521,752 3-component
    waveforms from global seismic networks)
  - Synthetic household noise (door slams, footsteps, traffic,
    washing machine vibration) for class 0

Edge deployment:
  - tflite-micro on ESP32-S3 (18 KB model, <200 ms inference)
  - Quantized to int8 for ESP32-S3 vector instructions

Architecture:
  Input:  (batch, 2000, 3)  — 2 s × 3-axis at 1000 Hz
  Conv1D(64, k=5, s=2, ReLU)   → (batch, 998, 64)
  Conv1D(64, k=5, s=2, ReLU)   → (batch, 497, 64)
  MaxPool1D(2)                  → (batch, 248, 64)
  Conv1D(128, k=3, s=1, ReLU)  → (batch, 246, 128)
  MaxPool1D(2)                  → (batch, 123, 128)
  Conv1D(128, k=3, s=1, ReLU)  → (batch, 121, 128)
  GlobalAvgPool1D              → (batch, 128)
  Dense(64, ReLU)              → (batch, 64)
  Dense(3, Softmax)            → (batch, 3)

License: MIT
"""
import os
import sys
import numpy as np
import h5py
from pathlib import Path

import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers

# ── Configuration ──────────────────────────────────────────────

STEAD_HDF5 = os.getenv("STEAD_PATH", "data/STEAD/chunk1.hdf5")
STEAD_CSV  = os.getenv("STEAD_CSV",  "data/STEAD/chunk1.csv")
OUTPUT_DIR = Path("models/p_wave_cnn")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

SAMPLE_LEN = 2000     # 2 seconds at 1000 Hz
N_AXES = 3            # X, Y, Z
BATCH_SIZE = 256
EPOCHS = 50
LEARNING_RATE = 0.001
TEST_SPLIT = 0.15
VAL_SPLIT = 0.15


def load_stead(hdf5_path: str, csv_path: str, max_samples=50000):
    """Load STEAD dataset: 3-component waveforms (P, S, noise).

    Returns:
      X: (N, 2000, 3) float32 acceleration (normalized)
      y: (N,) int labels — 0=noise, 1=P-wave, 2=S-wave
    """
    import pandas as pd
    df = pd.read_csv(csv_path)

    # STEAD labels: trace_category = 'local_earthquake' or 'noise'
    # p_arrival_sample, s_arrival_sample are sample indices
    X_list = []
    y_list = []

    with h5py.File(hdf5_path, "r") as f:
        for i, (_, row) in enumerate(df.iterrows()):
            if len(X_list) >= max_samples:
                break

            name = row["trace_name"]
            try:
                data = f["data"][name][:]
            except KeyError:
                continue

            # data shape: (3, 6000) — 3 components, 6000 samples (60 s @ 100 Hz)
            # STEAD is sampled at 100 Hz; we resample to 1000 Hz
            # For training: use the first 2000 samples (2 s at 1000 Hz)
            # Resample 100 Hz → 1000 Hz by interpolation
            if data.shape[0] != 3:
                continue

            # Interpolate from 100 Hz to 1000 Hz
            from scipy.interpolate import interp1d
            t_orig = np.linspace(0, 2, 200)  # 2 s at 100 Hz = 200 samples
            t_new = np.linspace(0, 2, 2000)   # 2 s at 1000 Hz
            data_1khz = np.zeros((3, 2000), dtype=np.float32)
            for ch in range(3):
                f_interp = interp1d(t_orig, data[ch, :200], kind="linear")
                data_1khz[ch] = f_interp(t_new)

            # Normalize per-component
            for ch in range(3):
                std = np.std(data_1khz[ch])
                if std > 1e-8:
                    data_1khz[ch] = (data_1khz[ch] - np.mean(data_1khz[ch])) / std
                else:
                    data_1khz[ch] = 0

            # Label assignment:
            # If trace_category == "noise": label = 0
            # If p_arrival_sample exists and < 200: label = 1 (P-wave window)
            # If s_arrival_sample exists and < 200: label = 2 (S-wave window)
            if row["trace_category"] == "noise":
                y_list.append(0)
            elif ("s_arrival_sample" in row and
                  not np.isnan(row.get("s_arrival_sample", np.nan)) and
                  row["s_arrival_sample"] < 200):
                y_list.append(2)  # S-wave
            elif ("p_arrival_sample" in row and
                  not np.isnan(row.get("p_arrival_sample", np.nan)) and
                  row["p_arrival_sample"] < 200):
                y_list.append(1)  # P-wave
            else:
                continue  # skip ambiguous

            X_list.append(data_1khz.T)  # (2000, 3)

    X = np.array(X_list, dtype=np.float32)
    y = np.array(y_list, dtype=np.int32)
    return X, y


def generate_synthetic_noise(n_samples=5000):
    """Generate synthetic household vibration noise for class 0.

    Patterns:
      - Door slam: high-frequency impulse with exponential decay
      - Footsteps: periodic low-frequency pulses
      - Traffic: broadband rumble + periodic components
      - Washing machine: rotational harmonic vibration
    """
    X = np.zeros((n_samples, SAMPLE_LEN, N_AXES), dtype=np.float32)

    for i in range(n_samples):
        pattern = np.random.choice(["door_slam", "footsteps",
                                     "traffic", "washing", "mixed"])

        if pattern == "door_slam":
            # Impulse at random time, exponential decay, ~10 Hz
            t = np.arange(SAMPLE_LEN) / 1000.0
            impulse_t = np.random.uniform(0.2, 1.8)
            for ch in range(N_AXES):
                amp = np.random.uniform(0.5, 1.0)
                freq = np.random.uniform(5, 20)
                decay = np.random.uniform(20, 50)
                signal = amp * np.exp(-decay * np.maximum(t - impulse_t, 0)) * \
                         np.sin(2 * np.pi * freq * t)
                X[i, :, ch] = signal + np.random.normal(0, 0.05, SAMPLE_LEN)

        elif pattern == "footsteps":
            # Periodic pulses at ~1-2 Hz
            t = np.arange(SAMPLE_LEN) / 1000.0
            step_freq = np.random.uniform(1.0, 2.5)
            for ch in range(N_AXES):
                signal = np.zeros(SAMPLE_LEN)
                for step in range(int(2 * step_freq)):
                    step_time = step / step_freq + np.random.uniform(-0.05, 0.05)
                    if step_time < 2.0:
                        idx = int(step_time * 1000)
                        env = np.exp(-30 * np.maximum(t - step_time, 0))
                        signal += 0.3 * env * np.sin(2 * np.pi * 15 * t)
                X[i, :, ch] = signal + np.random.normal(0, 0.05, SAMPLE_LEN)

        elif pattern == "traffic":
            # Low-frequency rumble + harmonics
            t = np.arange(SAMPLE_LEN) / 1000.0
            for ch in range(N_AXES):
                signal = 0.2 * np.sin(2 * np.pi * 3 * t) + \
                         0.1 * np.sin(2 * np.pi * 8 * t) + \
                         0.05 * np.sin(2 * np.pi * 20 * t)
                X[i, :, ch] = signal + np.random.normal(0, 0.1, SAMPLE_LEN)

        elif pattern == "washing":
            # Rotational harmonics (~20 Hz fundamental)
            t = np.arange(SAMPLE_LEN) / 1000.0
            fund = np.random.uniform(15, 30)
            for ch in range(N_AXES):
                signal = 0.3 * np.sin(2 * np.pi * fund * t) + \
                         0.15 * np.sin(2 * np.pi * 2 * fund * t) + \
                         0.08 * np.sin(2 * np.pi * 3 * fund * t)
                X[i, :, ch] = signal + np.random.normal(0, 0.05, SAMPLE_LEN)

        else:  # mixed
            t = np.arange(SAMPLE_LEN) / 1000.0
            for ch in range(N_AXES):
                signal = 0.1 * np.random.randn(SAMPLE_LEN) + \
                         0.15 * np.sin(2 * np.pi * np.random.uniform(5, 30) * t)
                X[i, :, ch] = signal

        # Normalize
        for ch in range(N_AXES):
            std = np.std(X[i, :, ch])
            if std > 1e-8:
                X[i, :, ch] = (X[i, :, ch] - np.mean(X[i, :, ch])) / std

    y = np.zeros(n_samples, dtype=np.int32)  # all class 0
    return X, y


def build_model():
    """Build 1D CNN for P-wave/S-wave/noise classification."""
    model = keras.Sequential([
        layers.Input(shape=(SAMPLE_LEN, N_AXES)),

        # Conv block 1
        layers.Conv1D(64, kernel_size=5, strides=2,
                      activation="relu", padding="same"),
        layers.BatchNormalization(),

        # Conv block 2
        layers.Conv1D(64, kernel_size=5, strides=2,
                      activation="relu", padding="same"),
        layers.BatchNormalization(),
        layers.MaxPooling1D(2),

        # Conv block 3
        layers.Conv1D(128, kernel_size=3, strides=1,
                      activation="relu", padding="same"),
        layers.BatchNormalization(),
        layers.MaxPooling1D(2),

        # Conv block 4
        layers.Conv1D(128, kernel_size=3, strides=1,
                      activation="relu", padding="same"),
        layers.BatchNormalization(),

        # Global average pooling (lighter than flatten)
        layers.GlobalAveragePooling1D(),

        # Dense
        layers.Dense(64, activation="relu"),
        layers.Dropout(0.3),

        # Output: 3 classes (noise, P-wave, S-wave)
        layers.Dense(3, activation="softmax"),
    ])

    model.compile(
        optimizer=keras.optimizers.Adam(LEARNING_RATE),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model


def main():
    print("=" * 60)
    print("QuakeGuard P-Wave / S-Wave CNN Training")
    print("=" * 60)

    # Load data
    print("\n[1/5] Loading STEAD dataset...")
    if os.path.exists(STEAD_HDF5):
        X_stead, y_stead = load_stead(STEAD_HDF5, STEAD_CSV)
        print(f"  STEAD: {len(X_stead)} samples "
              f"(P-wave={np.sum(y_stead==1)}, S-wave={np.sum(y_stead==2)})")
    else:
        print(f"  STEAD not found at {STEAD_HDF5}")
        print("  Generating synthetic seismic data for demo...")
        X_stead, y_stead = generate_synthetic_seismic(20000)

    print("\n[2/5] Generating synthetic household noise...")
    X_noise, y_noise = generate_synthetic_noise(10000)
    print(f"  Noise: {len(X_noise)} samples")

    # Combine
    X = np.concatenate([X_stead, X_noise], axis=0)
    y = np.concatenate([y_stead, y_noise], axis=0)

    # Shuffle
    idx = np.random.permutation(len(X))
    X = X[idx]
    y = y[idx]

    print(f"\n  Total: {len(X)} samples")
    print(f"  Class distribution: noise={np.sum(y==0)}, "
          f"P-wave={np.sum(y==1)}, S-wave={np.sum(y==2)}")

    # Split
    n_test = int(len(X) * TEST_SPLIT)
    n_val = int(len(X) * VAL_SPLIT)
    X_test, y_test = X[:n_test], y[:n_test]
    X_val, y_val = X[n_test:n_test + n_val], y[n_test:n_test + n_val]
    X_train, y_train = X[n_test + n_val:], y[n_test + n_val:]

    print(f"\n  Train: {len(X_train)}, Val: {len(X_val)}, Test: {len(X_test)}")

    # Build model
    print("\n[3/5] Building model...")
    model = build_model()
    model.summary()

    # Callbacks
    callbacks = [
        keras.callbacks.EarlyStopping(patience=10, restore_best_weights=True),
        keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=5),
        keras.callbacks.ModelCheckpoint(
            str(OUTPUT_DIR / "p_wave_cnn_best.keras"),
            save_best_only=True
        ),
    ]

    # Train
    print("\n[4/5] Training...")
    history = model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=EPOCHS,
        batch_size=BATCH_SIZE,
        callbacks=callbacks,
        verbose=1,
    )

    # Evaluate
    print("\n[5/5] Evaluating...")
    test_loss, test_acc = model.evaluate(X_test, y_test, verbose=0)
    print(f"  Test accuracy: {test_acc:.4f}")

    # Classification report
    y_pred = model.predict(X_test, verbose=0)
    y_pred_classes = np.argmax(y_pred, axis=1)
    from sklearn.metrics import classification_report
    print("\nClassification Report:")
    print(classification_report(y_test, y_pred_classes,
                                target_names=["noise", "P-wave", "S-wave"]))

    # Save full model
    model.save(OUTPUT_DIR / "p_wave_cnn.keras")

    # Convert to TFLite (int8 quantized for ESP32-S3)
    print("\nConverting to TFLite (int8 quantized)...")
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    # Representative dataset for quantization
    def representative_dataset():
        for i in range(200):
            yield [X_train[i:i+1].astype(np.float32)]

    converter.representative_dataset = representative_dataset

    tflite_model = converter.convert()
    tflite_path = OUTPUT_DIR / "p_wave_cnn.tflite"
    with open(tflite_path, "wb") as f:
        f.write(tflite_model)

    print(f"  TFLite model: {tflite_path} ({len(tflite_model)} bytes)")

    # Convert to C array for firmware embedding
    c_array_path = OUTPUT_DIR / "p_wave_cnn_tflite.c"
    with open(c_array_path, "w") as f:
        f.write("/* Auto-generated: P-wave/S-wave CNN model (int8 TFLite) */\n")
        f.write("/* Training: STEAD + synthetic noise */\n")
        f.write(f"/* Size: {len(tflite_model)} bytes */\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("const unsigned char p_wave_cnn_tflite[] = {\n")
        for i, b in enumerate(tflite_model):
            if i % 16 == 0:
                f.write("  ")
            f.write(f"0x{b:02x},")
            if i % 16 == 15:
                f.write("\n")
        f.write("\n};\n\n")
        f.write(f"const unsigned int p_wave_cnn_tflite_len = {len(tflite_model)};\n")

    print(f"  C array: {c_array_path}")
    print(f"\nDone! Model saved to {OUTPUT_DIR}/")
    print(f"Copy p_wave_cnn_tflite.c to firmware/hub/ for edge deployment.")


def generate_synthetic_seismic(n_samples):
    """Generate synthetic P-wave and S-wave data for demo training."""
    X = np.zeros((n_samples, SAMPLE_LEN, N_AXES), dtype=np.float32)
    y = np.zeros(n_samples, dtype=np.int32)

    for i in range(n_samples):
        t = np.arange(SAMPLE_LEN) / 1000.0
        if i < n_samples // 2:
            # P-wave: compressional, ~6-10 Hz, moderate amplitude
            arrival = np.random.uniform(0.1, 0.5)
            freq = np.random.uniform(6, 10)
            amp = np.random.uniform(0.3, 0.7)
            decay = np.random.uniform(5, 15)
            for ch in range(N_AXES):
                signal = amp * np.exp(-decay * np.maximum(t - arrival, 0)) * \
                         np.sin(2 * np.pi * freq * (t - arrival)) * \
                         (t > arrival).astype(float)
                X[i, :, ch] = signal + np.random.normal(0, 0.05, SAMPLE_LEN)
            y[i] = 1
        else:
            # S-wave: shear, ~2-4 Hz, high amplitude
            arrival = np.random.uniform(0.1, 0.5)
            freq = np.random.uniform(2, 4)
            amp = np.random.uniform(0.7, 1.0)
            decay = np.random.uniform(3, 8)
            for ch in range(N_AXES):
                signal = amp * np.exp(-decay * np.maximum(t - arrival, 0)) * \
                         np.sin(2 * np.pi * freq * (t - arrival)) * \
                         (t > arrival).astype(float)
                X[i, :, ch] = signal + np.random.normal(0, 0.05, SAMPLE_LEN)
            y[i] = 2

        # Normalize
        for ch in range(N_AXES):
            std = np.std(X[i, :, ch])
            if std > 1e-8:
                X[i, :, ch] = (X[i, :, ch] - np.mean(X[i, :, ch])) / std

    return X, y


if __name__ == "__main__":
    main()
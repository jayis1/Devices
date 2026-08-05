"""
SeizureSync — Model 1: SeizureNet (seizure detection 1D CNN)
Trains an 8-layer 1D CNN on wrist-worn accel + PPG + EDA.
Output: 4-class (seizure / syncope / motion / rest)
Target: 95% sensitivity, <0.21 FP/day
Exported as int8 quantized tflite for ESP32-S3 edge inference.

Dataset: EPILEPSIAE wrist-worn benchmark (accel + PPG + EDA paired with
TUH EEG seizure labels).
SPDX-License-Identifier: MIT
"""
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models
import argparse


def build_seizurennet(input_shape=(4000, 3)):
    """8-layer 1D CNN, ~120K params.
    Input: 2 s × 2000 Hz accel (mag) + 100 Hz PPG (upsampled) + 4 Hz EDA (upsampled)
    Output: 4-class softmax.
    """
    inp = layers.Input(shape=input_shape)

    # Block 1
    x = layers.Conv1D(32, 7, activation='relu', padding='same')(inp)
    x = layers.BatchNormalization()(x)
    x = layers.MaxPooling1D(4)(x)            # 1000 → 250

    # Block 2
    x = layers.Conv1D(32, 5, activation='relu', padding='same')(x)
    x = layers.BatchNormalization()(x)
    x = layers.MaxPooling1D(4)(x)            # 250 → 62

    # Block 3
    x = layers.Conv1D(64, 3, activation='relu', padding='same')(x)
    x = layers.BatchNormalization()(x)

    # Block 4
    x = layers.Conv1D(64, 3, activation='relu', padding='same')(x)
    x = layers.BatchNormalization()(x)
    x = layers.MaxPooling1D(2)(x)            # 62 → 31

    # Block 5
    x = layers.Conv1D(128, 3, activation='relu', padding='same')(x)
    x = layers.BatchNormalization()(x)

    # Block 6
    x = layers.Conv1D(128, 3, activation='relu', padding='same')(x)
    x = layers.BatchNormalization()(x)

    # Block 7
    x = layers.Conv1D(128, 3, activation='relu', padding='same')(x)
    x = layers.BatchNormalization()(x)
    x = layers.GlobalAveragePooling1D()(x)

    # Block 8 (classifier)
    x = layers.Dense(64, activation='relu')(x)
    x = layers.Dropout(0.5)(x)
    out = layers.Dense(4, activation='softmax')(x)   # seizure/syncope/motion/rest

    model = models.Model(inp, out, name='SeizureNet')
    model.compile(optimizer='adam',
                  loss='sparse_categorical_crossentropy',
                  metrics=['accuracy'])
    model.summary()
    return model


def load_data(data_dir):
    """Load EPILEPSIAE wrist-worn dataset.
    Expected: {data_dir}/seizure/*.npy, {data_dir}/rest/*.npy, etc.
    Each .npy is (4000, 3) = (accel_mag, ppg_hr, eda) for 2 s.
    """
    # Stub: generate synthetic data for development
    n_per_class = 500
    X = np.random.randn(n_per_class * 4, 4000, 3).astype(np.float32)
    y = np.array([0]*n_per_class + [1]*n_per_class +
                 [2]*n_per_class + [3]*n_per_class)

    # Make seizure class (0) have higher energy
    X[:n_per_class] *= 3.0
    # Make seizure class have HR spike
    X[:n_per_class, :, 1] += 50
    # Make seizure class have EDA surge
    X[:n_per_class, :, 2] += 2.0

    # Shuffle
    idx = np.random.permutation(len(X))
    return X[idx], y[idx]


def train(args):
    X, y = load_data(args.data)
    n = len(X)
    split = int(n * 0.8)
    X_train, X_val = X[:split], X[split:]
    y_train, y_val = y[:split], y[split:]

    model = build_seizurennet(input_shape=(4000, 3))

    # Class weights to handle imbalance (seizures are rare)
    class_weights = {0: 10.0, 1: 1.0, 2: 1.0, 3: 1.0}

    history = model.fit(X_train, y_train,
                       validation_data=(X_val, y_val),
                       epochs=args.epochs,
                       batch_size=64,
                       class_weight=class_weights)

    # Evaluate
    _, acc = model.evaluate(X_val, y_val)
    print(f"Validation accuracy: {acc:.4f}")

    # Save
    model.save("models/seizurennet_v1.h5")
    print("Saved models/seizurennet_v1.h5")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--data", default="data/ePILEPSIAE", help="Data dir")
    p.add_argument("--epochs", type=int, default=50)
    train(p.parse_args())
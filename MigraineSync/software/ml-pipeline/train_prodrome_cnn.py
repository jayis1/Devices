"""
MigraineSync — Prodrome Detector (1D-CNN)
==========================================
Trains a 6-layer 1D-CNN to detect the autonomic prodrome phase
that can precede migraine headache by 24-48 hours.

Input:  6h × 2 features (HRV variability, skin-temp slope)
Output: 3-class: normal / prodrome / migraine

Model: Conv1D(64) → Conv1D(64) → MaxPool → Conv1D(128) → Conv1D(128) → MaxPool → Dense(64) → Dense(3)
Export: TFLite quantized for ESP32-S3 edge inference

License: MIT
"""

import os
import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow.keras import layers, models
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")
MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "models")
os.makedirs(MODEL_DIR, exist_ok=True)

WINDOW_HOURS = 6
SAMPLES_PER_HOUR = 12  # 5-min intervals
WINDOW_SIZE = WINDOW_HOURS * SAMPLES_PER_HOUR  # 72 samples
FEATURES = ["hrv_rmssd", "skin_temp_c"]
N_FEATURES = len(FEATURES)
N_CLASSES = 3  # normal, prodrome, migraine


def create_sequences(df: pd.DataFrame):
    """Create 6h windows with 3-class labels.

    Class 0 = normal (no migraine in next 48h)
    Class 1 = prodrome (migraine in 24-48h — autonomic shift)
    Class 2 = migraine (migraine in 0-24h)
    """
    X, y = [], []
    patients = df["patient_id"].unique()

    for pid in patients:
        patient_df = df[df["patient_id"] == pid].reset_index(drop=True)
        data = patient_df[FEATURES].values
        labels = patient_df["migraine_onset"].values
        n = len(data)

        for i in range(WINDOW_SIZE, n - 576):  # 48h lookahead
            window = data[i - WINDOW_SIZE:i]

            # Check if migraine occurs in next 48h
            next_24h = labels[i:i + 288]      # 0-24h
            next_24_48h = labels[i + 288:i + 576]  # 24-48h

            if np.any(next_24h):
                label = 2  # migraine imminent
            elif np.any(next_24_48h):
                label = 1  # prodrome
            else:
                label = 0  # normal

            X.append(window)
            y.append(label)

    return np.array(X), np.array(y)


def build_model():
    """Build 6-layer 1D-CNN."""
    model = models.Sequential([
        layers.Input(shape=(WINDOW_SIZE, N_FEATURES)),
        layers.Conv1D(64, 5, activation="relu", padding="same"),
        layers.BatchNormalization(),
        layers.Conv1D(64, 5, activation="relu", padding="same"),
        layers.MaxPooling1D(2),
        layers.Dropout(0.3),
        layers.Conv1D(128, 3, activation="relu", padding="same"),
        layers.BatchNormalization(),
        layers.Conv1D(128, 3, activation="relu", padding="same"),
        layers.MaxPooling1D(2),
        layers.Dropout(0.3),
        layers.GlobalAveragePooling1D(),
        layers.Dense(64, activation="relu"),
        layers.Dropout(0.3),
        layers.Dense(N_CLASSES, activation="softmax"),
    ])
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model


def export_tflite_quantized(model, output_path):
    """Export quantized TFLite model for edge inference."""
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_types = [tf.int8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    def representative_data_gen():
        for _ in range(100):
            data = np.random.randn(1, WINDOW_SIZE, N_FEATURES).astype(np.float32) * 10
            yield [data]

    converter.representative_dataset = representative_data_gen
    tflite_model = converter.convert()

    with open(output_path, "wb") as f:
        f.write(tflite_model)
    print(f"TFLite quantized: {output_path} ({len(tflite_model)} bytes)")


def train():
    print("Loading data...")
    data_path = os.path.join(DATA_DIR, "synthetic_migraine_data.csv")
    df = pd.read_csv(data_path)

    print("Creating sequences (6h windows)...")
    X, y = create_sequences(df)

    # Class distribution
    unique, counts = np.unique(y, return_counts=True)
    for u, c in zip(unique, counts):
        print(f"  Class {u}: {c} ({c/len(y):.1%})")

    # Normalize
    mean = X.mean(axis=(0, 1))
    std = X.std(axis=(0, 1))
    std[std == 0] = 1
    X = (X - mean) / std

    np.save(os.path.join(MODEL_DIR, "prodrome_norm_mean.npy"), mean)
    np.save(os.path.join(MODEL_DIR, "prodrome_norm_std.npy"), std)

    # Split
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    # Compute class weights for imbalanced data
    class_weights = {i: len(y_train) / (N_CLASSES * np.sum(y_train == i)) for i in range(N_CLASSES)}
    print(f"Class weights: {class_weights}")

    # Build + train
    model = build_model()
    model.summary()

    callbacks = [
        tf.keras.callbacks.EarlyStopping(patience=5, restore_best_weights=True, monitor="val_accuracy"),
        tf.keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=3),
    ]

    print("Training 1D-CNN...")
    model.fit(X_train, y_train, validation_split=0.2, epochs=20, batch_size=128,
              class_weight=class_weights, callbacks=callbacks, verbose=1)

    # Evaluate
    print("\nEvaluation:")
    test_loss, test_acc = model.evaluate(X_test, y_test, verbose=0)
    print(f"Test accuracy: {test_acc:.4f}")

    y_pred = model.predict(X_test).argmax(axis=1)
    print(classification_report(y_test, y_pred,
                                target_names=["normal", "prodrome", "migraine"]))
    print("Confusion matrix:")
    print(confusion_matrix(y_test, y_pred))

    # Save
    model.save(os.path.join(MODEL_DIR, "prodrome_detector.h5"))
    export_tflite_quantized(model, os.path.join(MODEL_DIR, "prodrome_detector.tflite"))

    print("Done!")


if __name__ == "__main__":
    train()
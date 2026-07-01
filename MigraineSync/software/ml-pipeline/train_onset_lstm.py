"""
MigraineSync — Migraine Onset Predictor (LSTM)
===============================================
Trains a 2-layer LSTM (128 hidden units) to predict migraine onset
within the next 48 hours from a 48-hour feature window.

Input:  48h × 6 features (HRV, pressure_delta, hydration, light, skin_temp, activity)
Output: P(onset in next 48h) — binary classification

Model: LSTM(128) → LSTM(128) → Dense(64) → Dense(1, sigmoid)
Export: ONNX + TFLite (quantized for edge)

License: MIT
"""

import os
import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow.keras import layers, models
from sklearn.model_selection import train_test_split
from sklearn.metrics import roc_auc_score, precision_recall_curve, classification_report

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")
MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "models")
os.makedirs(MODEL_DIR, exist_ok=True)

# ── Hyperparameters ──────────────────────────────────────
WINDOW_HOURS = 48
SAMPLES_PER_HOUR = 12  # 5-min intervals
WINDOW_SIZE = WINDOW_HOURS * SAMPLES_PER_HOUR  # 576 samples
FORECAST_HOURS = 48
FEATURES = ["hrv_rmssd", "pressure_delta_3h", "hydration_ml",
            "light_lux", "skin_temp_c", "activity"]
N_FEATURES = len(FEATURES)


def create_sequences(df: pd.DataFrame, window_size: int, forecast_samples: int):
    """Create sliding-window sequences for LSTM training.

    X: (window_size, N_FEATURES) — 48h of features
    y: binary — did migraine occur within next 48h?
    """
    X, y = [], []
    patients = df["patient_id"].unique()

    for pid in patients:
        patient_df = df[df["patient_id"] == pid].reset_index(drop=True)
        data = patient_df[FEATURES].values
        labels = patient_df["migraine_onset"].values
        n = len(data)

        for i in range(window_size, n - forecast_samples):
            X.append(data[i - window_size:i])
            # Label: 1 if migraine occurs in next 48h
            future_window = labels[i:i + forecast_samples]
            y.append(1 if np.any(future_window) else 0)

    return np.array(X), np.array(y)


def normalize_features(X_train, X_val, X_test):
    """Z-score normalize features."""
    mean = X_train.mean(axis=(0, 1))
    std = X_train.std(axis=(0, 1))
    std[std == 0] = 1  # avoid division by zero

    X_train_norm = (X_train - mean) / std
    X_val_norm = (X_val - mean) / std
    X_test_norm = (X_test - mean) / std

    return X_train_norm, X_val_norm, X_test_norm, mean, std


def build_model(window_size: int, n_features: int) -> tf.keras.Model:
    """Build 2-layer LSTM model."""
    model = models.Sequential([
        layers.Input(shape=(window_size, n_features)),
        layers.LSTM(128, return_sequences=True),
        layers.Dropout(0.3),
        layers.LSTM(128, return_sequences=False),
        layers.Dropout(0.3),
        layers.Dense(64, activation="relu"),
        layers.Dense(1, activation="sigmoid"),
    ])
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss="binary_crossentropy",
        metrics=["accuracy", tf.keras.metrics.AUC(name="auc")],
    )
    return model


def export_tflite_quantized(model, output_path):
    """Export quantized TFLite model for ESP32-S3 edge inference."""
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_types = [tf.int8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    # Representative dataset for quantization calibration
    def representative_data_gen():
        for _ in range(100):
            data = np.random.randn(1, WINDOW_SIZE, N_FEATURES).astype(np.float32)
            yield [data]

    converter.representative_dataset = representative_data_gen
    tflite_model = converter.convert()

    with open(output_path, "wb") as f:
        f.write(tflite_model)
    print(f"TFLite quantized model saved: {output_path} ({len(tflite_model)} bytes)")


def export_onnx(model, output_path):
    """Export ONNX model for cloud inference."""
    try:
        import tf2onnx
        import onnxruntime as ort

        onnx_model, _ = tf2onnx.convert.from_keras(model)
        with open(output_path, "wb") as f:
            f.write(onnx_model.SerializeToString())
        print(f"ONNX model saved: {output_path}")
    except ImportError:
        print("tf2onnx not installed, skipping ONNX export")


def train():
    """Full training pipeline."""
    print("Loading synthetic data...")
    data_path = os.path.join(DATA_DIR, "synthetic_migraine_data.csv")
    df = pd.read_csv(data_path)

    print(f"Creating sequences (window={WINDOW_HOURS}h, forecast={FORECAST_HOURS}h)...")
    X, y = create_sequences(df, WINDOW_SIZE, FORECAST_HOURS)

    print(f"Dataset: {len(X)} sequences, positive class: {y.mean():.1%}")

    # Split
    X_train, X_temp, y_train, y_temp = train_test_split(X, y, test_size=0.3, random_state=42)
    X_val, X_test, y_val, y_test = train_test_split(X_temp, y_temp, test_size=0.5, random_state=42)

    # Normalize
    X_train, X_val, X_test, mean, std = normalize_features(X_train, X_val, X_test)

    # Save normalization params
    np.save(os.path.join(MODEL_DIR, "onset_norm_mean.npy"), mean)
    np.save(os.path.join(MODEL_DIR, "onset_norm_std.npy"), std)

    # Build model
    model = build_model(WINDOW_SIZE, N_FEATURES)
    model.summary()

    # Callbacks
    callbacks = [
        tf.keras.callbacks.EarlyStopping(patience=5, restore_best_weights=True, monitor="val_auc", mode="max"),
        tf.keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=3, monitor="val_loss"),
    ]

    # Train
    print("Training LSTM...")
    history = model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=20,
        batch_size=64,
        callbacks=callbacks,
        verbose=1,
    )

    # Evaluate
    print("\nEvaluation:")
    test_loss, test_acc, test_auc = model.evaluate(X_test, y_test, verbose=0)
    print(f"Test accuracy: {test_acc:.4f}, AUC: {test_auc:.4f}")

    y_pred = model.predict(X_test).ravel()
    print(f"ROC AUC: {roc_auc_score(y_test, y_pred):.4f}")

    # Optimal threshold
    precision, recall, thresholds = precision_recall_curve(y_test, y_pred)
    f1 = 2 * precision * recall / (precision + recall + 1e-9)
    best_idx = np.argmax(f1)
    best_threshold = thresholds[best_idx] if best_idx < len(thresholds) else 0.5
    print(f"Best F1 threshold: {best_threshold:.4f} (F1={f1[best_idx]:.4f})")

    y_pred_binary = (y_pred > best_threshold).astype(int)
    print(classification_report(y_test, y_pred_binary, target_names=["no migraine", "migraine"]))

    # Save models
    model_path = os.path.join(MODEL_DIR, "onset_predictor.h5")
    model.save(model_path)
    print(f"Keras model saved: {model_path}")

    export_tflite_quantized(model, os.path.join(MODEL_DIR, "onset_predictor.tflite"))
    export_onnx(model, os.path.join(MODEL_DIR, "onset_predictor.onnx"))

    # Save threshold
    with open(os.path.join(MODEL_DIR, "onset_threshold.txt"), "w") as f:
        f.write(str(best_threshold))

    print("Done!")


if __name__ == "__main__":
    train()
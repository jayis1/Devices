"""
train_apnea.py — Train apnea/snoring event detector for SleepSync

Input:  BCG signal windows (30s) + snoring intensity from sleep strip
Output: Apnea risk score (0-1), event type classification (normal/snoring/apnea/hypopnea)

Model: 1D-CNN + LSTM, INT8 quantized, ~60KB
"""

import argparse
import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report

# ---- Constants ----
WINDOW_SECONDS = 30
BCG_SAMPLE_RATE = 200  # Hz
WINDOW_SAMPLES = WINDOW_SECONDS * BCG_SAMPLE_RATE  # 6000 samples
NUM_CLASSES = 4  # 0=normal, 1=snoring, 2=apnea, 3=hypopnea


def build_model():
    """1D-CNN + LSTM apnea event detector"""
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(WINDOW_SAMPLES, 1)),
        # Feature extraction
        tf.keras.layers.Conv1D(64, kernel_size=7, activation='relu', padding='same'),
        tf.keras.layers.BatchNormalization(),
        tf.keras.layers.MaxPooling1D(pool_size=4),
        tf.keras.layers.Conv1D(32, kernel_size=5, activation='relu', padding='same'),
        tf.keras.layers.BatchNormalization(),
        tf.keras.layers.MaxPooling1D(pool_size=4),
        # Temporal modeling
        tf.keras.layers.LSTM(64, return_sequences=False),
        tf.keras.layers.Dense(32, activation='relu'),
        tf.keras.layers.Dropout(0.3),
        tf.keras.layers.Dense(NUM_CLASSES, activation='softmax'),
    ])
    model.compile(
        optimizer='adam',
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )
    return model


def generate_synthetic_bcg(n_samples=10000):
    """Generate synthetic BCG signals with labeled events"""
    np.random.seed(42)
    X = np.zeros((n_samples, WINDOW_SAMPLES, 1))
    y = np.zeros(n_samples, dtype=int)

    for i in range(n_samples):
        # Base cardiac signal (sinusoidal at heart rate frequency)
        t = np.linspace(0, WINDOW_SECONDS, WINDOW_SAMPLES)
        hr_freq = np.random.uniform(0.8, 1.5)  # 48-90 BPM in Hz
        cardiac = np.sin(2 * np.pi * hr_freq * t) * 0.5

        # Respiration modulation (0.1-0.5 Hz)
        rr_freq = np.random.uniform(0.15, 0.4)
        resp_mod = 0.1 * np.sin(2 * np.pi * rr_freq * t)

        # Add noise
        noise = np.random.normal(0, 0.05, WINDOW_SAMPLES)

        signal = cardiac + resp_mod + noise

        # Class assignment
        event_type = np.random.choice([0, 0, 0, 1, 1, 2, 3], p=[0.4, 0.15, 0.15, 0.15, 0.05, 0.05, 0.05])

        if event_type == 1:  # Snoring
            # Add high-frequency oscillation
            snore_start = np.random.randint(0, WINDOW_SAMPLES // 2)
            snore_end = min(snore_start + np.random.randint(200, 2000), WINDOW_SAMPLES)
            snore_freq = np.random.uniform(40, 150)  # Hz
            signal[snore_start:snore_end] += 0.3 * np.sin(
                2 * np.pi * snore_freq * t[snore_start:snore_end])

        elif event_type == 2:  # Apnea (central)
            # Breathing pauses > 10s
            pause_start = np.random.randint(200, WINDOW_SAMPLES - 4000)
            pause_len = np.random.randint(2000, 4000)  # 10-20s at 200Hz
            signal[pause_start:pause_start + pause_len] *= 0.2  # nearly flat

        elif event_type == 3:  # Hypopnea
            # Reduced breathing amplitude
            hypo_start = np.random.randint(200, WINDOW_SAMPLES - 2000)
            hypo_len = np.random.randint(2000, 4000)
            signal[hypo_start:hypo_start + hypo_len] *= 0.5

        X[i, :, 0] = signal
        y[i] = event_type

    # Normalize to [-1, 1]
    X = X / np.max(np.abs(X), axis=(1, 2), keepdims=True)

    return X, y


def train(args):
    print("Generating synthetic BCG data...")
    X, y = generate_synthetic_bcg(n_samples=args.samples)
    print(f"Dataset: {X.shape[0]} samples, window={WINDOW_SAMPLES} samples @ {BCG_SAMPLE_RATE}Hz")
    print(f"Class distribution: {dict(zip(*np.unique(y, return_counts=True)))}")

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    model = build_model()
    model.summary()

    # Class weights
    class_counts = np.bincount(y_train)
    total = len(y_train)
    class_weights = {i: total / (NUM_CLASSES * c) for i, c in enumerate(class_counts)}

    model.fit(
        X_train, y_train,
        validation_split=0.2,
        epochs=args.epochs,
        batch_size=32,
        class_weight=class_weights,
        callbacks=[
            tf.keras.callbacks.EarlyStopping(patience=5, restore_best_weights=True),
        ]
    )

    y_pred = np.argmax(model.predict(X_test), axis=1)
    print("\nClassification Report:")
    print(classification_report(y_test, y_pred,
                                target_names=["Normal", "Snoring", "Apnea", "Hypopnea"]))

    # Convert to TFLite INT8
    def representative_dataset():
        for i in range(min(500, len(X_train))):
            yield [X_train[i:i+1].astype(np.float32)]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_model = converter.convert()
    with open(args.output, 'wb') as f:
        f.write(tflite_model)

    print(f"\nTFLite INT8 model saved to {args.output}")
    print(f"Model size: {len(tflite_model) / 1024:.1f} KB")


def main():
    parser = argparse.ArgumentParser(description="Train SleepSync apnea detector")
    parser.add_argument("--samples", type=int, default=10000, help="Training samples")
    parser.add_argument("--epochs", type=int, default=30, help="Training epochs")
    parser.add_argument("--output", default="apnea_detector.tflite", help="Output TFLite model")
    args = parser.parse_args()
    train(args)


if __name__ == "__main__":
    main()
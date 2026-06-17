"""
train_fire_risk.py — Train 1D-CNN+LSTM dryer lint fire risk detector

Input:  Rolling window of dryer telemetry (last 60 readings = 10 min operation)
        Features: exhaust_temp, diff_pressure, humidity, ambient_temp,
                  current, vibration_rms
Output: Fire risk score (0-1)

Model architecture:
  Conv1D(64, k=3) → Conv1D(32, k=3) → LSTM(64) → Dense(32, ReLU) → Dense(1, Sigmoid)
  ~95 KB INT8 quantized for TFLite Micro on hub/dryer node

Usage:
    python3 train_fire_risk.py --samples 20000 --epochs 80
"""

import argparse
import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split

# ---- Normalization ranges ----
FEATURE_RANGES = {
    "exhaust_temp":  (0.0, 120.0),    # °C
    "diff_pressure": (0.0, 300.0),    # Pa
    "humidity":      (0.0, 100.0),    # %
    "ambient_temp":  (0.0, 50.0),     # °C
    "current":       (0.0, 30000.0),  # mA
    "vibration":     (0.0, 1000.0),   # milli-g
}

WINDOW_SIZE = 60   # 60 readings = 10 min at 10s/report
NUM_FEATURES = 6

# Baseline normal operation values
BASELINES = {
    "exhaust_temp":  60.0,
    "diff_pressure": 45.0,
    "humidity":      40.0,
    "ambient_temp":  22.0,
    "current":       2000.0,
    "vibration":     120.0,
}


def build_model():
    """1D-CNN + LSTM fire risk detector"""
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(WINDOW_SIZE, NUM_FEATURES)),
        tf.keras.layers.Conv1D(64, kernel_size=3, activation='relu', padding='same'),
        tf.keras.layers.Conv1D(32, kernel_size=3, activation='relu', padding='same'),
        tf.keras.layers.MaxPooling1D(pool_size=2),
        tf.keras.layers.LSTM(64, return_sequences=False),
        tf.keras.layers.Dense(32, activation='relu'),
        tf.keras.layers.Dropout(0.2),
        tf.keras.layers.Dense(1, activation='sigmoid')
    ])
    model.compile(
        optimizer='adam',
        loss='binary_crossentropy',
        metrics=['accuracy', tf.keras.metrics.AUC(name='auc')]
    )
    return model


def generate_synthetic_data(n_samples=20000):
    """Generate synthetic dryer telemetry with fire risk scenarios.

    Anomaly scenarios:
      1. Lint accumulation: diff_pressure rises gradually, exhaust temp rises
      2. Exhaust restriction: sharp pressure spike + temp rise
      3. Overheating element: exhaust temp spikes without pressure change
      4. Normal operation: baseline + small noise
    """
    np.random.seed(42)

    X = np.zeros((n_samples, WINDOW_SIZE, NUM_FEATURES))
    y = np.zeros(n_samples)

    feature_names = list(FEATURE_RANGES.keys())

    for i in range(n_samples):
        labels = np.zeros(NUM_FEATURES)
        scenario = np.random.choice(
            ['normal', 'lint_slow', 'lint_fast', 'overheat', 'exhaust_block'],
            p=[0.4, 0.2, 0.15, 0.15, 0.10]
        )

        for j, name in enumerate(feature_names):
            baseline = BASELINES[name]
            lo, hi = FEATURE_RANGES[name]
            noise_std = (hi - lo) * 0.03

            readings = baseline + np.random.normal(0, noise_std, WINDOW_SIZE)

            if scenario == 'lint_slow':
                # Gradual lint accumulation over the window
                if name in ('diff_pressure', 'exhaust_temp'):
                    drift = np.linspace(0, (hi - baseline) * 0.4, WINDOW_SIZE)
                    readings += drift
                    if name == 'diff_pressure':
                        labels[j] = 0.6
                    else:
                        labels[j] = 0.5
            elif scenario == 'lint_fast':
                # Rapid lint buildup
                if name in ('diff_pressure', 'exhaust_temp'):
                    start = np.random.randint(10, 30)
                    readings[start:] += np.linspace(0, (hi - baseline) * 0.6,
                                                     WINDOW_SIZE - start)
                    labels[j] = 0.8
            elif scenario == 'overheat':
                # Heating element malfunction — temp spike
                if name == 'exhaust_temp':
                    start = np.random.randint(20, 40)
                    readings[start:] += (hi - baseline) * 0.5
                    labels[j] = 0.9
                if name == 'humidity':
                    readings -= 20  # dry air
            elif scenario == 'exhaust_block':
                # Sudden exhaust blockage
                if name == 'diff_pressure':
                    start = np.random.randint(15, 35)
                    readings[start:] = hi * 0.9
                    labels[j] = 1.0
                if name == 'exhaust_temp':
                    start = np.random.randint(15, 35)
                    readings[start:] += (hi - baseline) * 0.4
                    labels[j] = 0.7

            readings = np.clip(readings, lo, hi)
            X[i, :, j] = (readings - lo) / (hi - lo)

        # Overall fire risk = weighted combination
        if scenario == 'normal':
            y[i] = 0.0
        elif scenario == 'lint_slow':
            y[i] = 0.4 + np.random.uniform(-0.1, 0.1)
        elif scenario == 'lint_fast':
            y[i] = 0.75 + np.random.uniform(-0.1, 0.1)
        elif scenario == 'overheat':
            y[i] = 0.9 + np.random.uniform(-0.05, 0.05)
        elif scenario == 'exhaust_block':
            y[i] = 0.95 + np.random.uniform(-0.05, 0.0)
        y[i] = np.clip(y[i], 0.0, 1.0)

    return X, y


def train(args):
    print("Generating synthetic dryer telemetry data...")
    X, y = generate_synthetic_data(n_samples=args.samples)
    print(f"Dataset: {X.shape[0]} samples, window={WINDOW_SIZE}, features={NUM_FEATURES}")

    # Class distribution
    risk_bins = [0, 0.2, 0.6, 0.8, 0.95, 1.01]
    for lo, hi in zip(risk_bins[:-1], risk_bins[1:]):
        count = np.sum((y >= lo) & (y < hi))
        print(f"  Risk [{lo:.2f}, {hi:.2f}): {count} samples")

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    model = build_model()
    model.summary()

    history = model.fit(
        X_train, y_train,
        validation_split=0.2,
        epochs=args.epochs,
        batch_size=128,
        callbacks=[
            tf.keras.callbacks.EarlyStopping(patience=8, restore_best_weights=True),
            tf.keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=4)
        ]
    )

    loss, acc, auc = model.evaluate(X_test, y_test)
    print(f"\nTest loss: {loss:.4f}, accuracy: {acc:.4f}, AUC: {auc:.4f}")

    # Convert to TFLite INT8 for edge deployment (hub + dryer node)
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
    parser = argparse.ArgumentParser(description="Train WashWise fire risk detector")
    parser.add_argument("--samples", type=int, default=20000)
    parser.add_argument("--epochs", type=int, default=80)
    parser.add_argument("--output", default="fire_risk.tflite")
    args = parser.parse_args()
    train(args)


if __name__ == "__main__":
    main()
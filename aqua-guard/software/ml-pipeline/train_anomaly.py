"""
train_anomaly.py — Train 1D-CNN+LSTM water quality anomaly detector

Input:  Rolling window of 8 water parameters (last 60 readings = 1 hour)
Output: Anomaly score per parameter + overall (0-1)

Model architecture:
  Conv1D(64, k=3) → Conv1D(32, k=3) → LSTM(64) → Dense(32, ReLU) → Dense(9, Sigmoid)
  (9 outputs: 8 per-parameter scores + 1 overall)
"""

import argparse
import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import MinMaxScaler

# Normalization ranges for aquarium parameters
PARAM_RANGES = {
    "ph":           (0.0, 14.0),
    "temperature":  (-5.0, 45.0),
    "ammonia":      (0.0, 10.0),
    "nitrite":      (0.0, 10.0),
    "nitrate":      (0.0, 100.0),
    "dissolved_o2": (0.0, 20.0),
    "tds":          (0.0, 50000.0),
    "turbidity":    (0.0, 3000.0),
}

WINDOW_SIZE = 60   # 60 readings = 1 hour at 1 reading/min
NUM_PARAMS = 8
NUM_OUTPUTS = 9     # 8 per-parameter + 1 overall


def build_model():
    """1D-CNN + LSTM anomaly detector"""
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(WINDOW_SIZE, NUM_PARAMS)),
        tf.keras.layers.Conv1D(64, kernel_size=3, activation='relu', padding='same'),
        tf.keras.layers.Conv1D(32, kernel_size=3, activation='relu', padding='same'),
        tf.keras.layers.MaxPooling1D(pool_size=2),
        tf.keras.layers.LSTM(64, return_sequences=False),
        tf.keras.layers.Dense(32, activation='relu'),
        tf.keras.layers.Dense(NUM_OUTPUTS, activation='sigmoid')
    ])
    model.compile(
        optimizer='adam',
        loss='binary_crossentropy',
        metrics=['accuracy']
    )
    return model


def generate_synthetic_data(n_samples=10000):
    """Generate synthetic aquarium data with injected anomalies"""
    np.random.seed(42)

    # Normal parameters (tropical freshwater baseline)
    baselines = {
        "ph": 7.0, "temperature": 26.0, "ammonia": 0.05,
        "nitrite": 0.02, "nitrate": 10.0, "dissolved_o2": 7.5,
        "tds": 200.0, "turbidity": 5.0
    }

    X = np.zeros((n_samples, WINDOW_SIZE, NUM_PARAMS))
    y = np.zeros((n_samples, NUM_OUTPUTS))

    for i in range(n_samples):
        labels = np.zeros(NUM_OUTPUTS)

        for j, (param, (lo, hi)) in enumerate(PARAM_RANGES.items()):
            baseline = baselines[param]
            noise_std = (hi - lo) * 0.02

            # Normal readings with drift
            readings = baseline + np.random.normal(0, noise_std, WINDOW_SIZE)

            # Inject anomaly with 30% probability per parameter
            if np.random.random() < 0.3:
                anomaly_type = np.random.choice(['spike', 'drift', 'crash'])
                start = np.random.randint(10, 40)

                if anomaly_type == 'spike':
                    readings[start:start+5] += (hi - baseline) * 0.5
                    labels[j] = 0.8
                elif anomaly_type == 'drift':
                    drift = np.linspace(0, (hi - baseline) * 0.3, WINDOW_SIZE - start)
                    readings[start:] += drift
                    labels[j] = 0.6
                elif anomaly_type == 'crash':
                    readings[start:] = lo + (baseline - lo) * 0.1
                    labels[j] = 1.0

            # Clip to valid range
            readings = np.clip(readings, lo, hi)

            # Normalize
            X[i, :, j] = (readings - lo) / (hi - lo)

        # Overall anomaly = max of per-parameter
        labels[8] = np.max(labels[:8])
        y[i] = labels

    return X, y


def train(args):
    print("Generating synthetic aquarium data...")
    X, y = generate_synthetic_data(n_samples=args.samples)
    print(f"Dataset: {X.shape[0]} samples, window={WINDOW_SIZE}, params={NUM_PARAMS}")

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    model = build_model()
    model.summary()

    history = model.fit(
        X_train, y_train,
        validation_split=0.2,
        epochs=args.epochs,
        batch_size=64,
        callbacks=[
            tf.keras.callbacks.EarlyStopping(patience=5, restore_best_weights=True),
            tf.keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=3)
        ]
    )

    loss, acc = model.evaluate(X_test, y_test)
    print(f"\nTest loss: {loss:.4f}, accuracy: {acc:.4f}")

    # Convert to TFLite INT8 for hub deployment
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
    parser = argparse.ArgumentParser(description="Train Aqua Guard anomaly detector")
    parser.add_argument("--samples", type=int, default=10000, help="Training samples")
    parser.add_argument("--epochs", type=int, default=50, help="Training epochs")
    parser.add_argument("--output", default="aqua_anomaly.tflite", help="Output TFLite model")
    args = parser.parse_args()
    train(args)


if __name__ == "__main__":
    main()
"""
train_sleep_staging.py — Train 1D-CNN+BiLSTM+Attention sleep stager for SleepSync

Input:  Rolling window of 60 sleep data packets (5 min at 5s intervals)
        11 features per timestep:
        - heart_rate (BPM×10), hrv, resp_rate (×10), rrv,
        - movement, snoring, sleep_stage, stage_conf, battery_pct
        - (plus 2 derived: hr_rr_ratio, movement_ma)

Output: Sleep stage classification (4 classes: wake, light, deep, REM)
        + stage confidence (0-1)

Model architecture:
  Conv1D(64, k=5) → Conv1D(32, k=3) → BiLSTM(64) → Attention(64) → Dense(32) → Dense(4, Softmax)
  Quantized to INT8 for ESP32-S3 TFLite Micro deployment
"""

import argparse
import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix

# ---- Constants ----
WINDOW_SIZE = 60    # 60 samples × 5s = 5 min window
NUM_FEATURES = 11   # Features per timestep
NUM_CLASSES = 4     # wake=0, light=1, deep=2, REM=3

# Normalization ranges
FEAT_RANGES = {
    "heart_rate":  (300, 1200),   # 30-120 BPM ×10
    "hrv":        (0, 255),
    "resp_rate":  (60, 300),      # 6-30 breaths/min ×10
    "rrv":        (0, 255),
    "movement":   (0, 255),
    "snoring":    (0, 255),
    "sleep_stage": (0, 3),
    "stage_conf": (0, 255),
    "battery_pct": (0, 100),
    "hr_rr_ratio": (0.2, 0.8),   # derived: HR/RR ratio (normalized)
    "movement_ma": (0, 255),     # derived: 5-sample moving average
}

# Typical sleep stage distributions (healthy adult)
STAGE_DURATIONS = {
    0: 0.05,  # wake: ~5% of total sleep time
    1: 0.50,  # light: ~50%
    2: 0.20,  # deep: ~20%
    3: 0.25,  # REM: ~25%
}


def build_model():
    """1D-CNN + BiLSTM + Attention sleep stager"""
    inputs = tf.keras.layers.Input(shape=(WINDOW_SIZE, NUM_FEATURES))

    # Convolutional feature extraction
    x = tf.keras.layers.Conv1D(64, kernel_size=5, activation='relu', padding='same')(inputs)
    x = tf.keras.layers.BatchNormalization()(x)
    x = tf.keras.layers.Conv1D(32, kernel_size=3, activation='relu', padding='same')(x)
    x = tf.keras.layers.BatchNormalization()(x)
    x = tf.keras.layers.MaxPooling1D(pool_size=2)(x)

    # Bidirectional LSTM for temporal context
    x = tf.keras.layers.Bidirectional(
        tf.keras.layers.LSTM(64, return_sequences=True)
    )(x)

    # Self-attention mechanism
    attention = tf.keras.layers.Dense(1, activation='tanh')(x)
    attention_weights = tf.keras.layers.Softmax(axis=1)(attention)
    x = tf.keras.layers.Multiply()([x, attention_weights])
    x = tf.keras.layers.Lambda(lambda t: tf.reduce_mean(t, axis=1))(x)

    # Classification head
    x = tf.keras.layers.Dense(64, activation='relu')(x)
    x = tf.keras.layers.Dropout(0.3)(x)
    x = tf.keras.layers.Dense(32, activation='relu')(x)
    outputs = tf.keras.layers.Dense(NUM_CLASSES, activation='softmax')(x)

    model = tf.keras.Model(inputs=inputs, outputs=outputs)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )
    return model


def generate_synthetic_sleep_data(n_nights=100, samples_per_night=720):
    """
    Generate realistic synthetic sleep data for training.

    A typical night:
    - 8 hours = 480 min = 5760 five-second intervals
    - But we chunk into 720 windows of 60 samples each (with stride 8)
    - Sleep progresses through cycles: wake → light → deep → light → REM → ...
    """
    np.random.seed(42)

    total_samples = n_nights * samples_per_night
    X = np.zeros((total_samples, WINDOW_SIZE, NUM_FEATURES))
    y = np.zeros(total_samples, dtype=int)

    sample_idx = 0

    for night in range(n_nights):
        # Generate a night's worth of sleep stages (5760 intervals of 5s)
        stages = generate_night_stages(night)

        # Generate corresponding physiological signals
        for i in range(0, len(stages) - WINDOW_SIZE, 8):  # stride 8
            window = stages[i:i + WINDOW_SIZE]
            features = generate_window_features(window)
            X[sample_idx] = features
            # Label = mode of window (most common stage)
            y[sample_idx] = np.bincount(window).argmax()
            sample_idx += 1

            if sample_idx >= total_samples:
                break
        if sample_idx >= total_samples:
            break

    return X[:sample_idx], y[:sample_idx]


def generate_night_stages(seed=0):
    """Generate a realistic night of sleep stages (8 hours, 5s intervals)"""
    np.random.seed(seed)
    n_intervals = 5760  # 8 hours × 60min × 12 (5s intervals per min)
    stages = np.zeros(n_intervals, dtype=int)

    # Sleep cycle pattern: ~90 min cycles
    # Each cycle: light → deep → light → REM
    # Deep sleep concentrated in first half of night
    # REM sleep concentrated in second half

    cycle_length = 90 * 12  # 90 min in 5s intervals
    pos = 60  # Start after 5 min of wake

    # Initial wake period (falling asleep)
    stages[0:pos] = 0  # awake
    latency = np.random.randint(48, 240)  # 4-20 min to fall asleep
    stages[pos:pos + latency] = 1  # light sleep onset
    pos += latency

    cycle = 0
    while pos < n_intervals - 120:
        # Light sleep at start of cycle
        light_duration = np.random.randint(60, 180)  # 5-15 min
        end = min(pos + light_duration, n_intervals)
        stages[pos:end] = 1
        pos = end

        # Deep sleep (more in early cycles)
        if cycle < 3:  # First 3 cycles have more deep sleep
            deep_duration = np.random.randint(120, 480)  # 10-40 min
        else:
            deep_duration = np.random.randint(0, 120)  # 0-10 min
        if deep_duration > 0:
            end = min(pos + deep_duration, n_intervals)
            stages[pos:end] = 2
            pos = end

        # Brief arousal / light sleep
        light_duration = np.random.randint(30, 120)  # 2.5-10 min
        end = min(pos + light_duration, n_intervals)
        stages[pos:end] = 1
        pos = end

        # REM sleep (more in later cycles)
        if cycle >= 1:  # REM starts after first cycle
            if cycle >= 3:
                rem_duration = np.random.randint(120, 360)  # 10-30 min
            else:
                rem_duration = np.random.randint(60, 180)  # 5-15 min
            end = min(pos + rem_duration, n_intervals)
            stages[pos:end] = 3
            pos = end

        # Possible brief wake between cycles
        if np.random.random() < 0.3:
            wake_duration = np.random.randint(12, 60)  # 1-5 min
            end = min(pos + wake_duration, n_intervals)
            stages[pos:end] = 0
            pos = end

        cycle += 1

    # Final wake period
    stages[pos:] = 0

    return stages


def generate_window_features(window_stages):
    """Generate realistic physiological features for a window of sleep stages"""
    features = np.zeros((WINDOW_SIZE, NUM_FEATURES))
    n = len(window_stages)

    # Base physiological parameters per stage
    stage_params = {
        0: {"hr": 650, "hrv": 30, "rr": 180, "rrv": 20, "move": 80, "snore": 5},
        1: {"hr": 600, "hrv": 45, "rr": 160, "rrv": 25, "move": 15, "snore": 20},
        2: {"hr": 530, "hrv": 70, "rr": 130, "rrv": 15, "move": 3,  "snore": 30},
        3: {"hr": 620, "hrv": 90, "rr": 150, "rrv": 35, "move": 8,  "snore": 40},
    }

    for i in range(n):
        stage = window_stages[i]
        p = stage_params[stage]

        # Add realistic noise and variability
        hr_noise = np.random.normal(0, 20)
        hrv_noise = np.random.normal(0, 8)
        rr_noise = np.random.normal(0, 10)
        move_noise = np.random.exponential(5) if stage < 2 else 0

        # Snoring: intermittent, mostly during light/deep sleep
        snore_val = 0
        if stage in (1, 2) and np.random.random() < 0.3:
            snore_val = p["snore"] + np.random.normal(0, 30)

        features[i, 0] = np.clip(p["hr"] + hr_noise, 300, 1200)
        features[i, 1] = np.clip(p["hrv"] + hrv_noise, 0, 255)
        features[i, 2] = np.clip(p["rr"] + rr_noise, 60, 300)
        features[i, 3] = np.clip(p["rrv"] + np.random.normal(0, 5), 0, 255)
        features[i, 4] = np.clip(p["move"] + move_noise, 0, 255)
        features[i, 5] = np.clip(abs(snore_val), 0, 255)
        features[i, 6] = stage
        features[i, 7] = np.clip(200 + np.random.normal(0, 30), 0, 255)
        features[i, 8] = np.clip(90 + np.random.normal(0, 5), 0, 100)
        features[i, 9] = features[i, 0] / max(features[i, 2], 1) * 0.1  # HR/RR ratio
        features[i, 10] = np.clip(np.mean(features[max(0, i-4):i+1, 4]), 0, 255)

    # Normalize features to [0, 1]
    for j, (feat, (lo, hi)) in enumerate(FEAT_RANGES.items()):
        features[:, j] = np.clip((features[:, j] - lo) / (hi - lo), 0, 1)

    return features


def train(args):
    print("Generating synthetic sleep data...")
    X, y = generate_synthetic_sleep_data(n_nights=args.nights, samples_per_night=500)
    print(f"Dataset: {X.shape[0]} samples, window={WINDOW_SIZE}, features={NUM_FEATURES}")
    print(f"Class distribution: {dict(zip(*np.unique(y, return_counts=True)))}")

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2,
                                                          random_state=42, stratify=y)

    model = build_model()
    model.summary()

    # Class weights to handle imbalance
    class_counts = np.bincount(y_train)
    total = len(y_train)
    class_weights = {i: total / (NUM_CLASSES * count) for i, count in enumerate(class_counts)}

    history = model.fit(
        X_train, y_train,
        validation_split=0.2,
        epochs=args.epochs,
        batch_size=64,
        class_weight=class_weights,
        callbacks=[
            tf.keras.callbacks.EarlyStopping(patience=8, restore_best_weights=True),
            tf.keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=4),
        ]
    )

    # Evaluate
    y_pred = model.predict(X_test)
    y_pred_classes = np.argmax(y_pred, axis=1)
    print("\nClassification Report:")
    print(classification_report(y_test, y_pred_classes,
                                target_names=["Wake", "Light", "Deep", "REM"]))
    print("\nConfusion Matrix:")
    print(confusion_matrix(y_test, y_pred_classes))

    # Convert to TFLite INT8 for ESP32-S3 deployment
    def representative_dataset():
        for i in range(min(1000, len(X_train))):
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
    parser = argparse.ArgumentParser(description="Train SleepSync sleep staging model")
    parser.add_argument("--nights", type=int, default=200, help="Synthetic nights to generate")
    parser.add_argument("--epochs", type=int, default=80, help="Training epochs")
    parser.add_argument("--output", default="sleep_staging.tflite", help="Output TFLite model")
    args = parser.parse_args()
    train(args)


if __name__ == "__main__":
    main()
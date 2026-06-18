"""
train_pirate_behavior.py — Train porch pirate behavior detection (temporal model)

This is the signature ML model of PorchGuard. It runs on the HUB (RP2040,
TFLite Micro, ~110 KB) and classifies the temporal behavior pattern of a
porch theft:

    loiter → approach porch → grab parcel → flee with object

Input: 30s rolling window of presence/parcel/person events, sampled at 2Hz
       (60 timesteps). Features per step:
         - mmWave presence distance (cm, -1=none)
         - parcel-present flag (0/1)
         - parcel class (0-4)
         - person-present flag (0/1)
         - person identity (0-4: none/resident/courier/unknown/loitering)
         - motion energy (0-255)

Output: pirate risk score (0.0-1.0)
Triggers:
    >0.6  = warning  (chime + app "Possible theft")
    >0.8  = critical (siren + clip + cloud)
    >0.95 = emergency (siren + 911-style alert)

Architecture: 1D-CNN + LSTM hybrid, INT8 quantized.

Usage:
    python3 train_pirate_behavior.py --epochs 100
    python3 train_pirate_behavior.py --epochs 100 --samples 20000
"""

import argparse
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, Model
from sklearn.model_selection import train_test_split

SEQ_LEN = 60      # 30s at 2Hz
N_FEATURES = 6    # mmwave_dist, parcel_present, parcel_class,
                  # person_present, person_id, motion_energy


def build_pirate_model(seq_len=SEQ_LEN, n_features=N_FEATURES):
    """1D-CNN + LSTM hybrid for temporal pirate behavior detection."""
    inputs = layers.Input(shape=(seq_len, n_features))
    # 1D conv to extract local motion patterns (grab, flee bursts)
    x = layers.Conv1D(32, 5, activation='relu', padding='same')(inputs)
    x = layers.MaxPooling1D(2)(x)  # 60 → 30
    x = layers.Conv1D(64, 3, activation='relu', padding='same')(x)
    x = layers.MaxPooling1D(2)(x)  # 30 → 15
    # LSTM to capture the sequential loiter→approach→grab→flee pattern
    x = layers.LSTM(64, return_sequences=False)(x)
    x = layers.Dropout(0.4)(x)
    x = layers.Dense(32, activation='relu')(x)
    x = layers.Dropout(0.3)(x)
    # Single risk score 0-1
    risk = layers.Dense(1, activation='sigmoid', name='risk')(x)
    model = Model(inputs, risk)
    model.compile(optimizer='adam', loss='binary_crossentropy',
                  metrics=['accuracy'])
    return model


def generate_pirate_sequences(n_samples=20000, seq_len=SEQ_LEN):
    """Synthesize temporal sequences of porch presence events.

    Three behavior classes:
      0. NORMAL (resident/courier visits, no theft)
      1. PIRATE (the classic loiter→approach→grab→flee pattern)
      2. AMBIGUOUS (looks suspicious but ends benign — e.g., neighbor
         picks up a parcel for you, or a person lingers but leaves empty)

    Returns: X (n, 60, 6), y (n,) binary (1 if pirate completes the grab)
    """
    np.random.seed(99)
    X = np.zeros((n_samples, seq_len, N_FEATURES), dtype=np.float32)
    y = np.zeros(n_samples, dtype=int)

    for i in range(n_samples):
        behavior = np.random.choice([0, 1, 2], p=[0.55, 0.25, 0.20])

        if behavior == 0:
            # NORMAL: resident arrives, maybe a parcel was already there,
            # they collect it and leave. Or courier drops a parcel and leaves.
            seq = _gen_normal(seq_len)
            y[i] = 0
        elif behavior == 1:
            # PIRATE: loiter → approach → parcel disappears → person leaves quickly
            seq = _gen_pirate(seq_len)
            y[i] = 1
        else:
            # AMBIGUOUS: loiter but no parcel grabbed (label 0)
            seq = _gen_ambiguous(seq_len)
            y[i] = 0
        X[i] = seq

    return X, y


def _step(dist, parcel_present, parcel_class, person_present, person_id, motion):
    """Build one feature vector."""
    return np.array([dist, parcel_present, parcel_class,
                     person_present, person_id, motion], dtype=np.float32)


def _gen_normal(seq_len):
    """Normal visit: resident or courier. Parcel may appear (delivery) or
    disappear (resident collects), but person is identified."""
    seq = np.zeros((seq_len, N_FEATURES), dtype=np.float32)
    # Phase 1: empty porch
    t = 0
    empty_len = np.random.randint(10, 30)
    for _ in range(empty_len):
        seq[t] = _step(-1, 0, 0, 0, 0, 0)
        t += 1
    # Phase 2: person arrives (resident or courier)
    person_id = np.random.choice([1, 2])  # resident or courier
    visit_len = np.random.randint(15, 30)
    # Maybe a parcel appears (courier delivery) or disappears (resident)
    parcel_appears = person_id == 2 and np.random.random() < 0.7
    parcel_disappears = person_id == 1 and np.random.random() < 0.4
    parcel_class = np.random.choice([1, 2, 3]) if parcel_appears else 0
    for _ in range(visit_len):
        if t >= seq_len:
            break
        dist = np.random.randint(40, 120)
        parcel_present = 1 if (parcel_appears and t > empty_len + 3) else (
            0 if (parcel_disappears and t > empty_len + 8) else 0)
        seq[t] = _step(dist, parcel_present, parcel_class, 1, person_id,
                       np.random.randint(20, 80))
        t += 1
    # Phase 3: person leaves
    while t < seq_len:
        seq[t] = _step(-1, 1 if parcel_appears else 0,
                       parcel_class if parcel_appears else 0, 0, 0, 0)
        t += 1
    return seq


def _gen_pirate(seq_len):
    """Pirate: loiter (no parcel interaction) → approach → grab → flee."""
    seq = np.zeros((seq_len, N_FEATURES), dtype=np.float32)
    # Parcel is present from the start (left by earlier courier)
    parcel_class = np.random.choice([1, 2, 3])
    # Phase 1: empty porch with parcel (0-10 steps)
    t = 0
    idle_len = np.random.randint(0, 10)
    for _ in range(idle_len):
        seq[t] = _step(-1, 1, parcel_class, 0, 0, 0)
        t += 1
    # Phase 2: loiter — unknown person appears at distance, lingers
    loiter_len = np.random.randint(15, 30)
    for _ in range(loiter_len):
        if t >= seq_len:
            break
        dist = np.random.randint(150, 250)  # far, watching
        seq[t] = _step(dist, 1, parcel_class, 1, 3, np.random.randint(10, 40))
        t += 1
    # Phase 3: approach — distance drops fast
    approach_len = np.random.randint(5, 12)
    for j in range(approach_len):
        if t >= seq_len:
            break
        dist = int(250 - (250 / approach_len) * j)  # closing in
        seq[t] = _step(dist, 1, parcel_class, 1, 4, np.random.randint(40, 100))
        t += 1
    # Phase 4: grab — parcel disappears, motion spikes
    if t < seq_len:
        seq[t] = _step(20, 0, 0, 1, 4, np.random.randint(180, 255))  # grab burst
        t += 1
    # Phase 5: flee — distance increases fast, no parcel
    while t < seq_len:
        dist = np.random.randint(150, 300)
        seq[t] = _step(dist, 0, 0, 1, 3, np.random.randint(80, 150))
        t += 1
    return seq


def _gen_ambiguous(seq_len):
    """Ambiguous: loiter but person leaves empty-handed (no parcel grabbed)."""
    seq = np.zeros((seq_len, N_FEATURES), dtype=np.float32)
    parcel_class = np.random.choice([1, 2, 3])
    t = 0
    idle_len = np.random.randint(0, 10)
    for _ in range(idle_len):
        seq[t] = _step(-1, 1, parcel_class, 0, 0, 0)
        t += 1
    loiter_len = np.random.randint(20, 35)
    for _ in range(loiter_len):
        if t >= seq_len:
            break
        dist = np.random.randint(150, 250)
        seq[t] = _step(dist, 1, parcel_class, 1, 3, np.random.randint(10, 40))
        t += 1
    # Approach partway then turn back (parcel still there)
    approach_len = np.random.randint(3, 8)
    for j in range(approach_len):
        if t >= seq_len:
            break
        dist = int(250 - (200 / approach_len) * j)
        seq[t] = _step(dist, 1, parcel_class, 1, 3, np.random.randint(30, 80))
        t += 1
    # Leave (parcel still present — person gave up / wrong house)
    while t < seq_len:
        dist = np.random.randint(200, 300)
        seq[t] = _step(dist, 1, parcel_class, 0, 0, 0)
        t += 1
    return seq


def normalize_features(X):
    """Normalize features for INT8 quantization.
    mmwave_dist: -1..300 → divide by 300
    parcel_present, person_present: 0/1 (keep)
    parcel_class: 0..4 → /4
    person_id: 0..4 → /4
    motion_energy: 0..255 → /255
    """
    Xn = X.copy()
    Xn[..., 0] = (Xn[..., 0] + 1) / 301.0
    Xn[..., 2] = Xn[..., 2] / 4.0
    Xn[..., 4] = Xn[..., 4] / 4.0
    Xn[..., 5] = Xn[..., 5] / 255.0
    return Xn


def train_model(model, X_train, y_train, X_test, y_test, epochs):
    print("\n--- Training Pirate Behavior Model ---")
    # Class weight to handle imbalance (pirates are the minority)
    classes = np.unique(y_train)
    weights = {c: (len(y_train) / (len(classes) * np.sum(y_train == c)))
               for c in classes}
    print(f"Class weights: {weights}")
    history = model.fit(
        X_train, y_train,
        validation_split=0.2,
        epochs=epochs,
        batch_size=64,
        class_weight=weights,
        callbacks=[
            tf.keras.callbacks.EarlyStopping(patience=12, restore_best_weights=True),
            tf.keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=5),
        ]
    )
    loss, acc = model.evaluate(X_test, y_test, verbose=0)
    print(f"\nPirate Behavior — Test loss: {loss:.4f}, accuracy: {acc:.4f}")

    # Per-class metrics
    y_pred = model.predict(X_test, verbose=0).flatten()
    y_pred_bin = (y_pred > 0.5).astype(int)
    tp = np.sum((y_pred_bin == 1) & (y_test == 1))
    fp = np.sum((y_pred_bin == 1) & (y_test == 0))
    fn = np.sum((y_pred_bin == 0) & (y_test == 1))
    print(f"  TP={tp} FP={fp} FN={fn}")
    if tp + fp > 0:
        print(f"  Precision: {tp/(tp+fp):.3f}")
    if tp + fn > 0:
        print(f"  Recall:    {tp/(tp+fn):.3f}")
    return model


def quantize_tflite(model, X_train, output_path):
    """Convert to TFLite INT8 for RP2040 (TFLite Micro) deployment."""
    def representative_data():
        for i in range(min(300, len(X_train))):
            yield [X_train[i:i+1].astype(np.float32)]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_data
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    tflite = converter.convert()
    with open(output_path, 'wb') as f:
        f.write(tflite)
    print(f"TFLite INT8 saved: {output_path} ({len(tflite)/1024:.1f} KB)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--samples", type=int, default=20000)
    parser.add_argument("--epochs", type=int, default=100)
    args = parser.parse_args()

    print("Generating pirate behavior sequences...")
    X, y = generate_pirate_sequences(args.samples)
    X = normalize_features(X)
    X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.2, random_state=42)

    model = build_pirate_model()
    model = train_model(model, X_tr, y_tr, X_te, y_te, args.epochs)
    quantize_tflite(model, X_tr, "pirate_behavior.tflite")


if __name__ == "__main__":
    main()
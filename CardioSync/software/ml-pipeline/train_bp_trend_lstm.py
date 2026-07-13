"""
train_bp_trend_lstm.py — Train BP trend prediction LSTM

Model: LSTM (2 layers, 64 units)
Input: 30-day BP history (systolic, diastolic, MAP, HR, time-of-day)
Output: BP trend + hypertension stage classification

Dataset: MIMIC-III BP records (50,000+ patient-days)

License: MIT
"""
import os
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models, optimizers
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# ── Configuration ─────────────────────────────────────────────
SEQ_LENGTH = 60       # 30 days × 2 readings/day
FEATURES = 5          # systolic, diastolic, map, hr, time_of_day
NUM_CLASSES = 7       # WHO/ISH categories
BATCH_SIZE = 32
EPOCHS = 40
LEARNING_RATE = 0.0005
MODEL_PATH = "models/bp_trend_lstm.h5"
TFLITE_PATH = "models/bp_trend_lstm.tflite"

CATEGORIES = ['Optimal', 'Normal', 'High Normal', 'Hypertension S1',
              'Hypertension S2', 'Hypertension S3', 'Isolated Systolic']

# ── Model Architecture ──────────────────────────────────────
def build_model():
    """
    LSTM for BP trend prediction:
      LSTM(64, return_sequences=True) → LSTM(64) → Dense(32) → Dense(7, softmax)
    """
    model = models.Sequential([
        layers.Input(shape=(SEQ_LENGTH, FEATURES)),
        layers.LSTM(64, return_sequences=True),
        layers.Dropout(0.3),
        layers.LSTM(64),
        layers.Dropout(0.3),
        layers.Dense(32, activation='relu'),
        layers.Dense(NUM_CLASSES, activation='softmax')
    ])

    model.compile(
        optimizer=optimizers.Adam(learning_rate=LEARNING_RATE),
        loss='categorical_crossentropy',
        metrics=['accuracy']
    )
    return model

# ── Synthetic Data Generation (for demo; use MIMIC-III in production) ──
def generate_synthetic_data(n_patients=5000):
    """Generate synthetic 30-day BP sequences with realistic patterns."""
    X = []
    y = []

    for _ in range(n_patients):
        # Random baseline BP
        baseline_sys = np.random.normal(125, 20)
        baseline_dia = np.random.normal(80, 12)

        # Random trend (stable, increasing, decreasing)
        trend = np.random.choice([-0.5, 0, 0.5, 1.0, -1.0])

        # Generate 30-day sequence (2 readings/day = 60 data points)
        sequence = []
        for t in range(SEQ_LENGTH):
            day = t // 2
            time_of_day = t % 2  # 0=AM, 1=PM

            # Add circadian variation (PM slightly higher)
            circadian = 3 if time_of_day == 1 else 0

            # Add noise
            noise_sys = np.random.normal(0, 5)
            noise_dia = np.random.normal(0, 3)

            sys = baseline_sys + trend * day + circadian + noise_sys
            dia = baseline_dia + trend * day * 0.6 + circadian * 0.5 + noise_dia
            map_val = dia + (sys - dia) / 3
            hr = np.random.normal(75, 10)

            sequence.append([sys, dia, map_val, hr, time_of_day])

        sequence = np.array(sequence)

        # Classify final BP
        final_sys = sequence[-1, 0]
        final_dia = sequence[-1, 1]

        if final_sys >= 180 or final_dia >= 110:
            cls = 5  # Stage 3
        elif final_sys >= 160 or final_dia >= 100:
            cls = 4  # Stage 2
        elif final_sys >= 140 or final_dia >= 90:
            if final_dia < 90:
                cls = 6  # Isolated systolic
            else:
                cls = 3  # Stage 1
        elif final_sys >= 130 or final_dia >= 85:
            cls = 2  # High normal
        elif final_sys >= 120 or final_dia >= 80:
            cls = 1  # Normal
        else:
            cls = 0  # Optimal

        X.append(sequence)
        y.append(cls)

    return np.array(X), np.array(y)

# ── Training ─────────────────────────────────────────────────
def train():
    print("=" * 60)
    print("CardioSync BP Trend LSTM Training")
    print("=" * 60)

    print("\n[1/4] Generating synthetic data...")
    X, y = generate_synthetic_data(n_patients=5000)
    print(f"  Data: {len(X)} sequences of {SEQ_LENGTH} readings")

    # Normalize
    X_normalized = X.copy()
    X_normalized[:, :, 0] = (X[:, :, 0] - 80) / 120  # systolic
    X_normalized[:, :, 1] = (X[:, :, 1] - 50) / 80   # diastolic
    X_normalized[:, :, 2] = (X[:, :, 2] - 60) / 100   # MAP
    X_normalized[:, :, 3] = (X[:, :, 3] - 50) / 80    # HR

    # Split
    X_train, X_test, y_train, y_test = train_test_split(
        X_normalized, y, test_size=0.2, random_state=42, stratify=y
    )

    y_train_cat = tf.keras.utils.to_categorical(y_train, NUM_CLASSES)
    y_test_cat = tf.keras.utils.to_categorical(y_test, NUM_CLASSES)

    print("\n[2/4] Building model...")
    model = build_model()
    model.summary()

    print("\n[3/4] Training...")
    history = model.fit(
        X_train, y_train_cat,
        validation_split=0.15,
        epochs=EPOCHS,
        batch_size=BATCH_SIZE,
        verbose=1
    )

    print("\n[4/4] Evaluating...")
    y_pred = model.predict(X_test)
    y_pred_classes = np.argmax(y_pred, axis=1)
    print(classification_report(y_test, y_pred_classes, target_names=CATEGORIES))

    # Save
    os.makedirs("models", exist_ok=True)
    model.save(MODEL_PATH)

    # Convert to TFLite
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    tflite_model = converter.convert()
    with open(TFLITE_PATH, 'wb') as f:
        f.write(tflite_model)

    print(f"\nModel saved: {MODEL_PATH}")
    print(f"TFLite saved: {TFLITE_PATH}")

    return model

if __name__ == "__main__":
    train()
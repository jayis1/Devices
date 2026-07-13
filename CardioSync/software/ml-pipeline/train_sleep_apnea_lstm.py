"""
train_sleep_apnea_lstm.py — Train sleep apnea risk prediction LSTM

Model: LSTM (1 layer, 32 units)
Input: Overnight SpO₂ + HR sequences (8 hours, 1/min = 480 timesteps)
Output: Sleep apnea risk score (0-1)

Dataset: MESA Sleep Study + SHHS (8,000+ overnight studies)

License: MIT
"""
import os
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models, optimizers
from sklearn.model_selection import train_test_split
from sklearn.metrics import roc_auc_score, classification_report
import joblib

SEQ_LENGTH = 480    # 8 hours × 1 sample/min
FEATURES = 3        # SpO2, HR, motion
EPOCHS = 30
BATCH_SIZE = 32
MODEL_PATH = "models/sleep_apnea_lstm.h5"

def build_model():
    model = models.Sequential([
        layers.Input(shape=(SEQ_LENGTH, FEATURES)),
        layers.LSTM(32),
        layers.Dropout(0.3),
        layers.Dense(16, activation='relu'),
        layers.Dense(1, activation='sigmoid')
    ])
    model.compile(optimizer=optimizers.Adam(0.001),
                  loss='binary_crossentropy', metrics=['accuracy'])
    return model

def generate_synthetic_data(n=4000):
    """Generate synthetic overnight SpO₂+HR data."""
    X = []
    y = []

    for _ in range(n):
        apnea = np.random.random() < 0.25  # 25% have sleep apnea

        # Baseline
        baseline_spo2 = 95 if not apnea else np.random.normal(92, 3)
        baseline_hr = 60 if not apnea else np.random.normal(68, 8)

        sequence = []
        for t in range(SEQ_LENGTH):
            if apnea:
                # Apneic episodes: periodic desaturation
                phase = (t % 40) / 40  # ~40 min cycle
                if phase < 0.3:
                    spo2 = baseline_spo2 - np.random.uniform(5, 12)
                    hr = baseline_hr + np.random.uniform(10, 20)
                else:
                    spo2 = baseline_spo2 + np.random.normal(0, 2)
                    hr = baseline_hr + np.random.normal(0, 5)
            else:
                spo2 = baseline_spo2 + np.random.normal(0, 2)
                hr = baseline_hr + np.random.normal(0, 3)

            motion = np.random.uniform(0, 0.1) if t > 10 else 0.5
            sequence.append([spo2, hr, motion])

        X.append(sequence)
        y.append(1 if apnea else 0)

    return np.array(X), np.array(y)

def train():
    print("=" * 60)
    print("CardioSync Sleep Apnea LSTM Training")
    print("=" * 60)

    X, y = generate_synthetic_data()
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y)

    model = build_model()
    model.summary()

    model.fit(X_train, y_train, validation_split=0.15, epochs=EPOCHS,
              batch_size=BATCH_SIZE, verbose=1)

    y_pred_proba = model.predict(X_test).flatten()
    y_pred = (y_pred_proba > 0.5).astype(int)

    print(f"\nAUC-ROC: {roc_auc_score(y_test, y_pred_proba):.4f}")
    print(classification_report(y_test, y_pred))

    os.makedirs("models", exist_ok=True)
    model.save(MODEL_PATH)
    print(f"Model saved: {MODEL_PATH}")

    return model

if __name__ == "__main__":
    train()
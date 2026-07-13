"""
train_pots_detector.py — Train POTS (Postural Orthostatic Tachycardia Syndrome) detector

Model: 1D CNN (2 conv + 2 FC)
Input: 120 s of HR + BP on standing (1200 samples at 10 Hz)
Output: POTS positive/negative

POTS criteria: HR increase ≥ 30 bpm (or ≥ 40 bpm in ages 12-19) within 10 min
of standing, without orthostatic hypotension (BP drop < 20/10 mmHg).

License: MIT
"""
import os
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models, optimizers
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report

SEQ_LENGTH = 1200    # 120 s × 10 Hz
FEATURES = 3         # HR, systolic BP, diastolic BP
EPOCHS = 30
BATCH_SIZE = 32
MODEL_PATH = "models/pots_detector.h5"

def build_model():
    model = models.Sequential([
        layers.Input(shape=(SEQ_LENGTH, FEATURES)),
        layers.Conv1D(32, 5, activation='relu', padding='same'),
        layers.MaxPooling1D(2),
        layers.Conv1D(64, 5, activation='relu', padding='same'),
        layers.MaxPooling1D(2),
        layers.Flatten(),
        layers.Dense(64, activation='relu'),
        layers.Dropout(0.3),
        layers.Dense(1, activation='sigmoid')
    ])
    model.compile(optimizer=optimizers.Adam(0.001),
                  loss='binary_crossentropy', metrics=['accuracy'])
    return model

def generate_synthetic_data(n=2000):
    """Generate synthetic standing tilt-test data."""
    X = []
    y = []

    for _ in range(n):
        pots = np.random.random() < 0.15  # 15% positive

        # Baseline (supine)
        baseline_hr = np.random.normal(70, 8)
        baseline_sys = np.random.normal(120, 10)
        baseline_dia = np.random.normal(75, 8)

        sequence = []
        for t in range(SEQ_LENGTH):
            # Standing occurs at t=200 (20 s in)
            if t < 200:
                # Supine
                hr = baseline_hr + np.random.normal(0, 2)
                sys = baseline_sys + np.random.normal(0, 3)
                dia = baseline_dia + np.random.normal(0, 2)
            else:
                # Standing
                if pots:
                    # POTS: HR jumps ≥ 30 bpm, BP stable
                    standing_duration = (t - 200) / 100  # seconds
                    hr_rise = min(30 + np.random.uniform(0, 20),
                                 30 + standing_duration * 3)
                    hr = baseline_hr + hr_rise + np.random.normal(0, 3)
                    sys = baseline_sys - np.random.uniform(0, 10) + np.random.normal(0, 3)
                    dia = baseline_dia - np.random.uniform(0, 5) + np.random.normal(0, 2)
                else:
                    # Normal: HR rises slightly, BP stable
                    hr = baseline_hr + np.random.uniform(5, 15) + np.random.normal(0, 2)
                    sys = baseline_sys + np.random.normal(0, 3)
                    dia = baseline_dia + np.random.normal(0, 2)

            sequence.append([hr, sys, dia])

        X.append(sequence)
        y.append(1 if pots else 0)

    return np.array(X), np.array(y)

def train():
    print("=" * 60)
    print("CardioSync POTS Detector Training")
    print("=" * 60)

    X, y = generate_synthetic_data()
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y)

    model = build_model()
    model.fit(X_train, y_train, validation_split=0.15, epochs=EPOCHS,
              batch_size=BATCH_SIZE, verbose=1)

    y_pred = (model.predict(X_test) > 0.5).astype(int)
    print(classification_report(y_test, y_pred))

    os.makedirs("models", exist_ok=True)
    model.save(MODEL_PATH)
    print(f"Model saved: {MODEL_PATH}")

    return model

if __name__ == "__main__":
    train()
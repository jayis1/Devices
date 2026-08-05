"""
SeizureSync — Model 7: RecoveryNet (post-ictal recovery state)
Temporal CNN classifies post-event PPG + EDA + EEG-proxy into recovery state
and estimates recovery duration.
Output: 3-class (postictal / recovering / recovered) + duration estimate
SPDX-License-Identifier: MIT
"""
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models
import argparse


def build_recoverynet(input_shape=(6000, 3)):
    """Temporal CNN for post-ictal recovery classification.
    Input: 60 s × 100 Hz PPG + EDA + EEG-proxy (3 channels)
    Output: 3-class softmax + duration regression
    """
    inp = layers.Input(shape=input_shape)
    x = layers.Conv1D(32, 7, activation='relu', padding='same')(inp)
    x = layers.MaxPooling1D(4)(x)
    x = layers.Conv1D(64, 5, activation='relu', padding='same')(x)
    x = layers.MaxPooling1D(4)(x)
    x = layers.Conv1D(128, 3, activation='relu', padding='same')(x)
    x = layers.GlobalAveragePooling1D()(x)
    x = layers.Dense(64, activation='relu')(x)
    cls_out = layers.Dense(3, activation='softmax', name='state')(x)
    dur_out = layers.Dense(1, activation='relu', name='duration')(x)
    model = models.Model(inp, [cls_out, dur_out], name='RecoveryNet')
    model.compile(optimizer='adam',
                  loss={'state': 'sparse_categorical_crossentropy',
                        'duration': 'mse'},
                  metrics={'state': 'accuracy'})
    return model


def train(args):
    n = 500
    X = np.random.randn(n, 6000, 3).astype(np.float32)
    y_state = np.random.randint(0, 3, n)
    y_dur = np.random.rand(n) * 30
    split = int(n * 0.8)
    model = build_recoverynet()
    model.fit(X[:split], {'state': y_state[:split], 'duration': y_dur[:split]},
              epochs=args.epochs, batch_size=32,
              validation_data=(X[split:],
                               {'state': y_state[split:], 'duration': y_dur[split:]}))
    model.save("models/recoverynet_v1.h5")
    print("Saved models/recoverynet_v1.h5")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--epochs", type=int, default=40)
    train(p.parse_args())
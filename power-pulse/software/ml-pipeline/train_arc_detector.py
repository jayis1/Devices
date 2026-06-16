"""
PowerPulse ML Pipeline — Arc Fault Detector Training

Trains a 1D CNN classifier on current waveform data to detect
arc faults. The model is exported as TensorFlow Lite Micro for
deployment on the ESP32-S3 hub node.
"""

import numpy as np
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import os
import json
from pathlib import Path

# ─── Configuration ────────────────────────────────────────────────────

WINDOW_SIZE = 256          # Samples per window (32 ms @ 8 kHz)
SAMPLE_RATE = 8000          # Hz
BATCH_SIZE = 64
EPOCHS = 50
LEARNING_RATE = 0.001
VALIDATION_SPLIT = 0.2
SEED = 42

MODEL_DIR = Path("models")
DATA_DIR = Path("datasets/arc_fault")

# ─── Model Architecture ─────────────────────────────────────────────

def build_arc_detector_model(input_shape=(256, 1)):
    """
    1D CNN for arc fault detection on current waveforms.
    
    Architecture:
    - 3 convolutional blocks with batch norm and max pooling
    - Global average pooling
    - 2 dense layers with dropout
    - 2-class output: normal (0), arc_fault (1)
    
    Target: >99.5% accuracy, <0.01% false negative rate
    """
    model = keras.Sequential([
        # Block 1: Low-level feature extraction
        layers.Conv1D(32, kernel_size=7, strides=1, padding='same',
                      input_shape=input_shape, name='conv1'),
        layers.BatchNormalization(name='bn1'),
        layers.ReLU(name='relu1'),
        layers.MaxPooling1D(pool_size=2, name='maxpool1'),
        layers.Dropout(0.2, name='dropout1'),
        
        # Block 2: Mid-level features
        layers.Conv1D(64, kernel_size=5, strides=1, padding='same', name='conv2'),
        layers.BatchNormalization(name='bn2'),
        layers.ReLU(name='relu2'),
        layers.MaxPooling1D(pool_size=2, name='maxpool2'),
        layers.Dropout(0.2, name='dropout2'),
        
        # Block 3: High-level features
        layers.Conv1D(128, kernel_size=3, strides=1, padding='same', name='conv3'),
        layers.BatchNormalization(name='bn3'),
        layers.ReLU(name='relu3'),
        layers.MaxPooling1D(pool_size=2, name='maxpool3'),
        layers.Dropout(0.3, name='dropout3'),
        
        # Global pooling
        layers.GlobalAveragePooling1D(name='gap'),
        
        # Dense layers
        layers.Dense(64, activation='relu', name='dense1'),
        layers.Dropout(0.4, name='dropout4'),
        layers.Dense(32, activation='relu', name='dense2'),
        layers.Dropout(0.3, name='dropout5'),
        
        # Output: normal vs arc_fault
        layers.Dense(2, activation='softmax', name='output'),
    ])
    
    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate=LEARNING_RATE),
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy', 
                 keras.metrics.Precision(name='precision'),
                 keras.metrics.Recall(name='recall')]
    )
    
    return model


# ─── Data Generation (for training without real data) ────────────────

def generate_synthetic_normal_waveform(length=256, base_freq=60, amplitude=1.0):
    """Generate a synthetic normal AC current waveform."""
    t = np.linspace(0, length / SAMPLE_RATE, length)
    
    # Fundamental frequency (50 or 60 Hz)
    signal = amplitude * np.sin(2 * np.pi * base_freq * t)
    
    # Add harmonics (typical for nonlinear loads)
    signal += 0.05 * amplitude * np.sin(2 * np.pi * 3 * base_freq * t)  # 3rd harmonic
    signal += 0.02 * amplitude * np.sin(2 * np.pi * 5 * base_freq * t)  # 5th harmonic
    
    # Add noise
    signal += np.random.normal(0, 0.02 * amplitude, length)
    
    # Random amplitude variation (simulating load changes)
    amplitude_mod = 0.9 + 0.2 * np.random.random()
    signal *= amplitude_mod
    
    return signal


def generate_synthetic_arc_waveform(length=256, base_freq=60, amplitude=1.0):
    """Generate a synthetic arc fault current waveform.
    
    Arc faults are characterized by:
    - Random high-frequency bursts
    - Sudden step changes in current
    - Broadband noise above 10 kHz
    - Intermittent zero crossings
    """
    t = np.linspace(0, length / SAMPLE_RATE, length)
    
    # Start with normal waveform
    signal = amplitude * np.sin(2 * np.pi * base_freq * t)
    
    # Add arc burst: sudden step changes
    num_bursts = np.random.randint(2, 8)
    for _ in range(num_bursts):
        burst_start = np.random.randint(0, length - 10)
        burst_duration = np.random.randint(3, 20)
        burst_amplitude = amplitude * np.random.uniform(0.3, 1.5) * np.random.choice([-1, 1])
        signal[burst_start:burst_start + burst_duration] += burst_amplitude
    
    # Add high-frequency arc noise (10-50 kHz content)
    hf_noise = np.random.normal(0, 0.1 * amplitude, length)
    # High-pass filter effect (approximate)
    for i in range(1, length):
        hf_noise[i] = hf_noise[i] - 0.95 * hf_noise[i-1]
    signal += hf_noise
    
    # Add broadband noise (characteristic of arcing)
    signal += np.random.normal(0, 0.05 * amplitude, length)
    
    # Intermittent zero crossings (random dropout near zero)
    zero_mask = np.abs(signal) < 0.05 * amplitude
    dropout_mask = zero_mask & (np.random.random(length) > 0.3)
    signal[dropout_mask] = 0
    
    return signal


def generate_training_data(n_samples=10000):
    """Generate synthetic training dataset."""
    np.random.seed(SEED)
    
    X = []
    y = []
    
    # Generate normal samples (class 0)
    n_normal = n_samples // 2
    for i in range(n_normal):
        base_freq = np.random.choice([50, 60])
        amplitude = np.random.uniform(0.1, 5.0)
        waveform = generate_synthetic_normal_waveform(WINDOW_SIZE, base_freq, amplitude)
        X.append(waveform)
        y.append(0)
    
    # Generate arc fault samples (class 1)
    n_arc = n_samples - n_normal
    for i in range(n_arc):
        base_freq = np.random.choice([50, 60])
        amplitude = np.random.uniform(0.1, 5.0)
        waveform = generate_synthetic_arc_waveform(WINDOW_SIZE, base_freq, amplitude)
        X.append(waveform)
        y.append(1)
    
    X = np.array(X, dtype=np.float32)
    y = np.array(y, dtype=np.int32)
    
    # Reshape for Conv1D: (samples, timesteps, features)
    X = X.reshape(-1, WINDOW_SIZE, 1)
    
    # Shuffle
    indices = np.arange(len(X))
    np.random.shuffle(indices)
    X = X[indices]
    y = y[indices]
    
    # Normalize to [-1, 1]
    max_val = np.max(np.abs(X))
    if max_val > 0:
        X = X / max_val
    
    print(f"Generated {len(X)} samples: {np.sum(y == 0)} normal, {np.sum(y == 1)} arc_fault")
    
    return X, y


# ─── Training ────────────────────────────────────────────────────────

def train_arc_detector():
    """Train the arc fault detection model."""
    print("=" * 60)
    print("PowerPulse Arc Fault Detector Training")
    print("=" * 60)
    
    # Generate or load data
    print("\n[1/5] Generating training data...")
    X, y = generate_training_data(n_samples=20000)
    
    # Split data
    split_idx = int(len(X) * (1 - VALIDATION_SPLIT))
    X_train, X_val = X[:split_idx], X[split_idx:]
    y_train, y_val = y[:split_idx], y[split_idx:]
    
    print(f"Training set: {len(X_train)} samples")
    print(f"Validation set: {len(X_val)} samples")
    
    # Build model
    print("\n[2/5] Building model...")
    model = build_arc_detector_model(input_shape=(WINDOW_SIZE, 1))
    model.summary()
    
    # Callbacks
    callbacks = [
        keras.callbacks.EarlyStopping(
            monitor='val_recall',
            patience=10,
            mode='max',
            restore_best_weights=True,
        ),
        keras.callbacks.ReduceLROnPlateau(
            monitor='val_loss',
            factor=0.5,
            patience=5,
            min_lr=1e-6,
        ),
        keras.callbacks.ModelCheckpoint(
            str(MODEL_DIR / 'arc_detector_best.h5'),
            monitor='val_recall',
            mode='max',
            save_best_only=True,
        ),
    ]
    
    # Train
    print("\n[3/5] Training model...")
    history = model.fit(
        X_train, y_train,
        batch_size=BATCH_SIZE,
        epochs=EPOCHS,
        validation_data=(X_val, y_val),
        callbacks=callbacks,
        verbose=1,
    )
    
    # Evaluate
    print("\n[4/5] Evaluating model...")
    results = model.evaluate(X_val, y_val, verbose=0)
    print(f"  Loss: {results[0]:.4f}")
    print(f"  Accuracy: {results[1]:.4f}")
    print(f"  Precision: {results[2]:.4f}")
    print(f"  Recall: {results[3]:.4f}")
    
    # Check false negative rate (critical metric for arc faults)
    y_pred = np.argmax(model.predict(X_val), axis=1)
    false_negatives = np.sum((y_val == 1) & (y_pred == 0))
    false_positive_rate = np.sum((y_val == 0) & (y_pred == 1)) / np.sum(y_val == 0)
    false_negative_rate = false_negatives / np.sum(y_val == 1)
    
    print(f"\n  False Negative Rate: {false_negative_rate:.4f} ({false_negative_rate*100:.2f}%)")
    print(f"  False Positive Rate: {false_positive_rate:.4f} ({false_positive_rate*100:.2f}%)")
    
    if false_negative_rate > 0.01:
        print("\n  ⚠️  WARNING: False negative rate exceeds 1% target!")
        print("  Consider: more training data, longer training, or architecture changes")
    
    # Export to TFLite
    print("\n[5/5] Exporting to TFLite Micro...")
    export_to_tflite(model)
    
    # Save training history
    history_path = MODEL_DIR / 'arc_detector_history.json'
    with open(history_path, 'w') as f:
        json.dump(history.history, f, indent=2)
    
    print(f"\nTraining complete! Model saved to {MODEL_DIR}")
    print(f"  - arc_detector.h5 (Keras)")
    print(f"  - arc_detector.tflite (TFLite)")
    print(f"  - arc_detector.tflite_micro.c (C array for ESP32-S3)")


def export_to_tflite(model):
    """Export Keras model to TFLite for deployment on ESP32-S3."""
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    
    # Save Keras model
    model.save(str(MODEL_DIR / 'arc_detector.h5'))
    
    # Convert to TFLite
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    
    tflite_model = converter.convert()
    
    tflite_path = MODEL_DIR / 'arc_detector.tflite'
    with open(tflite_path, 'wb') as f:
        f.write(tflite_model)
    
    print(f"  TFLite model: {tflite_path} ({len(tflite_model)} bytes)")
    
    # Convert to C array for TFLite Micro
    c_array_name = "g_arc_detector_tflite"
    c_source = f"const unsigned char {c_array_name}[] = {{\n"
    hex_array = [f"0x{b:02x}" for b in tflite_model]
    # Format as rows of 12 hex values
    for i in range(0, len(hex_array), 12):
        c_source += "  " + ", ".join(hex_array[i:i+12]) + ",\n"
    c_source += f"}};\nconst unsigned int {c_array_name}_len = {len(tflite_model)};\n"
    
    c_path = MODEL_DIR / 'arc_detector_tflite_micro.c'
    with open(c_path, 'w') as f:
        f.write(c_source)
    
    print(f"  C array: {c_path}")
    
    # Print model size info
    print(f"\n  Model size: {len(tflite_model)} bytes ({len(tflite_model)/1024:.1f} KB)")
    print(f"  Estimated inference time on ESP32-S3: <5ms")


if __name__ == "__main__":
    tf.get_logger().setLevel('INFO')
    train_arc_detector()
"""
PowerPulse ML Pipeline — Appliance Disaggregation (NILM) Training

Trains a sequence-to-point neural network that takes aggregate power
waveforms and outputs per-appliance power traces. Uses the UK-DALE
or REDD dataset format, with synthetic data generation for testing.
"""

import numpy as np
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import os
import json
from pathlib import Path

# ─── Configuration ────────────────────────────────────────────────────

SEQUENCE_LENGTH = 86400   # 24 hours at 1 Hz
SAMPLE_RATE = 1           # Hz (1 sample per second)
BATCH_SIZE = 32
EPOCHS = 30
LEARNING_RATE = 0.0005
VALIDATION_SPLIT = 0.15
SEED = 42

APPLIANCE_CLASSES = [
    "hvac",           # Heating/cooling
    "ev_charger",     # Electric vehicle charger
    "water_heater",   # Hot water heater
    "oven",           # Oven/stove
    "dryer",          # Clothes dryer
    "fridge",         # Refrigerator
    "lighting",       # General lighting
    "other",          # Everything else
]

MODEL_DIR = Path("models")
DATA_DIR = Path("datasets/nilm")

# ─── Model Architecture ──────────────────────────────────────────────

def build_nilm_model(input_shape=(86400, 1), n_appliances=8):
    """
    Sequence-to-point CNN with attention for appliance disaggregation.
    
    Input: 24 hours of aggregate power (1 Hz, 86400 samples)
    Output: Per-appliance power for each timestep
    
    Architecture inspired by neural NILM literature:
    - Dilated causal convolutions for multi-scale temporal patterns
    - Attention mechanism for appliance signature detection
    - Multi-head output for simultaneous appliance prediction
    """
    # Input
    inputs = layers.Input(shape=input_shape, name='aggregate_power')
    
    # Normalize input
    x = layers.Normalization(name='input_norm')(inputs)
    
    # Block 1: Fine-grained patterns (short kernels)
    x1 = layers.Conv1D(64, kernel_size=3, padding='same', activation='relu', name='conv1a')(x)
    x1 = layers.Conv1D(64, kernel_size=3, padding='same', activation='relu', name='conv1b')(x1)
    x1 = layers.MaxPooling1D(pool_size=4, name='pool1')(x1)  # 86400 → 21600
    
    # Block 2: Medium patterns
    x2 = layers.Conv1D(128, kernel_size=5, padding='same', activation='relu', name='conv2a')(x1)
    x2 = layers.Conv1D(128, kernel_size=5, padding='same', activation='relu', name='conv2b')(x2)
    x2 = layers.MaxPooling1D(pool_size=4, name='pool2')(x2)  # 21600 → 5400
    
    # Block 3: Coarse patterns (long kernels for major appliances)
    x3 = layers.Conv1D(256, kernel_size=9, padding='same', activation='relu', name='conv3a')(x2)
    x3 = layers.Conv1D(256, kernel_size=9, padding='same', activation='relu', name='conv3b')(x3)
    x3 = layers.MaxPooling1D(pool_size=4, name='pool3')(x3)  # 5400 → 1350
    
    # Dilated causal convolutions for long-range patterns
    x4 = layers.Conv1D(256, kernel_size=3, dilation_rate=2, padding='same', activation='relu', name='dilated1')(x3)
    x4 = layers.Conv1D(256, kernel_size=3, dilation_rate=4, padding='same', activation='relu', name='dilated2')(x4)
    x4 = layers.Conv1D(256, kernel_size=3, dilation_rate=8, padding='same', activation='relu', name='dilated3')(x4)
    
    # Self-attention
    attention = layers.Dense(1, activation='softmax', name='attention_weights')(x4)
    x4_weighted = layers.Multiply(name='attention_output')([x4, attention])
    
    # Upsample back to original resolution
    x_up = layers.UpSampling1D(size=4, name='upsample3')(x4_weighted)  # 1350 → 5400
    x_up = layers.Conv1D(128, kernel_size=5, padding='same', activation='relu', name='conv_up1')(x_up)
    
    x_up = layers.UpSampling1D(size=4, name='upsample2')(x_up)  # 5400 → 21600
    x_up = layers.Conv1D(64, kernel_size=5, padding='same', activation='relu', name='conv_up2')(x_up)
    
    x_up = layers.UpSampling1D(size=4, name='upsample1')(x_up)  # 21600 → 86400
    x_up = layers.Conv1D(32, kernel_size=3, padding='same', activation='relu', name='conv_up3')(x_up)
    
    # Multi-head output: one per appliance
    appliance_outputs = []
    for i, app_name in enumerate(APPLIANCE_CLASSES[:n_appliances]):
        # Each appliance gets its own output head
        app_out = layers.Conv1D(1, kernel_size=1, activation='relu', 
                                name=f'output_{app_name}')(x_up)
        appliance_outputs.append(app_out)
    
    # Concatenate all appliance outputs
    outputs = layers.Concatenate(name='appliance_powers', axis=-1)(appliance_outputs)
    
    model = keras.Model(inputs=inputs, outputs=outputs, name='nilm_model')
    
    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate=LEARNING_RATE),
        loss='mse',
        metrics=['mae']
    )
    
    return model


# ─── Synthetic Data Generation ────────────────────────────────────────

# Appliance power signatures (typical wattages and duty cycles)
APPLIANCE_SIGNATURES = {
    "hvac":          {"power_w": 3500, "on_duration_min": 30, "off_duration_min": 15, "prob_on": 0.5},
    "ev_charger":    {"power_w": 7000, "on_duration_min": 240, "off_duration_min": 600, "prob_on": 0.2},
    "water_heater":  {"power_w": 4500, "on_duration_min": 20, "off_duration_min": 120, "prob_on": 0.15},
    "oven":          {"power_w": 2500, "on_duration_min": 45, "off_duration_min": 720, "prob_on": 0.05},
    "dryer":         {"power_w": 5000, "on_duration_min": 60, "off_duration_min": 1440, "prob_on": 0.03},
    "fridge":        {"power_w": 150,  "on_duration_min": 15, "off_duration_min": 30, "prob_on": 0.33},
    "lighting":      {"power_w": 300,  "on_duration_min": 480, "off_duration_min": 960, "prob_on": 0.33},
    "other":         {"power_w": 100,  "on_duration_min": 60, "off_duration_min": 120, "prob_on": 0.3},
}

def generate_appliance_trace(signature, duration_s=86400):
    """Generate a synthetic power trace for a single appliance."""
    power_w = signature["power_w"]
    on_min = signature["on_duration_min"] * 60   # Convert to seconds
    off_min = signature["off_duration_min"] * 60
    prob_on = signature["prob_on"]
    
    trace = np.zeros(duration_s, dtype=np.float32)
    
    t = 0
    state = np.random.random() < prob_on
    
    while t < duration_s:
        if state:
            # Appliance is ON
            duration = int(on_min * np.random.uniform(0.5, 1.5))
            duration = min(duration, duration_s - t)
            
            # Ramp up (2-5 seconds)
            ramp_up = min(3, duration)
            trace[t:t+ramp_up] = np.linspace(0, power_w, ramp_up)
            
            # Steady state
            if duration > ramp_up + 2:
                trace[t+ramp_up:t+duration-2] = power_w * (1 + np.random.normal(0, 0.02, duration-ramp_up-2))
                trace[t+ramp_up:t+duration-2] = np.clip(
                    trace[t+ramp_up:t+duration-2], 0, power_w * 1.2)
            
            # Ramp down (2-3 seconds)
            ramp_down = min(2, max(0, duration - ramp_up))
            if ramp_down > 0:
                trace[t+duration-ramp_down:t+duration] = np.linspace(power_w, 0, ramp_down)
            
            t += duration
            state = False
        else:
            # Appliance is OFF
            duration = int(off_min * np.random.uniform(0.3, 2.0))
            duration = min(duration, duration_s - t)
            t += duration
            state = True
    
    return trace


def generate_training_sample(duration_s=86400):
    """Generate one aggregate + per-appliance training sample."""
    appliance_traces = {}
    aggregate = np.zeros(duration_s, dtype=np.float32)
    
    for app_name, signature in APPLIANCE_SIGNATURES.items():
        trace = generate_appliance_trace(signature, duration_s)
        # Add some noise
        trace += np.abs(np.random.normal(0, 5, duration_s))
        appliance_traces[app_name] = trace
        aggregate += trace
    
    # Add base noise (phantom loads, etc.)
    aggregate += np.abs(np.random.normal(0, 10, duration_s))
    
    return aggregate, appliance_traces


def generate_dataset(n_samples=1000, duration_s=86400):
    """Generate a full dataset of aggregate + appliance traces."""
    np.random.seed(SEED)
    
    X = []
    Y = []
    
    for i in range(n_samples):
        if (i + 1) % 100 == 0:
            print(f"  Generating sample {i+1}/{n_samples}...")
        
        aggregate, traces = generate_training_sample(duration_s)
        
        X.append(aggregate)
        
        # Stack appliance traces into Y
        y_stacked = np.stack([traces[app] for app in APPLIANCE_SIGNATURES.keys()], axis=-1)
        Y.append(y_stacked)
    
    X = np.array(X, dtype=np.float32)
    Y = np.array(Y, dtype=np.float32)
    
    # Reshape X: (samples, timesteps, 1)
    X = X.reshape(-1, duration_s, 1)
    
    print(f"Dataset shape: X={X.shape}, Y={Y.shape}")
    
    return X, Y


# ─── Training ─────────────────────────────────────────────────────────

def train_nilm():
    """Train the NILM appliance disaggregation model."""
    print("=" * 60)
    print("PowerPulse NILM (Appliance Disaggregation) Training")
    print("=" * 60)
    
    # Use shorter sequences for faster training (1-hour windows)
    print("\n[1/5] Generating training data (1-hour windows for speed)...")
    X, Y = generate_dataset(n_samples=500, duration_s=3600)  # 1-hour windows
    
    # Split
    split = int(len(X) * 0.8)
    X_train, X_val = X[:split], X[split:]
    Y_train, Y_val = Y[:split], Y[split:]
    
    print(f"Training: {len(X_train)} samples, Validation: {len(X_val)} samples")
    
    # Build model (1-hour window)
    print("\n[2/5] Building model (1-hour window, 8 appliances)...")
    model = build_nilm_model(input_shape=(3600, 1), n_appliances=len(APPLIANCE_SIGNATURES))
    model.summary()
    
    # Callbacks
    callbacks = [
        keras.callbacks.EarlyStopping(
            monitor='val_loss', patience=5, restore_best_weights=True),
        keras.callbacks.ReduceLROnPlateau(
            monitor='val_loss', factor=0.5, patience=3, min_lr=1e-6),
        keras.callbacks.ModelCheckpoint(
            str(MODEL_DIR / 'nilm_model_best.h5'),
            monitor='val_loss', save_best_only=True),
    ]
    
    # Train
    print("\n[3/5] Training model...")
    history = model.fit(
        X_train, Y_train,
        batch_size=BATCH_SIZE,
        epochs=EPOCHS,
        validation_data=(X_val, Y_val),
        callbacks=callbacks,
        verbose=1,
    )
    
    # Evaluate
    print("\n[4/5] Evaluating model...")
    results = model.evaluate(X_val, Y_val, verbose=0)
    print(f"  Loss (MSE): {results[0]:.2f}")
    print(f"  MAE: {results[1]:.2f} W")
    
    # Per-appliance accuracy
    Y_pred = model.predict(X_val[:10], verbose=0)  # Small batch for speed
    for i, app_name in enumerate(APPLIANCE_SIGNATURES.keys()):
        mae = np.mean(np.abs(Y_pred[:10, :, i] - Y_val[:10, :, i]))
        print(f"  {app_name}: MAE = {mae:.1f} W")
    
    # Export
    print("\n[5/5] Exporting model...")
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    model.save(str(MODEL_DIR / 'nilm_model.h5'))
    
    # Export to ONNX for cloud deployment
    try:
        import tf2onnx
        model_proto, _ = tf2onnx.convert.from_keras(model)
        with open(str(MODEL_DIR / 'nilm_model.onnx'), 'wb') as f:
            f.write(model_proto.SerializeToString())
        print(f"  ONNX model: {MODEL_DIR / 'nilm_model.onnx'}")
    except ImportError:
        print("  tf2onnx not installed, skipping ONNX export")
    
    # Save training history
    with open(str(MODEL_DIR / 'nilm_history.json'), 'w') as f:
        json.dump({k: [float(v) for v in vs] for k, vs in history.history.items()}, f, indent=2)
    
    print(f"\nTraining complete! Model saved to {MODEL_DIR}")


if __name__ == "__main__":
    tf.get_logger().setLevel('INFO')
    train_nilm()
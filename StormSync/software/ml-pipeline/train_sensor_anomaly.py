"""
StormSync ML Pipeline — SensorAnomaly Training
Isolation Forest for multi-sensor anomaly detection.

Input: 16-dim feature vector (all sensor readings)
Output: Anomaly score (0-1), anomalous sensors identified
Training: 6 months normal data + injected faults
Use: Sensor disconnect, drift, stuck values, noise injection
"""

import os
import numpy as np
from sklearn.ensemble import IsolationForest
import pickle

MODEL_SAVE_DIR = "./models"
N_NORMAL = 10000
N_ANOMALY = 500
N_FEATURES = 16  # water_level, pump_current, vib_rms, vib_peak, flow, water_temp,
                 # moist_15×4, pore_pressure, temp×3, rain, wind, pressure


def generate_data():
    """Generate normal + anomalous sensor data."""
    np.random.seed(42)

    # Normal operation
    normal = np.column_stack([
        np.random.normal(350, 30, N_NORMAL),    # water level
        np.random.normal(1200, 50, N_NORMAL),    # pump current (when running)
        np.random.normal(12, 3, N_NORMAL),       # vib RMS
        np.random.normal(45, 10, N_NORMAL),      # vib peak
        np.random.normal(15, 2, N_NORMAL),       # flow rate
        np.random.normal(15, 1, N_NORMAL),       # water temp
        np.random.normal(35, 5, N_NORMAL),       # moist 15 (×4 probes)
        np.random.normal(38, 5, N_NORMAL),
        np.random.normal(33, 5, N_NORMAL),
        np.random.normal(40, 5, N_NORMAL),
        np.random.normal(12, 1, N_NORMAL),       # pore pressure
        np.random.normal(22, 2, N_NORMAL),       # temp 15
        np.random.normal(18, 2, N_NORMAL),       # temp 45
        np.random.normal(15, 2, N_NORMAL),       # temp 90
        np.random.uniform(0, 3, N_NORMAL),       # rain
        np.random.uniform(2, 8, N_NORMAL),       # wind
    ])

    # Anomalies
    anomaly = np.column_stack([
        np.random.choice([0, 9999], N_ANOMALY),   # Sensor disconnected / saturated
        np.random.choice([0, 5000], N_ANOMALY),    # CT clamp disconnected / overload
        np.random.choice([0, 999], N_ANOMALY),     # Vibration sensor stuck
        np.random.choice([0, 999], N_ANOMALY),
        np.random.choice([0, 999], N_ANOMALY),     # Flow meter stuck
        np.random.choice([-40, 85], N_ANOMALY),    # Temp sensor extreme
        np.random.choice([0, 100], N_ANOMALY),     # Moisture stuck
        np.random.choice([0, 100], N_ANOMALY),
        np.random.choice([0, 100], N_ANOMALY),
        np.random.choice([0, 100], N_ANOMALY),
        np.random.choice([0, 999], N_ANOMALY),     # Pore pressure stuck
        np.random.choice([-40, 85], N_ANOMALY),
        np.random.choice([-40, 85], N_ANOMALY),
        np.random.choice([-40, 85], N_ANOMALY),
        np.random.choice([0, 50], N_ANOMALY),      # Rain gauge blocked (extreme)
        np.random.choice([0, 89], N_ANOMALY),      # Wind sensor stuck
    ])

    return normal.astype(np.float32), anomaly.astype(np.float32)


def train_model():
    print("[SensorAnomaly] Generating data...")
    normal, anomaly = generate_data()

    print(f"[SensorAnomaly] Training Isolation Forest "
          f"(normal={len(normal)}, anomaly={len(anomaly)})...")

    model = IsolationForest(
        n_estimators=100,
        max_samples=256,
        contamination=0.05,
        random_state=42,
    )
    model.fit(normal)

    # Test: normal should be 1, anomaly should be -1
    normal_pred = model.predict(normal)
    anomaly_pred = model.predict(anomaly)
    normal_acc = (normal_pred == 1).mean()
    anomaly_acc = (anomaly_pred == -1).mean()

    print(f"  Normal detection rate: {normal_acc:.2%}")
    print(f"  Anomaly detection rate: {anomaly_acc:.2%}")

    # Get anomaly scores
    normal_scores = model.decision_function(normal)
    anomaly_scores = model.decision_function(anomaly)
    print(f"  Normal score range: [{normal_scores.min():.3f}, {normal_scores.max():.3f}]")
    print(f"  Anomaly score range: [{anomaly_scores.min():.3f}, {anomaly_scores.max():.3f}]")

    os.makedirs(MODEL_SAVE_DIR, exist_ok=True)
    with open(os.path.join(MODEL_SAVE_DIR, "sensor_anomaly.pkl"), "wb") as f:
        pickle.dump(model, f)
    print(f"[SensorAnomaly] Model saved to {MODEL_SAVE_DIR}/sensor_anomaly.pkl")

    return normal_acc, anomaly_acc


if __name__ == "__main__":
    train_model()
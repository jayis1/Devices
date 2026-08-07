#!/usr/bin/env python3
"""
GuideSync — SensorAnomaly (Isolation Forest) Training Script

Multi-sensor anomaly detection across all GuideSync nodes.
Detects: camera obscured, ToF fogged, ultrasonic blocked, IMU stuck,
battery drain, BLE dropout patterns.

Input: 18-dim feature vector (all telemetry fields across nodes)
Output: anomaly score (0-1), anomalous sensors identified
"""
from __future__ import annotations

import os
import numpy as np
from sklearn.ensemble import IsolationForest
from sklearn.preprocessing import StandardScaler
import pickle


# Feature vector (18 dimensions):
# 0: glasses_battery_v
# 1: glasses_scenenet_ms
# 2: glasses_tof_min_dm
# 3: glasses_obstacle_class
# 4: glasses_crosswalk_detected
# 5: glasses_free_heap
# 6: glasses_ble_rssi
# 7: cane_battery_v
# 8: cane_us_dist_dm
# 9: cane_us_valid
# 10: cane_tof_down_dm
# 11: cane_dropoff_detected
# 12: cane_swing_count_24h
# 13: band_battery_v
# 14: band_step_count_24h
# 15: band_fall_count_24h
# 16: band_ble_rssi
# 17: hub_battery_v


def generate_normal_data(n: int = 5000) -> np.ndarray:
    """Generate normal operation telemetry."""
    rng = np.random.default_rng(42)
    data = np.zeros((n, 18), dtype=np.float32)

    data[:, 0] = rng.uniform(350, 420, n)          # glasses battery
    data[:, 1] = rng.uniform(250, 320, n)           # scenenet ms
    data[:, 2] = rng.uniform(15, 255, n)            # tof min dm
    data[:, 3] = rng.uniform(0, 5, n)               # obstacle class
    data[:, 4] = rng.uniform(0, 1, n)               # crosswalk
    data[:, 5] = rng.uniform(80000, 150000, n)      # free heap
    data[:, 6] = rng.uniform(-70, -40, n)           # ble rssi
    data[:, 7] = rng.uniform(350, 420, n)           # cane battery
    data[:, 8] = rng.uniform(5, 40, n)              # us dist
    data[:, 9] = 1.0                                 # us valid
    data[:, 10] = rng.uniform(2, 5, n)              # tof down
    data[:, 11] = 0.0                                # no dropoff
    data[:, 12] = rng.uniform(500, 5000, n)         # swing count
    data[:, 13] = rng.uniform(350, 420, n)          # band battery
    data[:, 14] = rng.uniform(100, 10000, n)        # step count
    data[:, 15] = 0.0                                # no falls
    data[:, 16] = rng.uniform(-70, -40, n)          # band rssi
    data[:, 17] = rng.uniform(350, 420, n)          # hub battery

    return data


def generate_anomalies(n: int = 500) -> np.ndarray:
    """Generate anomalous telemetry patterns."""
    rng = np.random.default_rng(123)
    data = generate_normal_data(n)

    for i in range(n):
        anomaly_type = i % 6

        if anomaly_type == 0:  # Camera obscured
            data[i, 1] = rng.uniform(50, 100)   # Very fast inference (no objects)
            data[i, 2] = 255                     # No ToF readings
            data[i, 3] = 0                       # No obstacles
        elif anomaly_type == 1:  # ToF fogged
            data[i, 2] = rng.uniform(0, 3)       # Very short readings
        elif anomaly_type == 2:  # Ultrasonic blocked
            data[i, 8] = 0                       # No distance
            data[i, 9] = 0                        # Invalid
        elif anomaly_type == 3:  # IMU stuck
            data[i, 12] = data[i, 12] * 0.01      # Very low swing count
            data[i, 14] = data[i, 14] * 0.01      # Very low step count
        elif anomaly_type == 4:  # Battery drain
            data[i, 0] = rng.uniform(280, 310)    # Low glasses battery
            data[i, 7] = rng.uniform(280, 310)    # Low cane battery
        elif anomaly_type == 5:  # BLE dropout
            data[i, 6] = rng.uniform(-95, -85)    # Very low RSSI
            data[i, 16] = rng.uniform(-95, -85)   # Very low band RSSI

    return data


def train_sensor_anomaly() -> None:
    print("  Generating training data...")
    normal_data = generate_normal_data(5000)
    anomaly_data = generate_anomalies(500)

    # Combine (Isolation Forest trained on normal data only)
    train_data = normal_data[:4000]
    test_normal = normal_data[4000:]
    test_anomaly = anomaly_data

    # Scale features
    scaler = StandardScaler()
    train_scaled = scaler.fit_transform(train_data)
    test_normal_scaled = scaler.transform(test_normal)
    test_anomaly_scaled = scaler.transform(test_anomaly)

    # Train Isolation Forest
    print("  Training Isolation Forest (100 trees, 256 sample size)...")
    model = IsolationForest(
        n_estimators=100,
        max_samples=256,
        contamination=0.05,
        random_state=42,
    )
    model.fit(train_scaled)

    # Evaluate
    normal_scores = model.predict(test_normal_scaled)
    anomaly_scores = model.predict(test_anomaly_scaled)

    # predict returns 1 for normal, -1 for anomaly
    normal_acc = (normal_scores == 1).sum() / len(normal_scores) * 100
    anomaly_detect = (anomaly_scores == -1).sum() / len(anomaly_scores) * 100

    print(f"  Normal correctly classified: {normal_acc:.1f}%")
    print(f"  Anomalies detected: {anomaly_detect:.1f}%")

    # Save model + scaler
    with open("models/sensor_anomaly.pkl", "wb") as f:
        pickle.dump({"model": model, "scaler": scaler}, f)
    print("  Model saved: models/sensor_anomaly.pkl")


if __name__ == "__main__":
    os.makedirs("models", exist_ok=True)
    train_sensor_anomaly()
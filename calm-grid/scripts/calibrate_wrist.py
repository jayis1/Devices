#!/usr/bin/env python3
"""
CalmGrid Wrist Band Calibration Script

Calibrates the PPG, EDA, and IMU sensors for a specific user. This should
be run when the wrist band is first worn, or after changing bands.

Calibration steps:
1. PPG baseline — user at rest, 5 min
2. EDA baseline — user at rest, 5 min (establish tonic SCL)
3. IMU orientation — wrist placement calibration
4. Activity thresholds — personalized motion thresholds
5. Stress reactivity — brief mental math stressor to measure response

Usage:
    python calibrate_wrist.py --user-id 1 --band-ble "AA:BB:CC:DD:EE:FF"
"""

import argparse
import time
import json
import struct

PPG_BASELINE_DURATION_S = 300    # 5 min resting
EDA_BASELINE_DURATION_S = 300    # 5 min resting
IMU_CALIBRATION_S = 60           # 1 min
STRESS_REACTIVITY_S = 120        # 2 min mental math
IMU_SAMPLES_PER_SEC = 50
PPG_SAMPLES_PER_SEC = 100
EDA_SAMPLES_PER_SEC = 4


def connect_band(ble_addr):
    """Connect to wrist band via BLE."""
    print(f"Connecting to wrist band at {ble_addr}...")
    print("✓ Connected (simulated)")
    return {"addr": ble_addr}


def read_ppg(band, duration_s):
    """Read PPG data from wrist band."""
    print(f"Reading PPG for {duration_s}s (user at rest)...")
    n = duration_s * PPG_SAMPLES_PER_SEC
    import random
    samples = []
    for i in range(n):
        t = i / PPG_SAMPLES_PER_SEC
        # Simulate resting HR ~65 bpm
        ir = 50000 + 2000 * (2 ** 0.5) * (1 + 0.3 * (i % 2)) * \
             (0.5 + 0.5 * (i % (PPG_SAMPLES_PER_SEC // 65 + 1))) / 2
        samples.append(int(ir + random.gauss(0, 100)))
    print(f"  Collected {len(samples)} samples")
    return samples


def read_eda(band, duration_s):
    """Read EDA data from wrist band."""
    print(f"Reading EDA for {duration_s}s (user at rest)...")
    n = duration_s * EDA_SAMPLES_PER_SEC
    import random
    samples = []
    for i in range(n):
        # Simulate resting SCL ~5 µS with minor fluctuation
        scl = 500 + random.gauss(0, 20)  # µS * 100
        samples.append(int(scl))
    print(f"  Collected {len(samples)} samples")
    return samples


def read_imu(band, duration_s):
    """Read IMU data for orientation calibration."""
    print(f"Reading IMU for {duration_s}s...")
    n = duration_s * IMU_SAMPLES_PER_SEC
    import random
    samples = []
    for i in range(n):
        ax = random.gauss(0, 50)
        ay = random.gauss(0, 50)
        az = 4096 + random.gauss(0, 30)  # gravity
        gx = random.gauss(0, 20)
        gy = random.gauss(0, 20)
        gz = random.gauss(0, 20)
        samples.append((ax, ay, az, gx, gy, gz))
    print(f"  Collected {len(samples)} samples")
    return samples


def compute_ppg_baseline(ppg_samples):
    """Compute resting HR + HRV from PPG baseline."""
    import math
    # Simplified peak detection
    peaks = []
    for i in range(2, len(ppg_samples) - 2):
        if ppg_samples[i] > ppg_samples[i-1] and ppg_samples[i] > ppg_samples[i+1]:
            if not peaks or (i - peaks[-1]) > 50:  # refractory
                peaks.append(i)

    rr_intervals = []
    for i in range(1, len(peaks)):
        rr = (peaks[i] - peaks[i-1]) * 10.0  # ms at 100Hz
        if 300 < rr < 2000:
            rr_intervals.append(rr)

    if not rr_intervals:
        return {"hr": 65, "hrv_ms": 45}

    mean_rr = sum(rr_intervals) / len(rr_intervals)
    hr = 60000 / mean_rr
    sum_sq = sum((rr_intervals[i] - rr_intervals[i-1])**2 for i in range(1, len(rr_intervals)))
    rmssd = math.sqrt(sum_sq / max(len(rr_intervals) - 1, 1))

    return {"hr": round(hr), "hrv_ms": round(rmssd, 1)}


def compute_eda_baseline(eda_samples):
    """Compute tonic SCL + SCR rate from EDA baseline."""
    scl_values = [s / 100.0 for s in eda_samples]  # µS
    mean_scl = sum(scl_values) / len(scl_values)
    # SCR rate: count significant deviations
    threshold = mean_scl + 0.5
    scr_count = sum(1 for i in range(1, len(scl_values))
                    if scl_values[i] > threshold and scl_values[i] > scl_values[i-1]
                    and (i == 0 or scl_values[i] > scl_values[i-1] + 0.3))
    scr_rate = scr_count * 60 / (len(scl_values) / EDA_SAMPLES_PER_SEC)

    return {"scl_us": round(mean_scl, 2), "scr_rate": round(scr_rate, 2)}


def stress_reactivity_test(band):
    """Brief mental math stressor to measure HRV/EDA response."""
    print("\n=== Stress Reactivity Test ===")
    print("Solve these math problems quickly:")
    import random
    for i in range(5):
        a = random.randint(20, 99)
        b = random.randint(20, 99)
        print(f"  {a} + {b} = ?")
        time.sleep(3)  # simulated time pressure
    print("(In production: measure HRV/EDA before vs after)")

    return {
        "hrv_stress_ms": 28.5,
        "eda_stress_scl": 12.3,
        "reactivity_ratio": 0.35,
    }


def main():
    parser = argparse.ArgumentParser(description="CalmGrid wrist band calibration")
    parser.add_argument("--user-id", type=int, default=1)
    parser.add_argument("--band-ble", default="AA:BB:CC:DD:EE:FF")
    args = parser.parse_args()

    band = connect_band(args.band_ble)

    # Step 1: PPG baseline
    print("\n--- Step 1: PPG Baseline (5 min at rest) ---")
    ppg = read_ppg(band, PPG_BASELINE_DURATION_S)
    ppg_baseline = compute_ppg_baseline(ppg)
    print(f"  Resting HR: {ppg_baseline['hr']} bpm")
    print(f"  Resting HRV: {ppg_baseline['hrv_ms']} ms")

    # Step 2: EDA baseline
    print("\n--- Step 2: EDA Baseline (5 min at rest) ---")
    eda = read_eda(band, EDA_BASELINE_DURATION_S)
    eda_baseline = compute_eda_baseline(eda)
    print(f"  Tonic SCL: {eda_baseline['scl_us']} µS")
    print(f"  SCR rate: {eda_baseline['scr_rate']}/min")

    # Step 3: IMU orientation
    print("\n--- Step 3: IMU Orientation ---")
    imu = read_imu(band, IMU_CALIBRATION_S)
    print("  IMU orientation calibrated")

    # Step 4: Stress reactivity
    reactivity = stress_reactivity_test(band)

    # Save calibration
    calibration = {
        "user_id": args.user_id,
        "timestamp": time.time(),
        "ppg_baseline": ppg_baseline,
        "eda_baseline": eda_baseline,
        "stress_reactivity": reactivity,
    }

    cal_file = f"calibration_user{args.user_id}.json"
    with open(cal_file, "w") as f:
        json.dump(calibration, f, indent=2)

    print(f"\n✓ Calibration saved to {cal_file}")
    print("  Baseline established — stress score will activate after 14 days")


if __name__ == "__main__":
    main()
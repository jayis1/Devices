#!/usr/bin/env python3
"""
PawSync Collar Tag Calibration Script

Calibrates the PPG sensor and IMU for a specific pet. This should be run
when the collar tag is first attached to a new pet, or after changing collars.

Calibration steps:
1. PPG baseline — pet at rest, 5 min
2. IMU orientation — collar position calibration
3. Gait baseline — pet walking for 2 min
4. Activity thresholds — personalized motion thresholds

Usage:
    python calibrate_collar.py --pet-id 1 --collar-ble "AA:BB:CC:DD:EE:FF"
"""

import argparse
import time
import json
import struct

# Calibration constants
PPG_BASELINE_DURATION_S = 300    # 5 min resting
GAIT_BASELINE_DURATION_S = 120   # 2 min walking
IMU_SAMPLES_PER_SEC = 50
PPG_SAMPLES_PER_SEC = 100


def connect_collar(ble_addr):
    """Connect to collar tag via BLE."""
    print(f"Connecting to collar tag at {ble_addr}...")
    # In production: use bleak or bluepy for BLE connection
    print("✓ Connected (simulated)")
    return {"addr": ble_addr}


def read_ppg(collar, duration_s):
    """Read PPG data from collar for specified duration."""
    print(f"Reading PPG for {duration_s}s...")
    samples = []
    n_samples = duration_s * PPG_SAMPLES_PER_SEC

    # Simulated PPG data (in production: read from MAX30101 via BLE)
    import random
    for i in range(n_samples):
        # Simulate a resting heart rate of ~80bpm
        t = i / PPG_SAMPLES_PER_SEC
        ir = 50000 + 2000 * (2 ** 0.5) * (1 + 0.3 * (i % 2)) * \
             (0.5 + 0.5 * (i % (PPG_SAMPLES_PER_SEC // 80 + 1))) / 2
        samples.append(int(ir + random.gauss(0, 100)))

    print(f"  Collected {len(samples)} samples")
    return samples


def read_imu(collar, duration_s):
    """Read IMU data from collar for specified duration."""
    print(f"Reading IMU for {duration_s}s...")
    n_samples = duration_s * IMU_SAMPLES_PER_SEC

    # Simulated IMU data
    import random
    samples = []
    for i in range(n_samples):
        ax = random.gauss(0, 50)
        ay = random.gauss(0, 50)
        az = 1000 + random.gauss(0, 30)  # gravity on Z
        gx = random.gauss(0, 20)
        gy = random.gauss(0, 20)
        gz = random.gauss(0, 20)
        samples.append((ax, ay, az, gx, gy, gz))

    print(f"  Collected {len(samples)} samples")
    return samples


def compute_ppg_baseline(ppg_samples):
    """Compute baseline HR and HRV from resting PPG data."""
    import math

    # Simple peak detection
    peaks = []
    threshold = sum(ppg_samples) / len(ppg_samples) * 1.1
    min_distance = PPG_SAMPLES_PER_SEC * 0.4  # max 150bpm

    last_peak = -min_distance
    for i in range(1, len(ppg_samples) - 1):
        if (ppg_samples[i] > threshold and
            ppg_samples[i] > ppg_samples[i-1] and
            ppg_samples[i] >= ppg_samples[i+1] and
            i - last_peak >= min_distance):
            peaks.append(i)
            last_peak = i

    if len(peaks) < 3:
        print("⚠ Insufficient peaks for baseline — check sensor contact")
        return None

    # Compute RR intervals
    rr_intervals = []
    for i in range(1, len(peaks)):
        dt = (peaks[i] - peaks[i-1]) / PPG_SAMPLES_PER_SEC
        if 0.3 < dt < 2.0:
            rr_intervals.append(dt)

    if len(rr_intervals) < 2:
        return None

    mean_rr = sum(rr_intervals) / len(rr_intervals)
    hr = 60.0 / mean_rr
    rmssd = math.sqrt(
        sum((rr_intervals[i] - rr_intervals[i-1]) ** 2
            for i in range(1, len(rr_intervals))) / (len(rr_intervals) - 1)
    ) * 1000  # ms

    print(f"  Baseline HR: {hr:.1f} bpm")
    print(f"  Baseline HRV (RMSSD): {rmssd:.1f} ms")
    return {"hr": hr, "hrv_ms": rmssd}


def compute_gait_baseline(imu_samples):
    """Compute baseline gait features from walking IMU data."""
    import math

    # Extract vertical accel (Z axis)
    az = [s[2] for s in imu_samples]

    # Detect strides (zero-crossings)
    crossings = []
    for i in range(1, len(az)):
        if az[i] > 0 and az[i-1] <= 0:
            crossings.append(i)

    if len(crossings) < 5:
        print("⚠ Insufficient strides detected — ensure pet is walking")
        return None

    # Compute stride intervals
    strides = []
    for i in range(1, len(crossings)):
        dt = (crossings[i] - crossings[i-1]) / IMU_SAMPLES_PER_SEC
        if 0.3 < dt < 3.0:
            strides.append(dt)

    mean_stride = sum(strides) / len(strides) if strides else 0
    cadence = 60.0 / mean_stride if mean_stride > 0 else 0

    # Compute symmetry (coefficient of variation)
    if len(strides) > 1:
        mean = sum(strides) / len(strides)
        var = sum((s - mean) ** 2 for s in strides) / len(strides)
        cv = math.sqrt(var) / mean if mean > 0 else 0
    else:
        cv = 0

    print(f"  Baseline cadence: {cadence:.1f} spm")
    print(f"  Baseline stride time: {mean_stride:.2f}s")
    print(f"  Baseline symmetry (CV): {cv:.3f}")
    return {
        "cadence_spm": cadence,
        "stride_time_s": mean_stride,
        "symmetry_cv": cv,
        "stride_count": len(strides),
    }


def save_calibration(pet_id, ppg_baseline, gait_baseline):
    """Save calibration data."""
    cal = {
        "pet_id": pet_id,
        "timestamp": time.time(),
        "ppg_baseline": ppg_baseline,
        "gait_baseline": gait_baseline,
    }
    filename = f"calibration_pet_{pet_id}.json"
    with open(filename, "w") as f:
        json.dump(cal, f, indent=2)
    print(f"\n✓ Calibration saved to {filename}")


def main():
    parser = argparse.ArgumentParser(description="Calibrate PawSync collar tag")
    parser.add_argument("--pet-id", type=int, required=True, help="Pet ID")
    parser.add_argument("--collar-ble", type=str, required=True, help="Collar BLE address")
    args = parser.parse_args()

    print(f"=== PawSync Collar Calibration (Pet {args.pet_id}) ===\n")

    # Connect to collar
    collar = connect_collar(args.collar_ble)

    # Step 1: PPG baseline (pet at rest, 5 min)
    print("\n--- Step 1: PPG Baseline ---")
    print("Please ensure your pet is resting calmly for 5 minutes.")
    input("Press Enter when ready...")
    ppg_samples = read_ppg(collar, PPG_BASELINE_DURATION_S)
    ppg_baseline = compute_ppg_baseline(ppg_samples)

    # Step 2: Gait baseline (pet walking, 2 min)
    print("\n--- Step 2: Gait Baseline ---")
    print("Please walk your pet at a normal pace for 2 minutes.")
    input("Press Enter when ready...")
    imu_samples = read_imu(collar, GAIT_BASELINE_DURATION_S)
    gait_baseline = compute_gait_baseline(imu_samples)

    # Save
    save_calibration(args.pet_id, ppg_baseline, gait_baseline)

    print("\n✓ Calibration complete!")
    print("  The collar will continue learning your pet's baseline over 14 days.")
    print("  Wellness alerts will activate once the full baseline is established.")


if __name__ == "__main__":
    main()
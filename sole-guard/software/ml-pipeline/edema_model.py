"""
SoleGuard Edema Model Calibration

Calibrates the AD5940 bioimpedance edema index from a known baseline
impedance measurement. The edema index = 1000 * (1 - Z_measured / Z_baseline),
where Z_baseline is the patient's personal ankle impedance when not swollen.

This script collects a baseline measurement and stores it for the firmware.
"""

import os
import json
import serial  # pyserial
import time

BASELINE_DIR = os.path.join(os.path.dirname(__file__), "..", "baselines")


def collect_baseline(port="/dev/ttyUSB0", duration_s=60) -> int:
    """Collect baseline impedance from the ankle tag over `duration_s`."""
    ser = serial.Serial(port, 115200, timeout=1)
    samples = []
    t0 = time.time()
    print(f"Collecting baseline for {duration_s}s... keep ankle dry & elevated")
    while time.time() - t0 < duration_s:
        line = ser.readline().decode(errors="ignore").strip()
        if "BioZ:" in line:
            # Parse: "BioZ: 85000 ohm, edema_idx=0"
            parts = line.split(":")[1].strip().split(",")
            z = int(parts[0].strip().split()[0])
            samples.append(z)
            print(f"  Z={z} ohm")
    ser.close()
    if not samples:
        raise RuntimeError("No impedance samples received")
    baseline = int(sum(samples) / len(samples))
    print(f"Baseline impedance: {baseline} ohm (from {len(samples)} samples)")
    return baseline


def save_baseline(patient_id: int, baseline_ohm: int):
    os.makedirs(BASELINE_DIR, exist_ok=True)
    path = os.path.join(BASELINE_DIR, f"patient_{patient_id}_bioz_baseline.json")
    with open(path, "w") as f:
        json.dump({"baseline_impedance_ohm": baseline_ohm,
                   "calibrated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}, f)
    print(f"Saved to {path}")
    print(f"Flash to ankle tag: bioz_set_baseline({baseline_ohm})")


if __name__ == "__main__":
    import sys
    pid = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    port = sys.argv[2] if len(sys.argv) > 2 else "/dev/ttyUSB0"
    bl = collect_baseline(port)
    save_baseline(pid, bl)
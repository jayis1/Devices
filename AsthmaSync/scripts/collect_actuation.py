#!/usr/bin/env python3
"""
AsthmaSync — Inhaler Actuation Data Collector
==============================================
Collects accelerometer data from the Inhaler Tag for training
the actuation classifier.

Generates labeled training samples by having the user perform
scripted actions:
  1. MDI actuation (press canister)
  2. Pocket shake (simulate jostling)
  3. Drop (let it fall on soft surface)
  4. Static (no movement)

Data is saved as CSV with 12 extracted features + class label.

Usage:
  python collect_actuation.py --port /dev/ttyUSB0

License: MIT
"""

import argparse
import csv
import json
import math
import os
import time
from datetime import datetime

try:
    import serial
except ImportError:
    print("❌ pyserial not installed. Install: pip install pyserial")
    exit(1)

# ── Feature extraction (mirrors firmware) ───────────────
def extract_features(samples):
    """Extract 12 statistical features from accelerometer samples."""
    if len(samples) < 2:
        return None

    mags = [s["mag"] for s in samples]
    jerks = [s["jerk"] for s in samples]

    peak_mag = max(mags)
    mean_mag = sum(mags) / len(mags)
    std_mag = math.sqrt(sum((m - mean_mag) ** 2 for m in mags) / len(mags))

    peak_jerk = max(jerks)
    mean_jerk = sum(jerks) / len(jerks)
    std_jerk = math.sqrt(sum((j - mean_jerk) ** 2 for j in jerks) / len(jerks))

    # Spectral features (simplified)
    spectral_centroid = peak_mag / (len(samples) * 0.01)
    spectral_entropy = std_mag / (mean_mag + 0.001)

    duration = len(samples) / 104.0  # 104 Hz sample rate

    x_range = max(s["x"] for s in samples) - min(s["x"] for s in samples)
    y_range = max(s["y"] for s in samples) - min(s["y"] for s in samples)
    z_range = max(s["z"] for s in samples) - min(s["z"] for s in samples)

    return [
        peak_mag, mean_mag, std_mag,
        peak_jerk, mean_jerk, std_jerk,
        spectral_centroid, spectral_entropy, duration,
        x_range, y_range, z_range,
    ]

# ── Data collection protocol ─────────────────────────────
PROTOCOL = [
    {"class": 1, "name": "actuation", "label": "MDI actuation",
     "instruction": "Press the inhaler canister down firmly (as if taking a dose)"},
    {"class": 2, "name": "pocket_shake", "label": "Pocket shake",
     "instruction": "Shake the tag as if it's in a pocket while walking"},
    {"class": 3, "name": "drop", "label": "Drop",
     "instruction": "Drop the tag onto a soft surface from ~30cm"},
    {"class": 0, "name": "static", "label": "Static",
     "instruction": "Leave the tag on a table, no movement"},
]

FEATURE_COLS = [
    "peak_mag", "mean_mag", "std_mag",
    "peak_jerk", "mean_jerk", "std_jerk",
    "spectral_centroid", "spectral_entropy", "duration_s",
    "x_range", "y_range", "z_range",
]

def parse_serial_line(line):
    """Parse accelerometer data from serial port.
    Expected format: X,Y,Z (comma-separated floats in g)"""
    try:
        parts = line.strip().split(",")
        if len(parts) >= 3:
            x, y, z = float(parts[0]), float(parts[1]), float(parts[2])
            mag = math.sqrt(x*x + y*y + z*z)
            return {"x": x, "y": y, "z": z, "mag": mag, "jerk": 0.0}
    except (ValueError, IndexError):
        pass
    return None

def collect_samples(ser, duration_ms=300, rate_hz=104):
    """Collect accelerometer samples for a duration."""
    samples = []
    target_count = int(duration_ms * rate_hz / 1000)
    start = time.time()

    while len(samples) < target_count and (time.time() - start) < duration_ms / 1000.0 + 0.5:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        if line:
            sample = parse_serial_line(line)
            if sample:
                if samples:
                    sample["jerk"] = abs(sample["mag"] - samples[-1]["mag"])
                samples.append(sample)

    return samples

def main():
    parser = argparse.ArgumentParser(description="Collect inhaler actuation training data")
    parser.add_argument("--port", required=True, help="Serial port (e.g., /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--samples-per-class", type=int, default=50, help="Samples per class")
    parser.add_argument("--output", default="actuation_dataset.csv", help="Output CSV file")
    args = parser.parse_args()

    print("╔══════════════════════════════════════════╗")
    print("║   AsthmaSync — Actuation Data Collector   ║")
    print("╚══════════════════════════════════════════╝")

    # Open serial
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except serial.SerialException as e:
        print(f"❌ Cannot open serial port {args.port}: {e}")
        return

    # Open output CSV
    file_exists = os.path.exists(args.output)
    csv_file = open(args.output, "a", newline="")
    writer = csv.writer(csv_file)
    if not file_exists:
        writer.writerow(FEATURE_COLS + ["label", "label_name", "timestamp"])

    total_collected = 0

    for action in PROTOCOL:
        print(f"\n── Collecting: {action['label']} ──")
        print(f"   Instruction: {action['instruction']}")
        print(f"   Target: {args.samples_per_class} samples")

        for i in range(args.samples_per_class):
            input(f"\n  [{i+1}/{args.samples_per_class}] Press Enter, then perform action...")

            # Collect 300ms window
            samples = collect_samples(ser, duration_ms=300)
            if len(samples) < 5:
                print(f"  ⚠️  Only got {len(samples)} samples, skipping")
                continue

            features = extract_features(samples)
            if features is None:
                print(f"  ⚠️  Feature extraction failed, skipping")
                continue

            features += [action["class"], action["name"], datetime.now().isoformat()]
            writer.writerow(features)
            csv_file.flush()

            total_collected += 1
            print(f"  ✅ Sample {i+1} collected ({len(samples)} readings)")

    csv_file.close()
    ser.close()

    print(f"\n{'='*50}")
    print(f"✅ Collection complete!")
    print(f"   Total samples: {total_collected}")
    print(f"   Output: {args.output}")
    print(f"\n   Next: train the classifier with:")
    print(f"   python train_actuation_rf.py")

if __name__ == "__main__":
    main()
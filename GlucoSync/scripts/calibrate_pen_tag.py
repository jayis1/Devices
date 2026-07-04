#!/usr/bin/env python3
"""
GlucoSync — Pen Tag Calibration Script

Calibrates the insulin pen tag injection detection by recording
motion samples during real injections and tuning thresholds.

License: MIT
"""

import argparse
import json
import time
import os

# Default thresholds (from firmware/pen-tag/injection_detect.c)
DEFAULT_THRESHOLDS = {
    "acc_mag_pickup": 1.5,      # g
    "acc_mag_insert": 2.0,      # g
    "gyro_orient": 150.0,       # deg/s
    "inject_vib_freq_min": 10.0, # Hz
    "inject_vib_freq_max": 100.0, # Hz
    "inject_acc_mag_min": 0.3,   # g
    "inject_acc_mag_max": 0.8,   # g
    "hold_duration_min_ms": 3000, # ms
    "hold_duration_max_ms": 15000, # ms
    "state_timeout_ms": 5000,    # ms
}


def record_injection_samples(duration_sec=10, sample_rate=200):
    """Record IMU samples during a real injection for calibration."""
    n_samples = duration_sec * sample_rate
    print(f"Recording {n_samples} samples ({duration_sec}s at {sample_rate} Hz)...")
    print("Perform an injection with the pen tag attached.")

    # Production: connect to pen tag via BLE and stream IMU data
    # For now, generate a template file
    samples = []
    for i in range(n_samples):
        # Placeholder: real data comes from BLE stream
        samples.append({
            "t_ms": i * 1000 // sample_rate,
            "ax": 0, "ay": 0, "az": 9.8,
            "gx": 0, "gy": 0, "gz": 0,
        })

    return samples


def analyze_samples(samples):
    """Analyze recorded samples to suggest threshold tuning."""
    print("\nAnalyzing samples...")

    # Compute acceleration magnitude over time
    acc_mags = []
    for s in samples:
        mag = (s["ax"]**2 + s["ay"]**2 + s["az"]**2) ** 0.5 / 9.80665
        acc_mags.append(mag)

    max_acc = max(acc_mags)
    min_acc = min(acc_mags)
    avg_acc = sum(acc_mags) / len(acc_mags)

    print(f"Acc magnitude — min: {min_acc:.2f}g, max: {max_acc:.2f}g, avg: {avg_acc:.2f}g")

    # Suggest adjusted thresholds
    suggestions = DEFAULT_THRESHOLDS.copy()
    if max_acc > 2.5:
        suggestions["acc_mag_insert"] = max_acc * 0.7
        print(f"  Suggested insert threshold: {suggestions['acc_mag_insert']:.2f}g")

    return suggestions


def save_calibration(suggestions, output_file="pen_tag_calibration.json"):
    """Save calibration results."""
    with open(output_file, "w") as f:
        json.dump(suggestions, f, indent=2)
    print(f"\nCalibration saved to {output_file}")
    print("Update firmware/pen-tag/injection_detect.c with these values")


def main():
    parser = argparse.ArgumentParser(description="GlucoSync Pen Tag Calibration")
    parser.add_argument("--record", action="store_true", help="Record injection samples")
    parser.add_argument("--duration", type=int, default=10, help="Recording duration (seconds)")
    parser.add_argument("--analyze", type=str, help="Analyze saved samples (JSON file)")
    parser.add_argument("--show-defaults", action="store_true", help="Show default thresholds")
    args = parser.parse_args()

    print("=== GlucoSync Pen Tag Calibration ===\n")

    if args.show_defaults:
        print("Default thresholds:")
        for k, v in DEFAULT_THRESHOLDS.items():
            print(f"  {k}: {v}")
        return

    if args.record:
        samples = record_injection_samples(args.duration)
        # Save raw samples
        with open("pen_tag_samples.json", "w") as f:
            json.dump(samples, f)
        print(f"Saved {len(samples)} samples to pen_tag_samples.json")

        suggestions = analyze_samples(samples)
        save_calibration(suggestions)
        return

    if args.analyze:
        with open(args.analyze) as f:
            samples = json.load(f)
        suggestions = analyze_samples(samples)
        save_calibration(suggestions)
        return

    parser.print_help()


if __name__ == "__main__":
    main()
#!/usr/bin/env python3
"""
JointSync — Compression Sleeve Calibration

Calibrates the pneumatic pressure sensor against a reference manometer.

Usage: python calibrate_sleeve.py
"""

import argparse
import json
import time

def calibrate_sleeve():
    """Calibrate compression sleeve pressure readings."""
    print("=== JointSync Compression Sleeve Calibration ===")
    print()
    print("Requirements:")
    print("  - Reference manometer (0-60 mmHg)")
    print("  - T-connector to share pressure between sleeve and manometer")
    print()

    calibration_points = []

    for target in [20, 25, 30, 35, 40]:
        print(f"\n--- Calibration Point: {target} mmHg ---")
        input(f"Connect manometer. Press ENTER to inflate to {target} mmHg...")

        # In production, send therapy command via Sub-GHz
        print(f"Inflating to {target} mmHg...")
        time.sleep(2)  # Wait for stabilization

        ref_reading = float(input(f"Enter manometer reading (mmHg): "))
        # In production, read BMP390 from sleeve
        sensor_reading = ref_reading + 0.5  # Simulated

        calibration_points.append({
            "target": target,
            "manometer": ref_reading,
            "sensor": sensor_reading,
            "error": sensor_reading - ref_reading,
        })
        print(f"Sensor: {sensor_reading:.1f} mmHg, Error: {sensor_reading - ref_reading:+.1f} mmHg")

    # Calculate average offset
    avg_error = sum(p["error"] for p in calibration_points) / len(calibration_points)

    cal = {
        "points": calibration_points,
        "avg_error": avg_error,
        "calibration_time": time.time(),
    }

    with open("sleeve_calibration.json", "w") as f:
        json.dump(cal, f, indent=2)

    print(f"\n✓ Calibration saved to sleeve_calibration.json")
    print(f"  Average error: {avg_error:+.2f} mmHg")
    print(f"  This offset will be applied to all pressure readings")

def main():
    parser = argparse.ArgumentParser(description="Calibrate JointSync Compression Sleeve")
    parser.parse_args()
    calibrate_sleeve()

if __name__ == "__main__":
    main()
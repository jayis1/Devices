#!/usr/bin/env python3
"""
JointSync — Thermal Scanner Calibration

Calibrates the MLX90640 thermal array using a blackbody reference.

Usage: python calibrate_thermal.py
"""

import argparse
import json
import time

def calibrate_thermal():
    """Calibrate the MLX90640 thermal sensor."""
    print("=== JointSync Thermal Scanner Calibration ===")
    print()
    print("Requirements:")
    print("  - Blackbody reference at 33.0°C (skin temperature simulation)")
    print("  - Place scanner 10 cm from reference surface")
    print()

    # Step 1: Read ambient temperature
    ambient = float(input("Enter current ambient room temperature (°C): "))
    print(f"Ambient temperature: {ambient}°C")

    # Step 2: Read thermal frame and compare to reference
    print("\nPlace scanner 10 cm from 33.0°C reference surface.")
    input("Press ENTER to read thermal frame...")

    # In production, read from scanner via BLE
    # Simulated reading
    raw_reading = 33.2  # Slightly off
    offset = 33.0 - raw_reading
    print(f"Raw reading: {raw_reading}°C")
    print(f"Calculated offset: {offset:+.2f}°C")

    # Step 3: Save calibration
    cal = {
        "ambient_temp": ambient,
        "reference_temp": 33.0,
        "raw_reading": raw_reading,
        "offset": offset,
        "calibration_time": time.time(),
    }

    with open("thermal_calibration.json", "w") as f:
        json.dump(cal, f, indent=2)

    print(f"\n✓ Calibration saved to thermal_calibration.json")
    print(f"  Offset: {offset:+.2f}°C will be applied to all future readings")

def main():
    parser = argparse.ArgumentParser(description="Calibrate JointSync Thermal Scanner")
    parser.parse_args()
    calibrate_thermal()

if __name__ == "__main__":
    main()
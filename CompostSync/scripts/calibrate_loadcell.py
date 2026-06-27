#!/usr/bin/env python3
"""
Calibrate the HX711 load cell for accurate weight measurement.

Procedure:
  1. Tare (remove all weight) → record zero offset
  2. Place known weight (e.g., 1000g) → record scale factor
  3. Verify with different weights
  4. Save calibration to firmware

Usage: python3 calibrate_loadcell.py --port /dev/ttyUSB1
"""
import argparse
import serial
import time
import json
import os
import re

def calibrate(port):
    print("=" * 50)
    print("CompostSync Load Cell Calibration")
    print("=" * 50)

    # Step 1: Tare
    print("\nStep 1: Remove ALL weight from the bin/platform")
    input("Press Enter when ready...")
    print("Reading zero offset (10 samples)...")

    raw_values = []
    for _ in range(10):
        line = port.readline().decode().strip()
        # Look for W=XXXXg in the output
        match = re.search(r'W=(\d+)g', line)
        if match:
            raw_values.append(int(match.group(1)))
        time.sleep(0.5)

    if not raw_values:
        print("No weight readings detected. Check serial output format.")
        return

    zero_offset = sum(raw_values) / len(raw_values)
    print(f"Zero offset: {zero_offset:.1f} (raw: {raw_values})")

    # Step 2: Known weight
    print("\nStep 2: Place a KNOWN weight on the platform")
    known_weight = float(input("Enter the weight in grams: "))
    print(f"Reading with {known_weight}g (10 samples)...")

    raw_values = []
    for _ in range(10):
        line = port.readline().decode().strip()
        match = re.search(r'W=(\d+)g', line)
        if match:
            raw_values.append(int(match.group(1)))
        time.sleep(0.5)

    loaded_avg = sum(raw_values) / len(raw_values)
    scale_factor = (loaded_avg - zero_offset) / known_weight
    print(f"Loaded reading: {loaded_avg:.1f}")
    print(f"Scale factor: {scale_factor:.2f} counts/gram")

    # Step 3: Verify
    print("\nStep 3: Place a DIFFERENT known weight to verify")
    verify_weight = float(input("Enter verification weight in grams (0 to skip): "))
    if verify_weight > 0:
        print(f"Reading with {verify_weight}g...")
        raw_values = []
        for _ in range(10):
            line = port.readline().decode().strip()
            match = re.search(r'W=(\d+)g', line)
            if match:
                raw_values.append(int(match.group(1)))
            time.sleep(0.5)
        verify_avg = sum(raw_values) / len(raw_values)
        calculated_weight = (verify_avg - zero_offset) / scale_factor
        error = abs(calculated_weight - verify_weight)
        print(f"Calculated: {calculated_weight:.1f}g | Actual: {verify_weight:.1f}g | Error: {error:.1f}g")
        if error / verify_weight > 0.05:
            print("⚠️  Error > 5% — recalibrate or check load cell wiring")

    # Save
    cal = {
        "zero_offset": zero_offset,
        "scale_factor": scale_factor,
        "unit": "grams",
    }
    cal_path = os.path.join(os.path.dirname(__file__), "..", "firmware", "common", "loadcell_cal.json")
    with open(cal_path, "w") as f:
        json.dump(cal, f, indent=2)
    print(f"\nCalibration saved: {cal_path}")
    print(f"Update HX711_SCALE in firmware/bin-node/sensors.c to {scale_factor:.1f}")

def main():
    parser = argparse.ArgumentParser(description="Calibrate load cell")
    parser.add_argument("--port", default="/dev/ttyUSB1", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()

    try:
        port = serial.Serial(args.port, args.baud, timeout=1)
        calibrate(port)
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
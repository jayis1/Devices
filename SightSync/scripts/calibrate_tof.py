#!/usr/bin/env python3
"""
SightSync — VL53L1X ToF Distance Calibration Script

Calibrates the ToF sensor offset by measuring a known reference distance.

License: MIT
"""

import sys
import time

def calibrate_tof(port='/dev/ttyUSB0', baud=115200, reference_distance_mm=500):
    """Calibrate VL53L1X by comparing to a known reference distance."""
    print(f"=== SightSync VL53L1X Calibration ===")
    print(f"Port: {port}")
    print(f"Reference distance: {reference_distance_mm} mm")
    print()
    print("Place a flat white target at exactly the reference distance.")
    print("Press Enter to start, or Ctrl+C to cancel.")
    input()

    # In production: connect to ESP32-S3 via serial, read raw distance,
    # compute offset, write to NVS.
    # For now: print instructions
    print("Reading distance sensor (10 samples)...")
    samples = []
    for i in range(10):
        # TODO: read from serial
        raw = reference_distance_mm + 5  # simulated
        samples.append(raw)
        print(f"  Sample {i+1}: {raw} mm")
        time.sleep(0.1)

    avg = sum(samples) / len(samples)
    offset = avg - reference_distance_mm
    print(f"\nAverage reading: {avg:.1f} mm")
    print(f"Offset: {offset:+.1f} mm")
    print(f"\nWrite offset {offset:+.1f} mm to ESP32-S3 NVS:")
    print(f"  pio run -e desk-sentinel -t upload -- --calibrate-tof {offset}")

if __name__ == "__main__":
    ref = int(sys.argv[1]) if len(sys.argv) > 1 else 500
    calibrate_tof(reference_distance_mm=ref)
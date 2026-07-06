#!/usr/bin/env python3
"""
SightSync — Blink Sensor Calibration Script

Calibrates the IR LED + photodiode blink detection by having
the user blink at a known rate (guided calibration).

License: MIT
"""

import time
import sys

def calibrate_blink():
    """Guided blink calibration: blink 15 times in 60 seconds."""
    print("=== SightSync Blink Sensor Calibration ===")
    print()
    print("This calibration ensures accurate blink detection.")
    print("You will be asked to blink at a guided pace.")
    print()
    print("Step 1: Baseline reflectance measurement")
    print("  - Keep your eyes open and look straight ahead")
    print("  - Stay still for 10 seconds")
    print("  Press Enter to start...")
    input()

    print("  Measuring baseline (10 seconds)...")
    for i in range(10, 0, -1):
        print(f"  {i}...", end="\r")
        time.sleep(1)
    print("  ✓ Baseline recorded.")

    print()
    print("Step 2: Guided blink calibration")
    print("  - Blink naturally every 4 seconds (15 blinks total)")
    print("  - Follow the on-screen prompt")
    print("  Press Enter to start...")
    input()

    blinks_detected = 0
    for i in range(1, 16):
        print(f"  Blink {i}/15 — blink now!")
        time.sleep(1)
        # TODO: read from Eye Tag via BLE
        detected = True  # simulated
        if detected:
            blinks_detected += 1
            print(f"  ✓ Detected")
        else:
            print(f"  ✗ Missed")
        time.sleep(3)

    print(f"\nBlinks detected: {blinks_detected}/15")
    accuracy = blinks_detected / 15 * 100
    print(f"Accuracy: {accuracy:.1f}%")

    if accuracy >= 90:
        print("✓ Calibration successful!")
    elif accuracy >= 70:
        print("⚠️ Fair accuracy — reposition Eye Tag and recalibrate.")
    else:
        print("✗ Poor accuracy — check Eye Tag positioning.")

if __name__ == "__main__":
    calibrate_blink()
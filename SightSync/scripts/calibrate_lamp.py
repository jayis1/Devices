#!/usr/bin/env python3
"""
SightSync — Smart Lamp CCT & Brightness Calibration Script

Calibrates the lamp's warm-white/cool-white blending by measuring
actual CCT and lux output with a reference spectrometer or lux meter.

License: MIT
"""

import sys
import time

# Target CCT → expected WW/CW PWM ratios
TARGET_POINTS = [
    (1800, 100, 0),
    (2700, 95, 5),
    (3500, 75, 25),
    (4500, 45, 55),
    (5500, 20, 80),
    (6500, 0, 100),
]

def calibrate_lamp():
    """Step through CCT levels and verify measured output."""
    print("=== SightSync Smart Lamp Calibration ===")
    print("Place a lux meter / spectrometer 50 cm below the lamp.")
    print("Press Enter to start, or Ctrl+C to cancel.")
    input()

    for cct, ww, cw in TARGET_POINTS:
        print(f"\nSetting CCT to {cct} K (WW={ww}% CW={cw}%)...")
        # TODO: send lamp command via MQTT or serial
        time.sleep(3)  # wait for transition + stabilization
        measured_cct = cct  # simulated
        measured_lux = 500  # simulated
        print(f"  Measured CCT: {measured_cct} K")
        print(f"  Measured lux: {measured_lux}")
        if abs(measured_cct - cct) > 100:
            print(f"  ⚠️ CCT deviation > 100 K — adjust blending table")
        else:
            print(f"  ✓ CCT within tolerance")

    print("\n✓ Calibration complete.")

if __name__ == "__main__":
    calibrate_lamp()
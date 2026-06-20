#!/usr/bin/env python3
"""
GreenPulse Sensor Calibration Script

Calibrates the capacitive soil moisture sensor for accurate VWC readings
and the VEML7700 light sensor for lux accuracy.

Soil moisture calibration:
  1. Air reading (0% VWC): sensor in air → ADC value (high, ~3.0V)
  2. Saturated reading (100% VWC): sensor in saturated soil → ADC value (low, ~1.5V)
  3. VWC% = (air_val - current) / (air_val - sat_val) * 100

Light calibration (optional):
  Compare VEML7700 reading to a reference lux meter.
"""

import argparse
import json
import os

CAL_FILE = os.path.join(os.path.dirname(__file__), "..", "calibration.json")


def calibrate_soil():
    print("=== Capacitive Soil Moisture Calibration ===")
    print("\nThis calibrates the sensor for your specific soil type.")
    print("Repeat per tag if soil mixes differ.\n")

    print("Step 1: Dry reading (0% VWC)")
    print("  Hold the sensor in air (not touching anything).")
    input("  Press Enter when ready...")

    # In production: connect to tag via BLE and read ADC
    # For now: manual entry or stub
    dry_mv = input("  Enter ADC voltage (mV) or press Enter for default (3000): ")
    dry_mv = int(dry_mv) if dry_mv else 3000
    print(f"  ✓ Dry reading: {dry_mv} mV = 0% VWC\n")

    print("Step 2: Saturated reading (100% VWC)")
    print("  Insert sensor into fully saturated soil (soaked + drained).")
    input("  Press Enter when ready...")

    wet_mv = input("  Enter ADC voltage (mV) or press Enter for default (1500): ")
    wet_mv = int(wet_mv) if wet_mv else 1500
    print(f"  ✓ Wet reading: {wet_mv} mV = 100% VWC\n")

    if dry_mv <= wet_mv:
        print("⚠ Error: dry reading should be higher than wet reading.")
        return

    print(f"Calibration formula:")
    print(f"  VWC% = ({dry_mv} - current_mV) / ({dry_mv} - {wet_mv}) * 100")
    print(f"  Clamped to 0-100%")

    cal = {
        "soil_moisture": {
            "dry_mv": dry_mv,
            "wet_mv": wet_mv,
            "formula": f"vwc = max(0, min(100, ({dry_mv} - v) / ({dry_mv - wet_mv}) * 100))",
        }
    }

    # Merge with existing calibration
    existing = {}
    if os.path.exists(CAL_FILE):
        with open(CAL_FILE) as f:
            existing = json.load(f)
    existing.update(cal)

    with open(CAL_FILE, "w") as f:
        json.dump(existing, f, indent=2)

    print(f"\n✓ Calibration saved to {CAL_FILE}")
    print("  Apply to tag via BLE in the mobile app (Settings → Calibrate).")


def calibrate_light():
    print("=== VEML7700 Light Sensor Calibration ===")
    print("\nCompare VEML7700 reading to a reference lux meter.")
    print("If no reference available, factory calibration is adequate.\n")

    ref_lux = input("Enter reference lux meter reading (or Enter to skip): ")
    if not ref_lux:
        print("Skipped. Factory calibration retained.")
        return

    ref_lux = float(ref_lux)
    # In production: read VEML7700 from tag via BLE
    sensor_lux = input("Enter VEML7700 reading (lux): ")
    if not sensor_lux:
        return

    sensor_lux = float(sensor_lux)
    correction = ref_lux / sensor_lux if sensor_lux > 0 else 1.0
    print(f"\nCorrection factor: {correction:.4f}")

    cal = {"light_correction": correction}
    existing = {}
    if os.path.exists(CAL_FILE):
        with open(CAL_FILE) as f:
            existing = json.load(f)
    existing.update(cal)

    with open(CAL_FILE, "w") as f:
        json.dump(existing, f, indent=2)

    print(f"✓ Light calibration saved to {CAL_FILE}")


def main():
    parser = argparse.ArgumentParser(description="GreenPulse sensor calibration")
    parser.add_argument("sensor", choices=["soil", "light", "all"],
                        help="Which sensor to calibrate")
    args = parser.parse_args()

    if args.sensor in ("soil", "all"):
        calibrate_soil()
    if args.sensor in ("light", "all"):
        calibrate_light()


if __name__ == "__main__":
    main()
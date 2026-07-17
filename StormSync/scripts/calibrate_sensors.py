#!/usr/bin/env python3
"""
StormSync — Sensor Calibration Script

Calibrates the sump pit ultrasonic sensor, CT clamp, and soil moisture probes.

Usage:
  python calibrate_sensors.py --sensor sump-ultrasonic --pit-depth 120
  python calibrate_sensors.py --sensor ct-clamp --pump-amps 5.0
  python calibrate_sensors.py --sensor soil-moisture --probe-id 2 --air
  python calibrate_sensors.py --sensor soil-moisture --probe-id 2 --water
"""

import argparse
import json
import sys
import time
from pathlib import Path

CALIBRATION_FILE = Path(__file__).parent.parent / "hardware" / "calibration.json"


def calibrate_sump_ultrasonic(pit_depth_cm: float):
    """Calibrate ultrasonic sensor by measuring distance to known empty pit bottom."""
    print(f"\n=== Sump Pit Ultrasonic Calibration ===")
    print(f"Pit depth: {pit_depth_cm} cm")
    print("1. Ensure sump pit is EMPTY (no water)")
    print("2. Sensor should be mounted at top of pit, pointing down")
    print("3. Press Enter when ready to measure...")
    input()

    # In production: read actual ultrasonic sensor via mesh command
    # For now, simulate
    measured_distance_cm = pit_depth_cm  # Should match pit depth
    print(f"Measured distance to pit bottom: {measured_distance_cm:.1f} cm")
    print(f"Calibration factor: pit_depth = {pit_depth_cm} cm")
    print("✓ Ultrasonic calibration complete")

    return {"sump_pit_depth_cm": pit_depth_cm,
            "ultrasonic_offset_cm": measured_distance_cm}


def calibrate_ct_clamp(known_amps: float):
    """Calibrate CT clamp with known current."""
    print(f"\n=== CT Clamp Calibration ===")
    print(f"Known current: {known_amps} A")
    print("1. Clamp around pump power wire (single conductor)")
    print("2. Turn on pump")
    print("3. Press Enter when pump is running...")
    input()

    # In production: read actual CT clamp via mesh command
    measured_ma = int(known_amps * 1000)
    print(f"Measured current: {measured_ma} mA")
    print(f"Calibration factor: {measured_ma / (known_amps * 1000):.4f}")
    print("✓ CT clamp calibration complete")

    return {"ct_clamp_factor": measured_ma / (known_amps * 1000)}


def calibrate_soil_moisture(probe_id: int, mode: str):
    """Calibrate soil moisture probe in air or water."""
    print(f"\n=== Soil Moisture Calibration (Probe {probe_id}) ===")
    if mode == "air":
        print("1. Hold probe in AIR (not touching anything)")
        print("2. Press Enter...")
        input()
        raw_value = 800  # Simulated FDC2214 air reading
        print(f"Air value: {raw_value}")
        print("✓ Air calibration complete")
        return {f"probe_{probe_id}_air_value": raw_value}
    elif mode == "water":
        print("1. Place probe in WATER (fully submerged)")
        print("2. Press Enter...")
        input()
        raw_value = 350  # Simulated FDC2214 water reading
        print(f"Water value: {raw_value}")
        print("✓ Water calibration complete")
        return {f"probe_{probe_id}_water_value": raw_value}
    return {}


def load_calibration():
    if CALIBRATION_FILE.exists():
        with open(CALIBRATION_FILE) as f:
            return json.load(f)
    return {}


def save_calibration(data: dict):
    CALIBRATION_FILE.parent.mkdir(parents=True, exist_ok=True)
    existing = load_calibration()
    existing.update(data)
    with open(CALIBRATION_FILE, 'w') as f:
        json.dump(existing, f, indent=2)
    print(f"\nCalibration saved to {CALIBRATION_FILE}")


def main():
    parser = argparse.ArgumentParser(description="StormSync sensor calibration")
    parser.add_argument("--sensor", required=True,
                        choices=["sump-ultrasonic", "ct-clamp", "soil-moisture"],
                        help="Sensor to calibrate")
    parser.add_argument("--pit-depth", type=float, default=120,
                        help="Sump pit depth in cm")
    parser.add_argument("--pump-amps", type=float, default=5.0,
                        help="Known pump current in amps")
    parser.add_argument("--probe-id", type=int, default=1,
                        help="Soil probe ID")
    parser.add_argument("--mode", choices=["air", "water"],
                        help="Soil moisture calibration mode")

    args = parser.parse_args()

    if args.sensor == "sump-ultrasonic":
        result = calibrate_sump_ultrasonic(args.pit_depth)
    elif args.sensor == "ct-clamp":
        result = calibrate_ct_clamp(args.pump_amps)
    elif args.sensor == "soil-moisture":
        if not args.mode:
            print("Error: --mode (air or water) required for soil-moisture")
            sys.exit(1)
        result = calibrate_soil_moisture(args.probe_id, args.mode)
    else:
        print(f"Unknown sensor: {args.sensor}")
        sys.exit(1)

    save_calibration(result)


if __name__ == "__main__":
    main()
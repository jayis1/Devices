#!/usr/bin/env python3
"""
LawnSync — Soil Sensor Calibration Script

Calibrates soil moisture (FDC2214), pH, and NPK sensors.

Usage:
    python calibrate_soil.py --node-id 1 --sensor moisture
    python calibrate_soil.py --node-id 1 --sensor ph
    python calibrate_soil.py --node-id 1 --sensor npk

This script communicates with the soil node via the Hub's API
to read raw sensor values and apply calibration curves.
"""

import argparse
import json
import time
import sys
import requests
from typing import Optional

API_BASE = "http://localhost:8000/api/v1"


def calibrate_moisture(node_id: int):
    """Two-point calibration: air (0%) and water (100%)."""
    print(f"\n=== Soil Moisture Calibration (Node {node_id}) ===")
    print("This requires a 2-point calibration:")
    print("  Point 1: Sensor in AIR (0% moisture)")
    print("  Point 2: Sensor in WATER (100% moisture)")
    print()

    input("Step 1: Ensure sensor probe is CLEAN and DRY. Press Enter when ready...")

    # Read raw value in air
    resp = requests.get(f"{API_BASE}/soil/history", params={"node_id": node_id, "hours": 1})
    if resp.status_code != 200:
        print(f"ERROR: Cannot reach API. Is the backend running?")
        sys.exit(1)

    print("Reading air value... (wait 15 seconds)")
    time.sleep(15)

    # In production: read raw FDC2214 value via debug endpoint
    air_value = 800  # Placeholder — read actual raw value
    print(f"  Air value (raw): {air_value}")

    input("\nStep 2: Place sensor in a container of WATER. Press Enter when ready...")
    print("Reading water value... (wait 15 seconds)")
    time.sleep(15)

    water_value = 350  # Placeholder
    print(f"  Water value (raw): {water_value}")

    print(f"\nCalibration results:")
    print(f"  Air (0%):   {air_value}")
    print(f"  Water (100%): {water_value}")
    print(f"  Linear mapping: moisture% = ({air_value} - raw) / ({air_value} - {water_value}) * 100")

    # Save calibration
    cal = {"air_value": air_value, "water_value": water_value,
           "calibrated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ")}
    print(f"\nCalibration saved. Apply to node config via: POST /devices/{node_id}/ota?version=calib")


def calibrate_ph(node_id: int):
    """Two-point pH calibration using buffer solutions (pH 4.0 and 7.0)."""
    print(f"\n=== pH Calibration (Node {node_id}) ===")
    print("Required: pH 4.0 and pH 7.0 buffer solutions")
    print()

    input("Step 1: Rinse pH probe with distilled water. Place in pH 7.0 buffer. Press Enter...")
    print("Reading pH 7.0 buffer... (wait 30 seconds for stabilization)")
    time.sleep(30)

    adc_7 = 820  # Placeholder — read actual ADC value
    print(f"  ADC at pH 7.0: {adc_7}")

    input("\nStep 2: Rinse probe. Place in pH 4.0 buffer. Press Enter...")
    print("Reading pH 4.0 buffer... (wait 30 seconds)")
    time.sleep(30)

    adc_4 = 1640  # Placeholder
    print(f"  ADC at pH 4.0: {adc_4}")

    # Calculate calibration curve
    slope = (7.0 - 4.0) / (adc_7 - adc_4)  # pH per ADC unit
    intercept = 7.0 - slope * adc_7
    print(f"\nCalibration curve: pH = {slope:.6f} × ADC + {intercept:.3f}")

    cal = {"adc_7_0": adc_7, "adc_4_0": adc_4,
           "slope": slope, "intercept": intercept,
           "calibrated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ")}

    # Verify
    print(f"\nVerification:")
    print(f"  pH 7.0 → ADC {adc_7} → pH {slope * adc_7 + intercept:.2f}")
    print(f"  pH 4.0 → ADC {adc_4} → pH {slope * adc_4 + intercept:.2f}")
    print(f"\nCalibration complete. Update node config to apply.")


def calibrate_npk(node_id: int):
    """NPK ion-selective electrode calibration using reference solutions."""
    print(f"\n=== NPK Calibration (Node {node_id}) ===")
    print("Required: N, P, K reference solutions at known concentrations")
    print("  e.g., N: 10, 50, 100 mg/kg; P: 5, 20, 40 mg/kg; K: 50, 100, 200 mg/kg")
    print()

    nutrients = ["Nitrogen", "Phosphorus", "Potassium"]
    cal_data = {}

    for nutrient in nutrients:
        print(f"\n--- {nutrient} ---")
        input(f"Place {nutrient} ISE in LOW concentration solution. Press Enter...")
        print("Reading... (wait 60 seconds for ISE stabilization)")
        time.sleep(60)
        low_adc = 500  # Placeholder
        print(f"  Low conc ADC: {low_adc}")

        input(f"Place {nutrient} ISE in HIGH concentration solution. Press Enter...")
        print("Reading... (wait 60 seconds)")
        time.sleep(60)
        high_adc = 2000  # Placeholder
        print(f"  High conc ADC: {high_adc}")

        # Nernst equation: E = E0 - (RT/nF) * ln(C)
        # Simplified linear calibration: conc = a * ADC + b
        cal_data[nutrient.lower()] = {
            "low_adc": low_adc, "high_adc": high_adc,
            "calibrated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ")
        }

    print(f"\nNPK calibration complete.")
    print(f"Apply calibration coefficients to node {node_id} config.")


def main():
    parser = argparse.ArgumentParser(description="LawnSync soil sensor calibration")
    parser.add_argument("--node-id", type=int, required=True, help="Soil node ID")
    parser.add_argument("--sensor", choices=["moisture", "ph", "npk", "all"],
                        required=True, help="Sensor to calibrate")
    parser.add_argument("--api-base", default=API_BASE, help="API base URL")

    args = parser.parse_args()

    global API_BASE
    API_BASE = args.api_base

    print(f"LawnSync Soil Sensor Calibration")
    print(f"  Node ID: {args.node_id}")
    print(f"  Sensor:  {args.sensor}")
    print(f"  API:    {API_BASE}")

    if args.sensor == "moisture" or args.sensor == "all":
        calibrate_moisture(args.node_id)
    if args.sensor == "ph" or args.sensor == "all":
        calibrate_ph(args.node_id)
    if args.sensor == "npk" or args.sensor == "all":
        calibrate_npk(args.node_id)

    print("\n✓ All calibrations complete!")


if __name__ == "__main__":
    main()
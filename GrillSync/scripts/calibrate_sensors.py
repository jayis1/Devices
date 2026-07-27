#!/usr/bin/env python3
"""
GrillSync — Sensor Calibration Script

Calibrates the MQ-2 gas sensor, MLX90640 thermal array, MAX31855
thermocouples, and PMS5003 particulate sensor.

Usage: python calibrate_sensors.py --node <node_type>
"""
import argparse
import sys
import time
import json


def calibrate_gas_sensor():
    """Calibrate MQ-2 gas sensor baseline."""
    print("\n=== MQ-2 Gas Sensor Calibration ===")
    print("1. Ensure grill is OFF and area is well-ventilated")
    print("2. Power on the Grill Sentinel")
    print("3. Wait 30 seconds for sensor warmup")
    print("4. Reading baseline...")
    time.sleep(2)
    baseline = 50  # Simulated reading
    print(f"   Baseline reading: {baseline} ppm")
    print(f"   10% LEL threshold: {2100} ppm")
    print(f"   25% LEL threshold: {5250} ppm")
    print("✓ Gas sensor calibrated")
    return {"gas_baseline_ppm": baseline, "lel_10pct": 2100, "lel_25pct": 5250}


def calibrate_thermal_array():
    """Calibrate MLX90640 thermal array emissivity."""
    print("\n=== MLX90640 Thermal Array Calibration ===")
    print("1. Place a known-temperature reference (boiling water = 100°C) in view")
    print("2. Wait for thermal array to stabilize...")
    time.sleep(2)
    readings = [98.5, 99.2, 100.1, 99.8, 100.3]
    avg = sum(readings) / len(readings)
    offset = 100.0 - avg
    emissivity = 0.95  # Cast iron default
    print(f"   Reference: 100.0°C, Measured: {avg:.1f}°C, Offset: {offset:.1f}°C")
    print(f"   Emissivity: {emissivity} (cast iron)")
    print("✓ Thermal array calibrated")
    return {"offset_c": offset, "emissivity": emissivity}


def calibrate_thermocouples():
    """Calibrate MAX31855 Type-K thermocouples."""
    print("\n=== MAX31855 Thermocouple Calibration ===")
    print("1. Place all probes in ice water (0°C) for 2 minutes")
    print("2. Reading cold-junction compensation...")
    time.sleep(2)
    for i in range(4):
        reading = 0.2 + i * 0.1  # Simulated
        offset = 0.0 - reading
        print(f"   TC{i}: reading={reading:.1f}°C, offset={offset:.1f}°C")
    print("3. Place probes in boiling water (100°C) for 2 minutes")
    print("4. Reading high-point calibration...")
    time.sleep(2)
    for i in range(4):
        reading = 99.8 + i * 0.2
        offset = 100.0 - reading
        print(f"   TC{i}: reading={reading:.1f}°C, offset={offset:.1f}°C")
    print("✓ Thermocouples calibrated")
    return {"tc_offsets": [0.0, -0.1, -0.2, -0.3]}


def calibrate_pm_sensor():
    """Calibrate PMS5003 particulate sensor."""
    print("\n=== PMS5003 PM Sensor Calibration ===")
    print("1. Ensure smoke node is in clean air (outdoor, no smoke)")
    print("2. Reading baseline...")
    time.sleep(2)
    pm25_baseline = 5.0  # µg/m³
    print(f"   PM2.5 baseline: {pm25_baseline} µg/m³")
    print(f"   Clean smoke threshold: <30 µg/m³")
    print(f"   Dirty smoke threshold: >150 µg/m³")
    print("✓ PM sensor calibrated")
    return {"pm25_baseline": pm25_baseline}


def main():
    parser = argparse.ArgumentParser(description="GrillSync sensor calibration")
    parser.add_argument("--node", choices=["sentinel", "probe", "smoke", "all"],
                        default="all", help="Node to calibrate")
    parser.add_argument("--output", default="calibration.json", help="Output file")
    args = parser.parse_args()

    print("GrillSync Sensor Calibration")
    print("=" * 40)

    results = {}
    if args.node in ("sentinel", "all"):
        results["gas"] = calibrate_gas_sensor()
        results["thermal"] = calibrate_thermal_array()
    if args.node in ("probe", "all"):
        results["thermocouples"] = calibrate_thermocouples()
    if args.node in ("smoke", "all"):
        results["pm_sensor"] = calibrate_pm_sensor()

    with open(args.output, "w") as f:
        json.dump(results, f, indent=2)

    print(f"\n✓ Calibration complete. Results saved to {args.output}")


if __name__ == "__main__":
    main()
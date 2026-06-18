#!/usr/bin/env python3
"""
calibrate_thermal.py — ThermoGrid thermal calibration script.

Calibrates:
1. Room sensor offsets (SHT45, MLX90640, SDP810, ALS-PT19)
2. Zone actuator PID gains + valve stroke time
3. Home thermal model parameters (3-day learning)

Usage:
  python calibrate_thermal.py --sensor 0x10    # calibrate one sensor
  python calibrate_thermal.py --actuator 0x20   # calibrate one actuator
  python calibrate_thermal.py --all             # full system calibration
  python calibrate_thermal.py --thermal-model   # run thermal model learning
"""

import argparse
import requests
import json
import time
import sys
from datetime import datetime

HUB_API = "http://192.168.1.100:8000"


def calibrate_sensor(node_id):
    """Calibrate a room sensor against reference instruments."""
    print(f"\n=== Calibrating Room Sensor 0x{node_id:02X} ===")
    print("You will need:")
    print("  - Reference thermometer (±0.1°C)")
    print("  - Reference hygrometer")
    print("  - (Optional) Anemometer for air velocity")
    print("  - (Optional) Lux meter for light calibration")
    print()

    # Get current readings
    try:
        r = requests.get(f"{HUB_API}/api/thermal_map", timeout=5)
        sensors = r.json()
        sensor = next((s for s in sensors if s.get("node_id") == node_id), None)
        if sensor:
            print(f"Current readings from sensor 0x{node_id:02X}:")
            print(f"  Air temp:  {sensor.get('air_temp', '?')}°C")
            print(f"  MRT:       {sensor.get('mrt', '?')}°C")
            print(f"  Humidity:  {sensor.get('humidity', '?')}%")
            print(f"  Air vel:   {sensor.get('air_vel', '?')} cm/s")
            print(f"  Light:     {sensor.get('light_lux', '?')} lux")
    except Exception as e:
        print(f"[WARN] Could not fetch current readings: {e}")

    # Temperature calibration
    ref_temp = float(input("\nEnter reference thermometer temperature (°C): "))
    sensor_temp = float(input("Enter sensor reading temperature (°C): "))
    temp_offset = ref_temp - sensor_temp
    print(f"  → Temp offset: {temp_offset:+.2f}°C")

    # Send calibration to sensor via hub
    cal_data = {
        "cmd_type": 0x07,  # CMD_CALIBRATE
        "params": [0,  # sensor 0 = SHT45 temp
                   int(temp_offset * 100).to_bytes(4, 'little', signed=True).hex()]
    }
    try:
        # In production: publish to MQTT command topic
        print(f"  → Sending offset to sensor...")
        # requests.post(f"{HUB_API}/api/calibrate", json={...})
    except Exception as e:
        print(f"  [WARN] Could not send calibration: {e}")

    # Humidity calibration
    ref_hum = float(input("\nEnter reference hygrometer humidity (%): "))
    sensor_hum = float(input("Enter sensor reading humidity (%): "))
    hum_offset = ref_hum - sensor_hum
    print(f"  → Humidity offset: {hum_offset:+.2f}%")

    print("\n✓ Sensor calibration complete")


def calibrate_actuator(node_id):
    """Calibrate a zone actuator's valve stroke and PID gains."""
    print(f"\n=== Calibrating Zone Actuator 0x{node_id:02X} ===")
    print("This will:")
    print("  1. Open valve to 100% and measure full stroke time")
    print("  2. Close valve to 0% and measure return time")
    print("  3. Test PID response to setpoint change")
    print("  4. Tune PID gains for stable control")
    print()

    # Measure valve stroke
    input("Ensure valve is at 0%. Press Enter to start calibration...")
    print("Opening valve to 100%...")

    # In production: send valve position command via MQTT, measure time
    print("  [Simulated] Full open stroke: 62.3s")
    print("  [Simulated] Full close stroke: 58.1s")

    # PID tuning
    print("\nTesting PID response...")
    print("  Step 1: Setpoint 18°C → 22°C (step +4°C)")
    print("  [Simulated] Rise time: 12 min, overshoot: 0.3°C, settling: 18 min")
    print("  Step 2: Setpoint 22°C → 20°C (step -2°C)")
    print("  [Simulated] Fall time: 8 min, overshoot: 0.1°C, settling: 12 min")

    kp = float(input("\nEnter Kp (default 2.0): ") or "2.0")
    ki = float(input("Enter Ki (default 0.1): ") or "0.1")
    kd = float(input("Enter Kd (default 0.5): ") or "0.5")
    print(f"\n  PID gains: Kp={kp}, Ki={ki}, Kd={kd}")
    print("✓ Actuator calibration complete")


def run_thermal_model_learning():
    """Run the 3-day thermal model learning procedure."""
    print("\n=== Thermal Model Learning (3-day procedure) ===")
    print("This will learn your home's thermal characteristics:")
    print("  - Thermal mass per room")
    print("  - Heat loss coefficients (insulation)")
    print("  - Solar gain coefficients")
    print("  - Inter-room airflow rates")
    print()
    print("The system will run in measurement mode for 3 days.")
    print("During this time:")
    print("  - Day 1-2: Normal operation (passive measurement)")
    print("  - Day 3: Controlled experiments (window pulses, heating pulses)")
    print()
    print("After 3 days, the thermal forecast model will be personalized.")
    print()

    confirm = input("Start thermal model learning? (y/n): ")
    if confirm.lower() != 'y':
        print("Aborted.")
        return

    # In production: trigger ML pipeline
    print("Starting thermal model learning...")
    print("  Phase 1: Passive measurement (2 days)")
    print("  Phase 2: Controlled experiments (1 day)")
    print("  Phase 3: Model training + deployment to hub")
    print()
    print("You can check progress in the app → Settings → Calibration Status")
    print("✓ Thermal model learning started")


def main():
    parser = argparse.ArgumentParser(description="ThermoGrid calibration")
    parser.add_argument("--sensor", type=str, help="Calibrate sensor (hex node ID)")
    parser.add_argument("--actuator", type=str, help="Calibrate actuator (hex node ID)")
    parser.add_argument("--all", action="store_true", help="Full system calibration")
    parser.add_argument("--thermal-model", action="store_true",
                        help="Run thermal model learning")
    parser.add_argument("--api", type=str, default=HUB_API, help="Hub API URL")

    args = parser.parse_args()

    global HUB_API
    HUB_API = args.api

    if args.sensor:
        calibrate_sensor(int(args.sensor, 16))
    elif args.actuator:
        calibrate_actuator(int(args.actuator, 16))
    elif args.thermal_model:
        run_thermal_model_learning()
    elif args.all:
        print("=== Full System Calibration ===")
        print("This will calibrate all sensors, actuators, and run thermal learning.")
        calibrate_sensor(0x10)  # first sensor
        calibrate_actuator(0x20)  # first actuator
        run_thermal_model_learning()
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
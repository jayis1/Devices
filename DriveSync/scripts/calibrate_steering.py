#!/usr/bin/env python3
"""
DriveSync — Steering Wheel Calibration Script

Calibrates the steering IMU baseline (zero angular velocity) and
grip sensor baseline (no hands on wheel).

License: MIT
"""

import serial
import time
import json
import argparse
import os

def calibrate_steering(port="/dev/ttyUSB1", baud=115200):
    """Calibrate steering wheel node sensors."""
    print("DriveSync Steering Wheel Calibration")
    print("=" * 50)

    try:
        ser = serial.Serial(port, baud, timeout=1)
    except Exception as e:
        print(f"Error opening serial port: {e}")
        print("Falling back to simulated calibration...")
        return simulate_steering_calibration()

    # Phase 1: IMU zero-rate calibration
    print("\n[Phase 1] Keep the steering wheel STILL for 10 seconds...")
    gyro_readings = []
    start = time.time()
    while time.time() - start < 10:
        line = ser.readline().decode("utf-8", errors="replace").strip()
        if "gyro" in line.lower():
            try:
                val = float(line.split("gyro=")[1].split()[0])
                gyro_readings.append(val)
            except (IndexError, ValueError):
                pass
        time.sleep(0.01)

    gyro_bias = sum(gyro_readings) / max(len(gyro_readings), 1)
    print(f"  Gyro zero-rate bias: {gyro_bias:.2f} milli-deg/sec")

    # Phase 2: Grip baseline
    print("\n[Phase 2] Take your hands OFF the wheel for 10 seconds...")
    grip_readings = []
    start = time.time()
    while time.time() - start < 10:
        line = ser.readline().decode("utf-8", errors="replace").strip()
        if "grip" in line.lower():
            try:
                val = float(line.split("grip=")[1].split()[0])
                grip_readings.append(val)
            except (IndexError, ValueError):
                pass
        time.sleep(0.01)

    grip_baseline = sum(grip_readings) / max(len(grip_readings), 1)
    print(f"  Grip baseline (no hands): {grip_baseline:.0f}")

    # Phase 3: Grip with hands
    print("\n[Phase 3] Put your hands ON the wheel normally for 10 seconds...")
    grip_with_hands = []
    start = time.time()
    while time.time() - start < 10:
        line = ser.readline().decode("utf-8", errors="replace").strip()
        if "grip" in line.lower():
            try:
                val = float(line.split("grip=")[1].split()[0])
                grip_with_hands.append(val)
            except (IndexError, ValueError):
                pass
        time.sleep(0.01)

    grip_active = sum(grip_with_hands) / max(len(grip_with_hands), 1)
    grip_threshold = (grip_baseline + grip_active) / 2
    print(f"  Grip with hands: {grip_active:.0f}")
    print(f"  Hands-on threshold: {grip_threshold:.0f}")

    calibration = {
        "gyro_zero_bias": gyro_bias,
        "grip_baseline": grip_baseline,
        "grip_active": grip_active,
        "hands_on_threshold": grip_threshold,
        "timestamp": time.time(),
    }

    cal_path = os.path.join(os.path.dirname(__file__), "..", "hardware", "calibration_steering.json")
    with open(cal_path, "w") as f:
        json.dump(calibration, f, indent=2)
    print(f"\nCalibration saved to {cal_path}")

    ser.close()
    return calibration


def simulate_steering_calibration():
    calibration = {
        "gyro_zero_bias": 2.3,
        "grip_baseline": 4500,
        "grip_active": 5200,
        "hands_on_threshold": 4850,
        "timestamp": time.time(),
        "simulated": True,
    }
    cal_path = os.path.join(os.path.dirname(__file__), "..", "hardware", "calibration_steering.json")
    os.makedirs(os.path.dirname(cal_path), exist_ok=True)
    with open(cal_path, "w") as f:
        json.dump(calibration, f, indent=2)
    print(f"Simulated calibration saved to {cal_path}")
    print(json.dumps(calibration, indent=2))
    return calibration


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="DriveSync Steering Calibration")
    parser.add_argument("--port", default="/dev/ttyUSB1", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()

    calibrate_steering(args.port, args.baud)
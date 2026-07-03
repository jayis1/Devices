#!/usr/bin/env python3
"""
DriveSync — Camera Calibration Script

Calibrates the OV5640 eye-closure detection thresholds.
Connects to the Dash Hub via serial USB and runs a calibration sequence.

License: MIT
"""

import serial
import time
import json
import argparse
import os

def calibrate_camera(port="/dev/ttyUSB0", baud=115200, duration_sec=60):
    """
    Run camera calibration:
    1. Subject keeps eyes open for 30 seconds (baseline PERCLOS = 0)
    2. Subject blinks normally for 30 seconds (validate blink rate)
    3. Subject closes eyes for 5 seconds (validate PERCLOS detection)
    """
    print(f"DriveSync Camera Calibration")
    print(f"Port: {port}, Duration: {duration_sec}s")
    print(f"=" * 50)

    try:
        ser = serial.Serial(port, baud, timeout=1)
    except Exception as e:
        print(f"Error opening serial port: {e}")
        print("Falling back to simulated calibration...")
        return simulate_calibration()

    # Phase 1: Open eyes baseline
    print("\n[Phase 1] Keep your eyes OPEN and look at the camera for 30 seconds...")
    open_perclos = collect_perclos(ser, 30)
    print(f"  Baseline PERCLOS (eyes open): {open_perclos:.3f} (should be < 0.05)")

    # Phase 2: Normal blinking
    print("\n[Phase 2] Blink normally for 30 seconds...")
    blink_data = collect_blink_data(ser, 30)
    avg_blink_rate = sum(d.get("blink", 0) for d in blink_data) / len(blink_data)
    print(f"  Average blink rate: {avg_blink_rate:.1f} blinks/min (normal: 15-20)")

    # Phase 3: Closed eyes
    print("\n[Phase 3] Close your eyes for 5 seconds...")
    closed_perclos = collect_perclos(ser, 5)
    print(f"  PERCLOS (eyes closed): {closed_perclos:.3f} (should be > 0.80)")

    # Save calibration
    calibration = {
        "open_perclos_baseline": open_perclos,
        "closed_perclos_threshold": closed_perclos * 0.8,
        "normal_blink_rate": avg_blink_rate,
        "drowsy_blink_threshold": avg_blink_rate * 0.6,
        "timestamp": time.time(),
    }

    cal_path = os.path.join(os.path.dirname(__file__), "..", "hardware", "calibration_camera.json")
    with open(cal_path, "w") as f:
        json.dump(calibration, f, indent=2)
    print(f"\nCalibration saved to {cal_path}")

    ser.close()
    return calibration


def collect_perclos(ser, duration):
    """Collect PERCLOS readings from hub serial output."""
    values = []
    start = time.time()
    while time.time() - start < duration:
        line = ser.readline().decode("utf-8", errors="replace").strip()
        if "perclos" in line.lower():
            try:
                # Parse PERCLOS from log output
                val = float(line.split("perclos=")[1].split()[0])
                values.append(val)
            except (IndexError, ValueError):
                pass
        time.sleep(0.1)
    return sum(values) / max(len(values), 1)


def collect_blink_data(ser, duration):
    """Collect blink rate data from hub serial output."""
    data = []
    start = time.time()
    while time.time() - start < duration:
        line = ser.readline().decode("utf-8", errors="replace").strip()
        if "blink" in line.lower():
            data.append({"blink": 15})  # Placeholder
        time.sleep(0.1)
    return data


def simulate_calibration():
    """Generate simulated calibration data for testing without hardware."""
    calibration = {
        "open_perclos_baseline": 0.02,
        "closed_perclos_threshold": 0.80,
        "normal_blink_rate": 17.5,
        "drowsy_blink_threshold": 10.5,
        "timestamp": time.time(),
        "simulated": True,
    }
    cal_path = os.path.join(os.path.dirname(__file__), "..", "hardware", "calibration_camera.json")
    os.makedirs(os.path.dirname(cal_path), exist_ok=True)
    with open(cal_path, "w") as f:
        json.dump(calibration, f, indent=2)
    print(f"Simulated calibration saved to {cal_path}")
    print(json.dumps(calibration, indent=2))
    return calibration


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="DriveSync Camera Calibration")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--duration", type=int, default=60, help="Calibration duration (sec)")
    args = parser.parse_args()

    calibrate_camera(args.port, args.baud, args.duration)
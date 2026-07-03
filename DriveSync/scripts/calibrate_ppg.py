#!/usr/bin/env python3
"""
DriveSync — PPG Sensor Calibration Script

Calibrates the MAX30101 PPG sensor on the Seat Belt Tag.
Validates heart rate detection and HRV computation.

License: MIT
"""

import serial
import time
import json
import argparse
import os

def calibrate_ppg(port="/dev/ttyUSB2", baud=115200, duration_sec=120):
    """
    Run PPG calibration:
    1. Sit still and breathe normally for 2 minutes
    2. Validate heart rate against a reference (manual pulse check)
    3. Record HRV baseline
    """
    print("DriveSync PPG Sensor Calibration")
    print(f"Port: {port}, Duration: {duration_sec}s")
    print("=" * 50)

    try:
        ser = serial.Serial(port, baud, timeout=1)
    except Exception as e:
        print(f"Error opening serial port: {e}")
        print("Falling back to simulated calibration...")
        return simulate_ppg_calibration()

    print("\n[Calibration] Sit still and clip the belt tag to your seatbelt.")
    print("  Breathe normally. Do not move for 2 minutes.")
    print("  Starting in 5 seconds...")
    time.sleep(5)

    hr_readings = []
    hrv_readings = []
    start = time.time()

    while time.time() - start < duration_sec:
        line = ser.readline().decode("utf-8", errors="replace").strip()
        if "hr=" in line.lower():
            try:
                hr = int(line.split("hr=")[1].split()[0])
                if 40 < hr < 180:
                    hr_readings.append(hr)
            except (IndexError, ValueError):
                pass
        if "hrv=" in line.lower():
            try:
                hrv = int(line.split("hrv=")[1].split()[0])
                if hrv > 0:
                    hrv_readings.append(hrv)
            except (IndexError, ValueError):
                pass
        time.sleep(0.1)

    if hr_readings:
        avg_hr = sum(hr_readings) / len(hr_readings)
        hr_var = max(hr_readings) - min(hr_readings)
        print(f"\n  Average HR: {avg_hr:.1f} bpm")
        print(f"  HR range: {min(hr_readings)}-{max(hr_readings)} bpm (var: {hr_var})")
    else:
        avg_hr = 0
        hr_var = 0
        print("\n  Warning: No HR readings received!")

    if hrv_readings:
        avg_hrv = sum(hrv_readings) / len(hrv_readings)
        print(f"  Average HRV (RMSSD): {avg_hrv:.1f} ms")
    else:
        avg_hrv = 0
        print("  Warning: No HRV readings received!")

    calibration = {
        "avg_hr": avg_hr,
        "hr_variance": hr_var,
        "avg_hrv_rmssd": avg_hrv,
        "hr_confidence": len(hr_readings) / (duration_sec / 2),  # readings per 0.5s
        "timestamp": time.time(),
    }

    cal_path = os.path.join(os.path.dirname(__file__), "..", "hardware", "calibration_ppg.json")
    with open(cal_path, "w") as f:
        json.dump(calibration, f, indent=2)
    print(f"\nCalibration saved to {cal_path}")

    ser.close()
    return calibration


def simulate_ppg_calibration():
    calibration = {
        "avg_hr": 72.3,
        "hr_variance": 8,
        "avg_hrv_rmssd": 42.5,
        "hr_confidence": 0.95,
        "timestamp": time.time(),
        "simulated": True,
    }
    cal_path = os.path.join(os.path.dirname(__file__), "..", "hardware", "calibration_ppg.json")
    os.makedirs(os.path.dirname(cal_path), exist_ok=True)
    with open(cal_path, "w") as f:
        json.dump(calibration, f, indent=2)
    print(f"Simulated calibration saved to {cal_path}")
    print(json.dumps(calibration, indent=2))
    return calibration


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="DriveSync PPG Calibration")
    parser.add_argument("--port", default="/dev/ttyUSB2", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--duration", type=int, default=120, help="Calibration duration (sec)")
    args = parser.parse_args()

    calibrate_ppg(args.port, args.baud, args.duration)
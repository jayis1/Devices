#!/usr/bin/env python3
"""
calibrate_bp.py — Calibrate BP cuff pressure sensor

Calibrates the MP3V5050GP pressure sensor against a reference
manometer. Saves calibration coefficients to the BP cuff firmware.

Usage:
    python calibrate_bp.py --port /dev/ttyUSB0

License: MIT
"""
import serial
import sys
import argparse
import json
import numpy as np

def calibrate(port):
    print("CardioSync BP Cuff Calibration")
    print("=" * 40)
    print(f"Port: {port}")
    print()
    print("You will need a reference manometer connected to the cuff.")
    print("Calibration points: 0, 50, 100, 150, 200 mmHg")
    print()

    try:
        ser = serial.Serial(port, 115200, timeout=5)
    except Exception as e:
        print(f"Error opening serial port: {e}")
        sys.exit(1)

    pressures = [0, 50, 100, 150, 200]
    adc_values = []

    for target in pressures:
        input(f"Inflate cuff to {target} mmHg using reference manometer, then press Enter...")
        ser.write(b"READ_PRESSURE\n")
        response = ser.readline().decode().strip()
        print(f"  ADC reading at {target} mmHg: {response}")
        try:
            adc = int(response.split("=")[1])
            adc_values.append(adc)
        except (IndexError, ValueError):
            print(f"  Error parsing ADC value: {response}")
            adc_values.append(0)

    ser.close()

    # Linear regression: mmHg = a × ADC + b
    x = np.array(adc_values)
    y = np.array(pressures)
    coeffs = np.polyfit(x, y, 1)
    a, b = coeffs

    print(f"\nCalibration coefficients:")
    print(f"  mmHg = {a:.6f} × ADC + {b:.6f}")

    # Save to file
    cal_data = {"a": float(a), "b": float(b), "points": list(zip(adc_values, pressures))}
    with open("bp_calibration.json", "w") as f:
        json.dump(cal_data, f, indent=2)
    print(f"\nCalibration saved to bp_calibration.json")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Calibrate BP cuff pressure sensor")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port")
    args = parser.parse_args()
    calibrate(args.port)
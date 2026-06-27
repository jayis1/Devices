#!/usr/bin/env python3
"""
Calibrate capacitive moisture sensors for the Bin Node.
Run this script with the sensors connected to the ESP32 via serial.

Procedure:
  1. Hold sensors in air (dry) → record dry reading
  2. Submerge in water (wet) → record wet reading
  3. Save calibration to firmware/config

Usage: python3 calibrate_moisture.py --port /dev/ttyUSB1
"""
import argparse
import serial
import time
import json
import os

def calibrate(port):
    print("=" * 50)
    print("CompostSync Moisture Sensor Calibration")
    print("=" * 50)

    readings = {"dry": [], "wet": []}

    # Step 1: Dry reading
    print("\nStep 1: Hold all 3 moisture sensors in AIR (dry)")
    input("Press Enter when ready...")
    print("Reading dry values (10 samples over 5 seconds)...")

    for i in range(10):
        line = port.readline().decode().strip()
        if "M1=" in line:
            # Parse M1=XX M2=XX M3=XX from serial output
            m1 = m2 = m3 = 0
            for part in line.split():
                if part.startswith("M1="): m1 = int(part.replace("M1=","").replace("%",""))
                elif part.startswith("M2="): m2 = int(part.replace("M2=","").replace("%",""))
                elif part.startswith("M3="): m3 = int(part.replace("M3=","").replace("%",""))
            readings["dry"].append([m1, m2, m3])
        time.sleep(0.5)

    dry_avg = [sum(col) / len(col) for col in zip(*readings["dry"])]
    print(f"Dry readings (avg): M1={dry_avg[0]:.0f} M2={dry_avg[1]:.0f} M3={dry_avg[2]:.0f}")

    # Step 2: Wet reading
    print("\nStep 2: Submerge all 3 sensors in WATER (wet)")
    input("Press Enter when ready...")
    print("Reading wet values (10 samples over 5 seconds)...")

    readings["wet"] = []
    for i in range(10):
        line = port.readline().decode().strip()
        if "M1=" in line:
            m1 = m2 = m3 = 0
            for part in line.split():
                if part.startswith("M1="): m1 = int(part.replace("M1=","").replace("%",""))
                elif part.startswith("M2="): m2 = int(part.replace("M2=","").replace("%",""))
                elif part.startswith("M3="): m3 = int(part.replace("M3=","").replace("%",""))
            readings["wet"].append([m1, m2, m3])
        time.sleep(0.5)

    wet_avg = [sum(col) / len(col) for col in zip(*readings["wet"])]
    print(f"Wet readings (avg): M1={wet_avg[0]:.0f} M2={wet_avg[1]:.0f} M3={wet_avg[2]:.0f}")

    # Save calibration
    cal = {
        "dry_adc": [int(d) for d in dry_avg],
        "wet_adc": [int(w) for w in wet_avg],
        "dry_pct": 0,
        "wet_pct": 100,
    }

    cal_path = os.path.join(os.path.dirname(__file__), "..", "firmware", "common", "moisture_cal.json")
    with open(cal_path, "w") as f:
        json.dump(cal, f, indent=2)
    print(f"\nCalibration saved to: {cal_path}")
    print("Update MOISTURE_DRY and MOISTURE_WET in firmware/bin-node/sensors.c")

def main():
    parser = argparse.ArgumentParser(description="Calibrate moisture sensors")
    parser.add_argument("--port", default="/dev/ttyUSB1", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()

    try:
        port = serial.Serial(args.port, args.baud, timeout=1)
        calibrate(port)
    except Exception as e:
        print(f"Error: {e}")
        print(f"Make sure the Bin Node is connected to {args.port} and outputting sensor readings")

if __name__ == "__main__":
    main()
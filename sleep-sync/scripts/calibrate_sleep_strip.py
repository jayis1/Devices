#!/usr/bin/env python3
"""
calibrate_sleep_strip.py — Calibrate SleepSync sleep strip sensors

Usage:
    python calibrate_sleep_strip.py --port /dev/ttyACM0

Calibration steps:
1. Remove strip from pillow (no load on FSRs)
2. Run tare to zero all HX711 channels
3. Place known weight on strip (e.g., 500g calibration weight)
4. Record scale factor for each FSR channel
5. Save calibration to strip's NVS
"""

import argparse
import serial
import time
import json

def calibrate(port, baudrate=115200):
    print("=== SleepSync Sleep Strip Calibration ===\n")
    
    try:
        ser = serial.Serial(port, baudrate, timeout=5)
    except Exception as e:
        print(f"Error opening {port}: {e}")
        return

    # Wait for strip to boot
    time.sleep(2)
    
    # Step 1: Tare (zero offset)
    print("Step 1: TARE")
    print("  Remove all weight from the sleep strip.")
    input("  Press Enter when ready...")
    ser.write(b"CAL:TARE\n")
    time.sleep(2)
    response = ser.readline().decode().strip()
    print(f"  Response: {response}")
    
    # Step 2: Scale factor
    print("\nStep 2: SCALE FACTOR")
    print("  Place a known weight on the center of the strip.")
    weight_grams = float(input("  Enter weight in grams: "))
    ser.write(f"CAL:SCALE:{weight_grams}\n".encode())
    time.sleep(3)
    response = ser.readline().decode().strip()
    print(f"  Response: {response}")
    
    # Step 3: Heart rate validation
    print("\nStep 3: HEART RATE VALIDATION")
    print("  Lie still on the strip with it under your pillow.")
    print("  The strip will compare its HR reading to a reference.")
    ref_hr = float(input("  Enter your reference heart rate (BPM): "))
    ser.write(f"CAL:HR:{ref_hr}\n".encode())
    time.sleep(10)
    response = ser.readline().decode().strip()
    print(f"  Response: {response}")
    
    # Step 4: Save calibration
    print("\nStep 4: SAVE")
    ser.write(b"CAL:SAVE\n")
    time.sleep(1)
    response = ser.readline().decode().strip()
    print(f"  Response: {response}")
    
    ser.close()
    print("\nCalibration complete! You can now use the sleep strip.")


def main():
    parser = argparse.ArgumentParser(description="Calibrate SleepSync sleep strip")
    parser.add_argument("--port", required=True, help="Serial port (e.g., /dev/ttyACM0)")
    parser.add_argument("--baudrate", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()
    calibrate(args.port, args.baudrate)


if __name__ == "__main__":
    main()
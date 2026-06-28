#!/usr/bin/env python3
"""
PestSync — Camera Calibration
scripts/calibrate_camera.py

Calibrates the OV2640 camera on a Pest Sentinel via USB serial.
Adjusts exposure, gain, white balance, and captures test frames.

Usage:
  python calibrate_camera.py --port /dev/ttyUSB0
"""
import argparse
import serial
import time
import sys
import base64


def calibrate_camera(port: str, baud: int = 115200):
    print(f"\n🔧 Pest Sentinel Camera Calibration")
    print(f"   Port: {port}")

    ser = serial.Serial(port, baud, timeout=10)
    time.sleep(2)  # Wait for ESP32-S3 boot

    # Check camera status
    print("\nStep 1: Checking camera connection...")
    ser.write(b"CAM:STATUS\n")
    response = ser.readline().decode().strip()
    print(f"   Camera status: {response}")

    if "OK" not in response:
        print("❌ Camera not detected. Check OV2640 connection.")
        ser.close()
        return

    # Step 2: Adjust exposure
    print("\nStep 2: Auto-exposure capture (daylight mode)...")
    ser.write(b"CAM:MODE:DAY\n")
    time.sleep(1)
    ser.write(b"CAM:CAPTURE\n")
    time.sleep(2)
    response = ser.readline().decode().strip()
    if response.startswith("FRAME:"):
        frame_b64 = response.split("FRAME:")[1]
        with open("test_frame_day.jpg", "wb") as f:
            f.write(base64.b64decode(frame_b64))
        print("   ✅ Daylight frame saved: test_frame_day.jpg")
    else:
        print(f"   ⚠️  No frame received: {response}")

    # Step 3: Night mode (IR)
    print("\nStep 3: Night mode capture (IR illumination)...")
    ser.write(b"CAM:MODE:NIGHT\n")
    time.sleep(1)
    ser.write(b"CAM:CAPTURE\n")
    time.sleep(2)
    response = ser.readline().decode().strip()
    if response.startswith("FRAME:"):
        frame_b64 = response.split("FRAME:")[1]
        with open("test_frame_night.jpg", "wb") as f:
            f.write(base64.b64decode(frame_b64))
        print("   ✅ Night (IR) frame saved: test_frame_night.jpg")
    else:
        print(f"   ⚠️  No frame received: {response}")

    # Step 4: ML model verification
    print("\nStep 4: ML model inference test...")
    ser.write(b"CAM:INFER\n")
    time.sleep(3)
    response = ser.readline().decode().strip()
    print(f"   Inference result: {response}")

    print("\n✅ Camera calibration complete!")
    print("   Check test_frame_day.jpg and test_frame_night.jpg")
    ser.close()


def main():
    parser = argparse.ArgumentParser(description="PestSync Camera Calibration")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()

    try:
        calibrate_camera(args.port, args.baud)
    except serial.SerialException as e:
        print(f"❌ Serial error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
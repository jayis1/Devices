#!/usr/bin/env python3
"""
EchoSync — Sensor Calibration Script

Calibrates room sentinel microphones, door tag piezo thresholds,
and wrist band haptic patterns.
"""
import argparse
import serial
import time
import json
import sys


def calibrate_sentinel(port: str):
    """Calibrate room sentinel 4-mic array."""
    print(f"=== Room Sentinel Calibration (port: {port}) ===")
    print("1. Place sentinel in quiet room")
    print("2. Press ENTER to start calibration...")
    input()

    try:
        ser = serial.Serial(port, 115200, timeout=5)
    except Exception as e:
        print(f"Error opening {port}: {e}")
        return

    # Send calibration command
    ser.write(b"CALIBRATE\n")
    time.sleep(2)

    # Read response
    response = ser.readline().decode().strip()
    print(f"Calibration response: {response}")

    # Check each microphone
    for mic in range(4):
        ser.write(f"MIC_TEST {mic}\n".encode())
        time.sleep(1)
        result = ser.readline().decode().strip()
        print(f"  Mic {mic}: {result}")

    print("\nCalibration complete. Check noise floor levels.")
    ser.close()


def calibrate_door_tag(port: str):
    """Calibrate door tag piezo threshold."""
    print(f"=== Door Tag Calibration (port: {port}) ===")
    print("1. Mount door tag on door surface")
    print("2. Knock 3 times on the door")
    print("3. Press ENTER to start...")
    input()

    try:
        ser = serial.Serial(port, 115200, timeout=5)
    except Exception as e:
        print(f"Error opening {port}: {e}")
        return

    # Read piezo values for 5 seconds
    ser.write(b"PIEZO_CALIBRATE\n")
    print("Calibrating piezo threshold (5 seconds)...")

    values = []
    start = time.time()
    while time.time() - start < 5:
        line = ser.readline().decode().strip()
        if line.startswith("PIEZO:"):
            val = int(line.split(":")[1])
            values.append(val)

    if values:
        max_val = max(values)
        threshold = max_val + (max_val * 0.2)  # 20% above max
        print(f"  Max piezo value: {max_val}")
        print(f"  Recommended threshold: {threshold}")
        ser.write(f"SET_THRESHOLD {threshold}\n".encode())
        print(f"  Threshold set to {threshold}")
    else:
        print("  No piezo data received!")

    ser.close()


def calibrate_wrist_band(port: str):
    """Calibrate wrist band haptic patterns."""
    print(f"=== Wrist Band Calibration (port: {port}) ===")
    print("Testing haptic patterns...")
    print("Make sure the band is on your wrist.")

    try:
        ser = serial.Serial(port, 115200, timeout=5)
    except Exception as e:
        print(f"Error opening {port}: {e}")
        return

    patterns = [
        ("Info (single-tap)", 12),
        ("Important (double-pulse)", 47),
        ("Emergency (triple-burst)", 73),
    ]

    for name, effect in patterns:
        print(f"\nTesting: {name} (effect {effect})")
        input("Press ENTER to feel the pattern...")
        ser.write(f"HAPTIC_TEST {effect}\n".encode())
        time.sleep(1)

    print("\nAll patterns tested.")
    ser.close()


def main():
    parser = argparse.ArgumentParser(description="EchoSync Sensor Calibration")
    parser.add_argument("--node", choices=["sentinel", "door-tag", "wrist-band"],
                        required=True, help="Node type to calibrate")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port")
    args = parser.parse_args()

    if args.node == "sentinel":
        calibrate_sentinel(args.port)
    elif args.node == "door-tag":
        calibrate_door_tag(args.port)
    elif args.node == "wrist-band":
        calibrate_wrist_band(args.port)


if __name__ == "__main__":
    main()
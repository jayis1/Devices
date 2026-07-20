#!/usr/bin/env python3
"""
MosquitoSync — Sensor Calibration Script

Calibrates the sensors on each MosquitoSync node:
  - Acoustic Sentinel: microphone gain, WingNet threshold
  - CO2 Trap: IR beam sensitivity, PTC heater PID tuning
  - Window Barrier: motor stall threshold, reed switch positions
  - Weather Sentinel: wind direction zero, rain gauge tips per mm

Usage: python calibrate_sensors.py --node <node_type> [--port /dev/ttyUSB0]
"""
from __future__ import annotations

import argparse
import serial
import sys
import time


def calibrate_acoustic(port: str) -> None:
    """Calibrate acoustic sentinel microphone and WingNet threshold."""
    print(f"[calibrate] Acoustic Sentinel calibration on {port}")
    print("  1. Verify microphone array is working...")
    print("  2. Set I²S gain (manual: play 440 Hz tone, check capture)")
    print("  3. Set WingNet confidence threshold (default: 70%)")
    print("  4. Record background noise level")
    print("  5. Verify detection: play synthetic wingbeat (484 Hz)")

    with serial.Serial(port, 115200, timeout=5) as s:
        s.write(b"CAL:ACOUSTIC\n")
        time.sleep(1)
        response = s.read_all().decode(errors="replace")
        print(f"  Node response: {response.strip()}")

        # Check audio energy
        s.write(b"CAL:AUDIO_ENERGY\n")
        time.sleep(2)
        response = s.read_all().decode(errors="replace")
        print(f"  Audio energy: {response.strip()}")

    print("[calibrate] Acoustic sentinel calibration complete")


def calibrate_trap(port: str) -> None:
    """Calibrate CO2 trap IR beam, heater PID, and propane."""
    print(f"[calibrate] CO2 Trap calibration on {port}")
    print("  1. IR beam sensitivity (pass object, verify count)")
    print("  2. PTC heater PID (target: 37°C)")
    print("  3. Propane valve test (open/close)")
    print("  4. Fan speed test")
    print("  5. Camera capture test")
    print("  6. Safety check: MQ-4 gas sensor")

    with serial.Serial(port, 115200, timeout=5) as s:
        s.write(b"CAL:TRAP\n")
        time.sleep(1)
        response = s.read_all().decode(errors="replace")
        print(f"  Node response: {response.strip()}")

        # IR beam test
        print("  → Pass an object through the IR beam 5 times...")
        s.write(b"CAL:IR_TEST\n")
        time.sleep(10)
        response = s.read_all().decode(errors="replace")
        print(f"  IR beam counts: {response.strip()}")

        # Heater test
        s.write(b"CAL:HEATER_ON\n")
        time.sleep(30)
        s.write(b"CAL:HEATER_TEMP\n")
        time.sleep(2)
        response = s.read_all().decode(errors="replace")
        print(f"  Heater temperature: {response.strip()} (target: 37°C)")
        s.write(b"CAL:HEATER_OFF\n")

    print("[calibrate] CO2 trap calibration complete")


def calibrate_barrier(port: str) -> None:
    """Calibrate window barrier motor + reed switches."""
    print(f"[calibrate] Window Barrier calibration on {port}")
    print("  1. Motor open test (verify reed switch open position)")
    print("  2. Motor close test (verify reed switch closed position)")
    print("  3. Stall current calibration")
    print("  4. Cycle count test")

    with serial.Serial(port, 115200, timeout=5) as s:
        s.write(b"CAL:BARRIER\n")
        time.sleep(1)
        response = s.read_all().decode(errors="replace")
        print(f"  Node response: {response.strip()}")

        # Close test
        print("  → Closing barrier...")
        s.write(b"CAL:CLOSE\n")
        time.sleep(5)
        response = s.read_all().decode(errors="replace")
        print(f"  Close result: {response.strip()}")

        # Open test
        print("  → Opening barrier...")
        s.write(b"CAL:OPEN\n")
        time.sleep(5)
        response = s.read_all().decode(errors="replace")
        print(f"  Open result: {response.strip()}")

        # Stall test
        print("  → Stall test (hold screen, motor should stop)")
        s.write(b"CAL:STALL_TEST\n")
        time.sleep(5)
        response = s.read_all().decode(errors="replace")
        print(f"  Stall current: {response.strip()} mA")

    print("[calibrate] Window barrier calibration complete")


def calibrate_weather(port: str) -> None:
    """Calibrate weather sentinel wind direction + rain gauge."""
    print(f"[calibrate] Weather Sentinel calibration on {port}")
    print("  1. Wind direction zero calibration (point vane North)")
    print("  2. Rain gauge tip count (pour known water volume)")
    print("  3. BME280 verification")

    with serial.Serial(port, 115200, timeout=5) as s:
        s.write(b"CAL:WEATHER\n")
        time.sleep(1)
        response = s.read_all().decode(errors="replace")
        print(f"  Node response: {response.strip()}")

        # Wind direction
        print("  → Point vane to North, then press Enter...")
        input()
        s.write(b"CAL:WIND_ZERO\n")
        time.sleep(2)
        response = s.read_all().decode(errors="replace")
        print(f"  Wind direction zeroed: {response.strip()}")

        # Rain gauge
        print("  → Pour 10 mL water slowly into rain gauge...")
        input()
        s.write(b"CAL:RAIN_COUNT\n")
        time.sleep(10)
        response = s.read_all().decode(errors="replace")
        print(f"  Rain tips counted: {response.strip()} (expect ~50 tips at 0.2mm/tip)")

    print("[calibrate] Weather sentinel calibration complete")


def main() -> None:
    parser = argparse.ArgumentParser(description="MosquitoSync sensor calibration")
    parser.add_argument("--node", required=True,
                        choices=["acoustic", "trap", "barrier", "weather"],
                        help="Node type to calibrate")
    parser.add_argument("--port", default="/dev/ttyUSB0",
                        help="Serial port (default: /dev/ttyUSB0)")
    args = parser.parse_args()

    try:
        if args.node == "acoustic":
            calibrate_acoustic(args.port)
        elif args.node == "trap":
            calibrate_trap(args.port)
        elif args.node == "barrier":
            calibrate_barrier(args.port)
        elif args.node == "weather":
            calibrate_weather(args.port)
    except serial.SerialException as e:
        print(f"ERROR: Cannot open serial port {args.port}: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
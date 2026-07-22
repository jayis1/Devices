#!/usr/bin/env python3
"""
VoiceSync — Sensor Calibration Script

Calibrates the sensors on each VoiceSync node:
  - Vocal Band: contact microphone gain, F0 range, jitter/shimmer thresholds
  - Room Sentinel: I²S microphone gain, VoiceNet confidence threshold
  - Hydration Tag: HX711 tare + scale calibration, IMU sip detection sensitivity
  - Humidity Node: SHT40 verification, ultrasonic tank calibration, relay test

Usage: python calibrate_sensors.py --node <node_type> [--port /dev/ttyUSB0]
"""
from __future__ import annotations

import argparse
import serial
import sys
import time


def calibrate_vocal_band(port: str) -> None:
    """Calibrate vocal band contact microphone and feature extraction."""
    print(f"[calibrate] Vocal Band calibration on {port}")
    print("  1. Verify contact microphone is working...")
    print("  2. Set I²S codec PGA gain (sing at normal volume)")
    print("  3. Verify F0 detection (sing sustained 'ah' at known pitch)")
    print("  4. Check jitter/shimmer (should be <1.04% / <3.81% for normal)")
    print("  5. Verify HNR (>20 dB for normal voice)")
    print("  6. IMU neck angle calibration (look straight ahead)")
    print("  7. TMP117 skin temp verification (~35°C)")

    with serial.Serial(port, 115200, timeout=5) as s:
        s.write(b"CAL:VOCAL_BAND\n")
        time.sleep(1)
        response = s.read_all().decode(errors="replace")
        print(f"  Node response: {response.strip()}")

        # F0 detection test
        print("  → Sing a sustained 'ah' at 150 Hz for 5 seconds...")
        s.write(b"CAL:F0_TEST\n")
        time.sleep(6)
        response = s.read_all().decode(errors="replace")
        print(f"  F0 detected: {response.strip()} (expect ~150 Hz)")

        # Jitter/shimmer test
        s.write(b"CAL:JITTER_SHIMMER\n")
        time.sleep(3)
        response = s.read_all().decode(errors="replace")
        print(f"  Jitter/Shimmer: {response.strip()}")

        # IMU calibration
        print("  → Look straight ahead, press Enter...")
        input()
        s.write(b"CAL:IMU_ZERO\n")
        time.sleep(2)
        response = s.read_all().decode(errors="replace")
        print(f"  IMU zeroed: {response.strip()}")

    print("[calibrate] Vocal Band calibration complete")


def calibrate_room_sentinel(port: str) -> None:
    """Calibrate room sentinel microphone and VoiceNet threshold."""
    print(f"[calibrate] Room Sentinel calibration on {port}")
    print("  1. Verify 4-mic I²S array is working")
    print("  2. Set I²S gain (speak at normal volume)")
    print("  3. VoiceNet confidence threshold (default: 75%)")
    print("  4. SHT40 temp/humidity verification")
    print("  5. SGP40 VOC sensor baseline")

    with serial.Serial(port, 115200, timeout=5) as s:
        s.write(b"CAL:ROOM\n")
        time.sleep(1)
        response = s.read_all().decode(errors="replace")
        print(f"  Node response: {response.strip()}")

        # Audio test
        print("  → Speak normally for 5 seconds...")
        s.write(b"CAL:AUDIO_TEST\n")
        time.sleep(6)
        response = s.read_all().decode(errors="replace")
        print(f"  Audio + VoiceNet: {response.strip()}")

        # SHT40 test
        s.write(b"CAL:SHT40\n")
        time.sleep(2)
        response = s.read_all().decode(errors="replace")
        print(f"  SHT40: {response.strip()}")

        # SGP40 baseline
        s.write(b"CAL:SGP40_BASELINE\n")
        time.sleep(15)
        response = s.read_all().decode(errors="replace")
        print(f"  SGP40 baseline: {response.strip()}")

    print("[calibrate] Room Sentinel calibration complete")


def calibrate_hydration_tag(port: str) -> None:
    """Calibrate hydration tag load cell + IMU."""
    print(f"[calibrate] Hydration Tag calibration on {port}")
    print("  1. Tare load cell (empty bottle)")
    print("  2. Scale calibration (add known mass)")
    print("  3. IMU sip detection sensitivity")
    print("  4. Battery voltage check")

    with serial.Serial(port, 115200, timeout=5) as s:
        s.write(b"CAL:HYDRATION\n")
        time.sleep(1)
        response = s.read_all().decode(errors="replace")
        print(f"  Node response: {response.strip()}")

        # Tare
        print("  → Place empty bottle on tag, then press Enter...")
        input()
        s.write(b"CAL:TARE\n")
        time.sleep(3)
        response = s.read_all().decode(errors="replace")
        print(f"  Tare: {response.strip()}")

        # Scale calibration
        print("  → Add 500 g known mass, then press Enter...")
        input()
        s.write(b"CAL:SCALE_500G\n")
        time.sleep(3)
        response = s.read_all().decode(errors="replace")
        print(f"  Scale (500g): {response.strip()}")

        # Sip detection
        print("  → Lift bottle, tilt, drink, put down (1 cycle)...")
        s.write(b"CAL:SIP_TEST\n")
        time.sleep(10)
        response = s.read_all().decode(errors="replace")
        print(f"  Sip detection: {response.strip()}")

    print("[calibrate] Hydration Tag calibration complete")


def calibrate_humidity_node(port: str) -> None:
    """Calibrate humidity node SHT40 + ultrasonic + relays."""
    print(f"[calibrate] Humidity Node calibration on {port}")
    print("  1. SHT40 verification (compare to reference hygrometer)")
    print("  2. Ultrasonic tank level calibration (measure tank height)")
    print("  3. Humidifier relay test")
    print("  4. Fan relay test")

    with serial.Serial(port, 115200, timeout=5) as s:
        s.write(b"CAL:HUMIDITY\n")
        time.sleep(1)
        response = s.read_all().decode(errors="replace")
        print(f"  Node response: {response.strip()}")

        # SHT40
        s.write(b"CAL:SHT40_READ\n")
        time.sleep(2)
        response = s.read_all().decode(errors="replace")
        print(f"  SHT40: {response.strip()}")

        # Ultrasonic
        print("  → Measure tank height in cm, enter it:")
        height = input()
        s.write(f"CAL:TANK_HEIGHT:{height}\n".encode())
        time.sleep(2)
        response = s.read_all().decode(errors="replace")
        print(f"  Tank level: {response.strip()}")

        # Relay test
        print("  → Testing humidifier relay (5s ON)...")
        s.write(b"CAL:HUM_RELAY_ON\n")
        time.sleep(5)
        s.write(b"CAL:HUM_RELAY_OFF\n")
        response = s.read_all().decode(errors="replace")
        print(f"  Relay: {response.strip()}")

    print("[calibrate] Humidity Node calibration complete")


def main() -> None:
    parser = argparse.ArgumentParser(description="VoiceSync sensor calibration")
    parser.add_argument("--node", required=True,
                        choices=["vocal_band", "room", "hydration", "humidity"],
                        help="Node type to calibrate")
    parser.add_argument("--port", default="/dev/ttyUSB0",
                        help="Serial port (default: /dev/ttyUSB0)")
    args = parser.parse_args()

    try:
        if args.node == "vocal_band":
            calibrate_vocal_band(args.port)
        elif args.node == "room":
            calibrate_room_sentinel(args.port)
        elif args.node == "hydration":
            calibrate_hydration_tag(args.port)
        elif args.node == "humidity":
            calibrate_humidity_node(args.port)
    except serial.SerialException as e:
        print(f"ERROR: Cannot open serial port {args.port}: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
#!/usr/bin/env python3
"""
RehabSync — Sensor Calibration Script

Calibrates Body Sensor IMU (LSM6DSO + LIS3MDL), Smart Band load cell (HX711),
and Pressure Mat FSR array. Run this script via the Hub's USB-C serial console
or over Wi-Fi via the calibration API endpoint.

Usage:
  python calibrate_sensors.py --hub /dev/ttyUSB0
  python calibrate_sensors.py --wifi 192.168.1.100
"""
import argparse
import serial
import time
import json
import sys


def calibrate_body_sensor(ser, sensor_id):
    """Calibrate IMU: zero-rate level + magnetometer hard/soft iron."""
    print(f"\n=== Body Sensor {sensor_id} Calibration ===")
    print("Place sensor flat on a level surface, keep still for 10 seconds...")

    # Send calibration command
    ser.write(json.dumps({
        "cmd": "calibrate",
        "node_type": "body_sensor",
        "node_id": sensor_id,
        "cal_type": "imu_zero",
    }).encode() + b"\n")

    # Wait for response
    start = time.time()
    while time.time() - start < 15:
        if ser.in_waiting:
            resp = ser.readline().decode().strip()
            print(f"  {resp}")
            if "calibration_complete" in resp:
                break
        time.sleep(0.1)

    print("\nNow rotate the sensor in a figure-8 pattern for 15 seconds")
    print("(for magnetometer hard/soft iron calibration)...")
    time.sleep(3)

    ser.write(json.dumps({
        "cmd": "calibrate",
        "node_type": "body_sensor",
        "node_id": sensor_id,
        "cal_type": "mag_cal",
    }).encode() + b"\n")

    start = time.time()
    while time.time() - start < 20:
        if ser.in_waiting:
            resp = ser.readline().decode().strip()
            print(f"  {resp}")
            if "mag_cal_complete" in resp:
                break
        time.sleep(0.1)

    print(f"Body Sensor {sensor_id} calibration complete.")


def calibrate_smart_band(ser):
    """Calibrate HX711 load cell: tare + known weight."""
    print("\n=== Smart Band Calibration ===")
    print("Remove all tension from the band (no load)...")

    ser.write(json.dumps({
        "cmd": "calibrate",
        "node_type": "smart_band",
        "cal_type": "tare",
    }).encode() + b"\n")

    time.sleep(3)
    if ser.in_waiting:
        resp = ser.readline().decode().strip()
        print(f"  Tare: {resp}")

    print("\nNow apply a known weight (e.g., 5 kg) to the band...")
    input("Press Enter when weight is applied...")

    weight = float(input("Enter the known weight in kg: "))

    ser.write(json.dumps({
        "cmd": "calibrate",
        "node_type": "smart_band",
        "cal_type": "known_weight",
        "weight_kg": weight,
    }).encode() + b"\n")

    time.sleep(2)
    if ser.in_waiting:
        resp = ser.readline().decode().strip()
        print(f"  Scale calibration: {resp}")

    print("Smart Band calibration complete.")


def calibrate_pressure_mat(ser):
    """Calibrate FSR array: zero-pressure baseline."""
    print("\n=== Pressure Mat Calibration ===")
    print("Ensure no one is standing on the mat...")

    ser.write(json.dumps({
        "cmd": "calibrate",
        "node_type": "pressure_mat",
        "cal_type": "zero_pressure",
    }).encode() + b"\n")

    start = time.time()
    while time.time() - start < 30:
        if ser.in_waiting:
            resp = ser.readline().decode().strip()
            print(f"  {resp}")
            if "zero_cal_complete" in resp:
                break
        time.sleep(0.1)

    print("\nStand on the mat with both feet for weight reference...")
    input("Press Enter when standing on the mat...")

    weight = float(input("Enter your body weight in kg: "))

    ser.write(json.dumps({
        "cmd": "calibrate",
        "node_type": "pressure_mat",
        "cal_type": "weight_ref",
        "weight_kg": weight,
    }).encode() + b"\n")

    time.sleep(3)
    if ser.in_waiting:
        resp = ser.readline().decode().strip()
        print(f"  Weight calibration: {resp}")

    print("Pressure Mat calibration complete.")


def main():
    parser = argparse.ArgumentParser(description="RehabSync sensor calibration")
    parser.add_argument("--hub", type=str, help="Hub serial port (e.g., /dev/ttyUSB0)")
    parser.add_argument("--wifi", type=str, help="Hub IP address for Wi-Fi calibration")
    parser.add_argument("--sensors", type=str, default="all",
                       help="Comma-separated sensor IDs to calibrate (default: all)")
    args = parser.parse_args()

    if args.hub:
        ser = serial.Serial(args.hub, 115200, timeout=1)
    elif args.wifi:
        print("Wi-Fi calibration not yet implemented. Use USB-C serial.")
        sys.exit(1)
    else:
        print("Specify --hub or --wifi")
        sys.exit(1)

    print("RehabSync Sensor Calibration")
    print("=" * 40)

    # Wait for Hub to report connected sensors
    time.sleep(2)
    sensors_report = ""
    while ser.in_waiting:
        sensors_report += ser.read(ser.in_waiting).decode()
    print(f"Connected sensors: {sensors_report}")

    # Calibrate body sensors
    if args.sensors == "all" or "body" in args.sensors:
        calibrate_body_sensor(ser, 1)
        calibrate_body_sensor(ser, 2)

    # Calibrate smart band
    if args.sensors == "all" or "band" in args.sensors:
        calibrate_smart_band(ser)

    # Calibrate pressure mat
    if args.sensors == "all" or "mat" in args.sensors:
        calibrate_pressure_mat(ser)

    print("\n" + "=" * 40)
    print("All sensor calibration complete!")
    ser.close()


if __name__ == "__main__":
    main()
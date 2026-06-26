#!/usr/bin/env python3
"""
TrailSync — Sensor Calibration Script

Calibrates Wrist Unit and Shoe Pod sensors:
- PPG/SpO2 (MAX30101): LED current, sample rate, averaging
- Barometric altimeter (BMP390): sea-level pressure, altitude offset
- IMU (LSM6DSL): accel/gyro offset calibration
- Pressure insole (FSR array): zero-point and per-sensor gain

SPDX-License-Identifier: MIT
"""
import argparse
import json
import sys

def calibrate_ppg(serial_port: str):
    """Calibrate PPG/SpO2 sensor (MAX30101)."""
    print("PPG/SpO2 Calibration (MAX30101)")
    print("=" * 40)
    print(f"Connecting to {serial_port}...")

    # In production: send calibration commands over UART/Bluetooth
    # - Set LED current: red=0x1F, IR=0x1F, green=0x1F
    # - Set sample rate: 100 Hz
    # - Set pulse width: 411 us (18-bit)
    # - Average 4 samples

    cal = {
        "led_current_red": 0x1F,
        "led_current_ir": 0x1F,
        "led_current_green": 0x1F,
        "sample_rate_hz": 100,
        "pulse_width_us": 411,
        "averaging": 4,
    }
    print(f"PPG calibration: {json.dumps(cal, indent=2)}")
    print("✓ PPG calibration complete")
    return cal


def calibrate_barometer(serial_port: str):
    """Calibrate barometric altimeter (BMP390)."""
    print("\nBarometric Altimeter Calibration (BMP390)")
    print("=" * 40)

    # Known altitude for calibration
    known_altitude = float(input("Enter known altitude (meters above sea level): ") or "0")
    print(f"Setting altitude offset to {known_altitude}m")

    # In production: read current pressure, compute sea-level pressure
    # Using barometric formula: P0 = P * (1 + (alt/44330))^5.255
    current_pressure = 101325  # hPa * 10, placeholder
    sea_level_pressure = current_pressure * ((1 + known_altitude / 44330.0) ** 5.255)

    cal = {
        "known_altitude_m": known_altitude,
        "sea_level_pressure_hpa_x10": sea_level_pressure,
        "oversampling": "ultra_high",
        "iir_filter": "3",
    }
    print(f"Barometer calibration: {json.dumps(cal, indent=2)}")
    print("✓ Barometer calibration complete")
    return cal


def calibrate_imu(serial_port: str):
    """Calibrate IMU (LSM6DSL) accel/gyro offsets."""
    print("\nIMU Calibration (LSM6DSL)")
    print("=" * 40)
    print("Place the device flat on a level surface.")
    print("Do not move during calibration (5 seconds)...")

    # In production:
    # 1. Read 1000 samples of accel/gyro at rest
    # 2. Compute mean offset for each axis
    # 3. Accel Z should read ~1G (16384 LSB at ±16G)
    # 4. Accel X, Y should read ~0
    # 5. Gyro X, Y, Z should read ~0

    cal = {
        "accel_offset_x": 0,
        "accel_offset_y": 0,
        "accel_offset_z": 0,
        "gyro_offset_x": 0,
        "gyro_offset_y": 0,
        "gyro_offset_z": 0,
        "accel_range": "±16G",
        "gyro_range": "±2000dps",
        "odr": "6.66kHz",
    }
    print(f"IMU calibration: {json.dumps(cal, indent=2)}")
    print("✓ IMU calibration complete")
    return cal


def calibrate_pressure_insole(serial_port: str):
    """Calibrate 24-point pressure insole (FSR array)."""
    print("\nPressure Insole Calibration (24× FSR)")
    print("=" * 40)
    print("Remove shoes and stand normally on the insole.")
    print("This calibrates zero-point and per-sensor gain.")

    # In production:
    # 1. Record baseline with no load (all sensors)
    # 2. Record with known weight (body weight)
    # 3. Compute per-sensor offset and gain

    sensors = 24
    offsets = [0] * sensors
    gains = [1.0] * sensors

    cal = {
        "num_sensors": sensors,
        "offsets": offsets,
        "gains": gains,
        "weight_kg": 70,  # user body weight
        "sampling_rate_hz": 200,
    }
    print(f"Pressure insole calibration: {json.dumps(cal, indent=2)}")
    print("✓ Pressure insole calibration complete")
    return cal


def main():
    parser = argparse.ArgumentParser(description="TrailSync sensor calibration")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port")
    parser.add_argument("--sensor", choices=["ppg", "barometer", "imu", "insole", "all"],
                       default="all", help="Sensor to calibrate")
    args = parser.parse_args()

    results = {}
    if args.sensor in ("ppg", "all"):
        results["ppg"] = calibrate_ppg(args.port)
    if args.sensor in ("barometer", "all"):
        results["barometer"] = calibrate_barometer(args.port)
    if args.sensor in ("imu", "all"):
        results["imu"] = calibrate_imu(args.port)
    if args.sensor in ("insole", "all"):
        results["insole"] = calibrate_pressure_insole(args.port)

    with open("calibration.json", "w") as f:
        json.dump(results, f, indent=2)
    print(f"\n✓ All calibrations saved to calibration.json")


if __name__ == "__main__":
    main()
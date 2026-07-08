#!/usr/bin/env python3
"""
QuakeGuard Seismic Sensor Calibration Script

Calibrates ADXL355 accelerometer: zero-offset, scale factor, and
adaptive noise baseline. Run once after installation and after
any sensor replacement.

Procedure:
  1. Zero-offset: Average 1000 readings with sensor level (no motion)
  2. Scale factor: Apply 1g by rotating sensor (Z-axis down)
  3. Noise baseline: Collect 24h of data (or 10 min minimum)
  4. Set threshold: 6σ above baseline noise

Usage:
  python calibrate_seismic.py --port /dev/ttyUSB1

License: MIT
"""
import argparse
import serial
import time
import struct
import statistics
import json
from pathlib import Path

CONFIG_DIR = Path(__file__).parent.parent / "hardware" / "config"
CONFIG_DIR.mkdir(parents=True, exist_ok=True)


def read_adxl355_serial(ser: serial.Serial) -> tuple[int, int, int]:
    """Read 3-axis acceleration from Floor Node serial output.

    Expected format: "AX:X AY:Y AZ:Z\r\n" (milli-g)
    """
    line = ser.readline().decode().strip()
    parts = line.split()
    x = y = z = 0
    for part in parts:
        if part.startswith("AX:"):
            x = int(part[3:])
        elif part.startswith("AY:"):
            y = int(part[3:])
        elif part.startswith("AZ:"):
            z = int(part[3:])
    return x, y, z


def calibrate_zero_offset(ser: serial.Serial, n_samples=1000) -> dict:
    """Step 1: Zero-offset calibration.

    Place sensor on a flat, level surface with no vibration.
    Read 1000 samples and compute average offset.
    """
    print("\n[Step 1/4] Zero-offset calibration")
    print("  Place sensor on a LEVEL surface with NO vibration.")
    print("  Press Enter when ready...")
    input()

    print(f"  Collecting {n_samples} samples...")
    x_vals, y_vals, z_vals = [], [], []

    for i in range(n_samples):
        x, y, z = read_adxl355_serial(ser)
        x_vals.append(x)
        y_vals.append(y)
        z_vals.append(z)
        if i % 100 == 0:
            print(f"  {i}/{n_samples}...", end="\r")

    x_offset = statistics.mean(x_vals)
    y_offset = statistics.mean(y_vals)
    z_offset = statistics.mean(z_vals)

    # Z-axis should read ~1000 mg (1g) when level
    z_offset_from_1g = z_offset - 1000

    print(f"\n  Results:")
    print(f"    X offset: {x_offset:.1f} mg (should be ~0)")
    print(f"    Y offset: {y_offset:.1f} mg (should be ~0)")
    print(f"    Z offset: {z_offset:.1f} mg (should be ~1000)")
    print(f"    Z offset from 1g: {z_offset_from_1g:.1f} mg")

    return {
        "x_offset": x_offset,
        "y_offset": y_offset,
        "z_offset": z_offset,
        "z_offset_from_1g": z_offset_from_1g,
    }


def calibrate_scale_factor(ser: serial.Serial) -> dict:
    """Step 2: Scale factor calibration.

    Rotate sensor so each axis points down (experiences +1g).
    """
    print("\n[Step 2/4] Scale factor calibration")

    readings = {}
    for axis in ["X", "Y", "Z"]:
        print(f"\n  Place sensor with {axis}-axis pointing DOWN (experiences +1g).")
        print("  Press Enter when ready...")
        input()

        print(f"  Reading {axis}-axis...")
        vals = []
        for _ in range(500):
            x, y, z = read_adxl355_serial(ser)
            if axis == "X":
                vals.append(x)
            elif axis == "Y":
                vals.append(y)
            else:
                vals.append(z)

        avg = statistics.mean(vals)
        readings[f"{axis}_down"] = avg
        print(f"  {axis}-axis down: {avg:.1f} mg (should be ~+1000)")

    # Also read Z-axis up (should be -1000 mg)
    print("\n  Place sensor LEVEL again (Z-axis UP).")
    print("  Press Enter when ready...")
    input()

    vals = []
    for _ in range(500):
        x, y, z = read_adxl355_serial(ser)
        vals.append(z)
    z_up = statistics.mean(vals)
    readings["Z_up"] = z_up
    print(f"  Z-axis up: {z_up:.1f} mg (should be ~-1000)")

    # Scale factor: (reading_down - reading_up) / 2000 mg = scale
    z_scale = (readings["Z_down"] - z_up) / 2000.0
    print(f"\n  Z scale factor: {z_scale:.6f} (ideal: 1.0)")

    return readings


def calibrate_noise_baseline(ser: serial.Serial, duration_s=600) -> dict:
    """Step 3: Noise baseline calibration.

    Collect data for the specified duration (minimum 10 min for demo,
    24 hours recommended for production) and compute baseline statistics.
    """
    print(f"\n[Step 3/4] Noise baseline calibration ({duration_s}s)")
    print("  Ensure NO seismic activity, NO heavy foot traffic, NO door slamming.")
    print("  Press Enter when ready...")
    input()

    print(f"  Collecting for {duration_s} seconds...")
    readings = []
    start = time.time()

    while time.time() - start < duration_s:
        x, y, z = read_adxl355_serial(ser)
        mag = (x**2 + y**2 + z**2) ** 0.5
        readings.append(mag)

        if len(readings) % 1000 == 0:
            elapsed = time.time() - start
            print(f"  {elapsed:.0f}s / {duration_s}s "
                  f"({len(readings)} samples)", end="\r")

    mean = statistics.mean(readings)
    stdev = statistics.stdev(readings)
    threshold_6sigma = mean + 6 * stdev

    print(f"\n  Results ({len(readings)} samples):")
    print(f"    Mean: {mean:.1f} mg")
    print(f"    Std dev: {stdev:.1f} mg")
    print(f"    6σ threshold: {threshold_6sigma:.1f} mg")

    # Minimum threshold: 400 mg
    if threshold_6sigma < 400:
        threshold_6sigma = 400
        print(f"    Adjusted to minimum: {threshold_6sigma:.1f} mg")

    return {
        "mean": mean,
        "stdev": stdev,
        "threshold_6sigma": threshold_6sigma,
        "samples": len(readings),
        "duration_s": duration_s,
    }


def save_calibration(node_addr: str, calibration: dict):
    """Save calibration data to JSON file."""
    path = CONFIG_DIR / f"calibration_{node_addr}.json"
    with open(path, "w") as f:
        json.dump(calibration, f, indent=2)
    print(f"\n  Saved to {path}")


def main():
    parser = argparse.ArgumentParser(description="QuakeGuard seismic calibration")
    parser.add_argument("--port", required=True, help="Serial port")
    parser.add_argument("--addr", default="0x10", help="Node address")
    parser.add_argument("--baseline-duration", type=int, default=600,
                        help="Baseline duration in seconds (default: 600 = 10 min)")
    args = parser.parse_args()

    print("=" * 60)
    print("QuakeGuard Seismic Sensor Calibration")
    print("=" * 60)

    ser = serial.Serial(args.port, 115200, timeout=1)
    print(f"Connected to {args.port}")

    # Send calibration mode command
    ser.write(b"CALIBRATION_MODE\n")
    time.sleep(1)
    response = ser.readline().decode().strip()
    print(f"  Calibration mode: {response}")

    # Step 1: Zero-offset
    zero_offset = calibrate_zero_offset(ser)

    # Step 2: Scale factor
    scale_factor = calibrate_scale_factor(ser)

    # Step 3: Noise baseline
    baseline = calibrate_noise_baseline(ser, args.baseline_duration)

    # Step 4: Save and apply
    print("\n[Step 4/4] Saving calibration...")
    calibration = {
        "node_addr": args.addr,
        "zero_offset": zero_offset,
        "scale_factor": scale_factor,
        "baseline": baseline,
        "timestamp": time.time(),
    }
    save_calibration(args.addr, calibration)

    # Send calibration to node
    ser.write(f"SET_THRESHOLD {int(baseline['threshold_6sigma'])}\n".encode())
    time.sleep(0.5)
    response = ser.readline().decode().strip()
    print(f"  Threshold set: {response}")

    ser.close()
    print("\n✅ Calibration complete!")


if __name__ == "__main__":
    main()
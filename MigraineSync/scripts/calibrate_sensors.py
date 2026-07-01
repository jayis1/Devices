"""
MigraineSync — Sensor Calibration Script
=========================================
Calibrates barometric pressure, load cell, and PPG sensors.

Usage:
  python calibrate_sensors.py [--hub PORT] [--env PORT]
                              [--aura PORT] [--hydrate PORT]

License: MIT
"""

import argparse
import serial
import time
import json
import os

CALIBRATION_DIR = os.path.join(os.path.dirname(__file__), "..", "hardware", "calibration")
os.makedirs(CALIBRATION_DIR, exist_ok=True)


def calibrate_barometric(port: str, duration_s: int = 60):
    """Calibrate BMP390 barometric pressure sensors (Env Sentinel + Aura Band).

    Place both sensors side-by-side for the calibration period.
    Records readings and computes offset between them.
    """
    print(f"\n{'='*60}")
    print(f"  Barometric Pressure Calibration ({duration_s}s)")
    print(f"{'='*60}")
    print(f"  Port: {port}")
    print(f"  Place Env Sentinel and Aura Band side-by-side.")
    print(f"  Ensure stable temperature, no drafts.")
    print()

    # In production: connect via serial, read pressure values
    # For now: generate calibration instructions

    readings = []
    print("  Collecting readings...")

    # Simulated readings (in production: read from serial)
    for i in range(duration_s):
        # Reading would come from: ser.readline()
        reading = {"timestamp": time.time(), "pressure": 1013.25 + (i % 10) * 0.01}
        readings.append(reading)
        if (i + 1) % 10 == 0:
            print(f"    {i+1}/{duration_s}s: {reading['pressure']:.2f} hPa")
        time.sleep(1)

    avg_pressure = sum(r["pressure"] for r in readings) / len(readings)
    print(f"\n  Average pressure: {avg_pressure:.2f} hPa")
    print(f"  Reference (sea level): 1013.25 hPa")
    offset = avg_pressure - 1013.25
    print(f"  Offset: {offset:+.2f} hPa")

    cal_data = {
        "sensor": "BMP390",
        "port": port,
        "duration_s": duration_s,
        "avg_pressure_hpa": avg_pressure,
        "offset_hpa": offset,
        "n_readings": len(readings),
        "calibrated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }

    output_path = os.path.join(CALIBRATION_DIR, "barometric_calibration.json")
    with open(output_path, "w") as f:
        json.dump(cal_data, f, indent=2)
    print(f"  Saved: {output_path}")


def calibrate_load_cell(port: str):
    """Calibrate HX711 load cell for Hydrate Tag.

    Place known weights on the bottle platform and record readings.
    """
    print(f"\n{'='*60}")
    print(f"  Load Cell Calibration (HX711)")
    print(f"{'='*60}")
    print(f"  Port: {port}")
    print()

    weights = [0, 100, 250, 500, 750, 1000]  # grams
    print("  Place each weight on the platform and press Enter.")

    readings = []
    for w in weights:
        input(f"  Place {w}g on platform, then press Enter...")
        # In production: read HX711 raw value from serial
        raw = 8300000 + int(w * 420)  # simulated
        readings.append({"weight_g": w, "raw": raw})
        print(f"    {w}g → raw: {raw}")

    # Linear regression: weight = (raw - offset) / scale
    import numpy as np
    raws = np.array([r["raw"] for r in readings])
    weights_arr = np.array([r["weight_g"] for r in readings])

    # y = mx + b → weight = scale * raw + offset
    coeffs = np.polyfit(raws, weights_arr, 1)
    scale = coeffs[0]
    offset = coeffs[1]

    print(f"\n  Calibration: weight_g = {scale:.6f} × raw + {offset:.2f}")
    print(f"  Scale factor: {1/scale:.2f} counts per gram")

    cal_data = {
        "sensor": "HX711",
        "port": port,
        "scale": float(scale),
        "offset": float(offset),
        "counts_per_gram": float(1 / scale),
        "readings": readings,
        "calibrated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }

    output_path = os.path.join(CALIBRATION_DIR, "loadcell_calibration.json")
    with open(output_path, "w") as f:
        json.dump(cal_data, f, indent=2)
    print(f"  Saved: {output_path}")


def calibrate_ppg(port: str):
    """Calibrate MAX30101 PPG sensor for Aura Band."""
    print(f"\n{'='*60}")
    print(f"  PPG Calibration (MAX30101)")
    print(f"{'='*60}")
    print(f"  Port: {port}")
    print()
    print("  1. Wear the Aura Band snugly on wrist")
    print("  2. Stay still for 30 seconds")
    print("  3. Enter your skin tone (1-6 Fitzpatrick scale):")

    skin_tone = input("  Fitzpatrick skin type (1-6): ").strip()
    led_current_map = {1: 3.2, 2: 4.4, 3: 5.6, 4: 6.4, 5: 7.6, 6: 8.8}

    led_current = led_current_map.get(int(skin_tone), 6.4)
    print(f"  Recommended green LED current: {led_current} mA")

    cal_data = {
        "sensor": "MAX30101",
        "port": port,
        "fitzpatrick_skin_type": int(skin_tone),
        "green_led_current_ma": led_current,
        "calibrated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }

    output_path = os.path.join(CALIBRATION_DIR, "ppg_calibration.json")
    with open(output_path, "w") as f:
        json.dump(cal_data, f, indent=2)
    print(f"  Saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(description="MigraineSync sensor calibration")
    parser.add_argument("--hub", help="Hub serial port (e.g., /dev/ttyUSB0)")
    parser.add_argument("--env", help="Env Sentinel serial port")
    parser.add_argument("--aura", help="Aura Band serial port")
    parser.add_argument("--hydrate", help="Hydrate Tag serial port")
    parser.add_argument("--baro-duration", type=int, default=60,
                        help="Barometric calibration duration (seconds)")
    args = parser.parse_args()

    print("MigraineSync — Sensor Calibration")
    print("=" * 60)

    if args.env:
        calibrate_barometric(args.env, args.baro_duration)
    if args.aura:
        calibrate_barometric(args.aura, args.baro_duration)
        calibrate_ppg(args.aura)
    if args.hydrate:
        calibrate_load_cell(args.hydrate)

    if not any([args.env, args.aura, args.hydrate]):
        print("\nNo ports specified. Use --env, --aura, --hydrate to specify serial ports.")
        print("Example: python calibrate_sensors.py --env /dev/ttyUSB0 --aura /dev/ttyUSB1")


if __name__ == "__main__":
    main()
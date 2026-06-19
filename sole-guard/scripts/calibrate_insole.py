"""
SoleGuard Insole Calibration Script

Calibrates the 24-FSR pressure matrix and 8-NTC thermistor array on a
SoleGuard insole node. Connects to the insole's USB serial console and
runs a calibration sequence:

1. Zero-load offset: read all FSRs with no load → store baseline ADC
2. Known-weight calibration: place a 50N weight on each zone → compute
   kPa-per-ADC-count scale factor per sensor
3. Thermistor offset: read NTCs at known ambient temp → adjust B-parameter

Sends calibration constants to the insole via serial and saves to a JSON file.
"""

import os
import json
import serial
import time
import argparse

KNOWN_WEIGHT_N = 50.0       # 50N calibration weight
KNOWN_TEMP_C    = 25.0      # ambient temp during calibration
CALIB_DIR       = os.path.join(os.path.dirname(__file__), "..", "hardware", "calibration")


def read_all_sensors(ser: serial.Serial) -> dict:
    """Send 'READ' command and parse sensor values."""
    ser.write(b"READ\n")
    time.sleep(0.1)
    resp = ser.readline().decode(errors="ignore").strip()
    # Expected: "P:0,1,2,...,23|T:0,1,...,7"
    data = {"pressure": [0]*24, "temp": [0]*8}
    if "|" in resp:
        p_str, t_str = resp.split("|")
        if p_str.startswith("P:"):
            data["pressure"] = [int(x) for x in p_str[2:].split(",")]
        if t_str.startswith("T:"):
            data["temp"] = [int(x) for x in t_str[2:].split(",")]
    return data


def calibrate(port: str, side: str):
    os.makedirs(CALIB_DIR, exist_ok=True)
    ser = serial.Serial(port, 115200, timeout=2)
    time.sleep(1)  # let insole settle

    print(f"=== Calibrating {side} insole on {port} ===")

    # Step 1: zero-load offsets
    print("Step 1: Remove all weight from the insole. Press Enter when ready.")
    input()
    zero = read_all_sensors(ser)
    print(f"  Zero-load pressure: {zero['pressure']}")
    print(f"  Ambient temp: {[t/100 for t in zero['temp']]} °C")

    # Step 2: known-weight calibration per zone
    print(f"\nStep 2: Place {KNOWN_WEIGHT_N}N weight on each zone when prompted.")
    scales = [0.0] * 24
    zone_names = ["heel", "midfoot", "metatarsal-1", "metatarsal-2-5", "hallux", "lesser-toes"]
    for z in range(6):
        print(f"  Place weight on zone {z} ({zone_names[z]}). Press Enter when ready.")
        input()
        loaded = read_all_sensors(ser)
        for i in range(z*4, (z+1)*4):
            delta = loaded["pressure"][i] - zero["pressure"][i]
            if delta > 5:
                # 50N over 1 sensor area (~3cm²) ≈ 167 kPa; scale = kPa/count
                scales[i] = 167.0 / delta
            else:
                scales[i] = 2.0  # default

    # Step 3: thermistor offset (assume ambient = KNOWN_TEMP_C)
    temp_offset = [int(KNOWN_TEMP_C * 100) - zero["temp"][i] for i in range(8)]
    print(f"  Temp offsets (centi-degC): {temp_offset}")

    calib = {
        "side": side,
        "calibrated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "zero_offset": zero["pressure"],
        "pressure_scale_kpa_per_count": scales,
        "temp_offset_centic": temp_offset,
        "known_weight_n": KNOWN_WEIGHT_N,
        "known_temp_c": KNOWN_TEMP_C,
    }

    # Send to insole
    cmd = f"CALIB:{json.dumps(calib)}\n"
    ser.write(cmd.encode())
    time.sleep(0.5)
    ack = ser.readline().decode(errors="ignore").strip()
    print(f"  Insole ACK: {ack}")

    # Save locally
    path = os.path.join(CALIB_DIR, f"insole_{side}_calibration.json")
    with open(path, "w") as f:
        json.dump(calib, f, indent=2)
    print(f"\nCalibration saved to {path}")
    ser.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Calibrate SoleGuard insole")
    parser.add_argument("port", help="Serial port (e.g. /dev/ttyUSB0)")
    parser.add_argument("--side", choices=["left", "right"], default="left")
    args = parser.parse_args()
    calibrate(args.port, args.side)
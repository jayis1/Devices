#!/usr/bin/env python3
"""
MedSync - Load Cell Calibration Script
Calibrates HX711 load cells for each bin in the pill station.

Usage:
    python3 calibrate_scales.py --port /dev/ttyUSB0 --bins 8

This script communicates with the pill station over UART and performs
a two-point calibration for each bin's load cell:
  1. Tare (empty bin)
  2. Known weight (calibration mass)

Copyright (c) 2026 jayis1 - MIT License
"""

import serial
import time
import argparse
import json
from pathlib import Path


def send_command(ser, cmd: str, timeout: float = 5.0) -> dict:
    """Send a command to the pill station and read the response."""
    ser.write(f"{cmd}\n".encode())
    ser.flush()

    response = b""
    start = time.time()
    while time.time() - start < timeout:
        if ser.in_waiting:
            chunk = ser.read(ser.in_waiting)
            response += chunk
            if b"OK\n" in response or b"ERR\n" in response:
                break

    # Parse response
    text = response.decode().strip()
    if text.startswith("OK:"):
        try:
            return json.loads(text[3:])
        except json.JSONDecodeError:
            return {"raw": text}
    elif text.startswith("ERR:"):
        raise RuntimeError(f"Pill station error: {text[4:]}")
    else:
        return {"raw": text}


def calibrate_bin(ser, bin_id: int, calibration_weight_g: float):
    """Calibrate a single bin's load cell."""
    print(f"\n{'='*50}")
    print(f"  Calibrating Bin {bin_id}")
    print(f"{'='*50}")

    # Step 1: Tare (empty)
    print(f"\n  Step 1: Remove all items from bin {bin_id}")
    input("  Press Enter when bin is empty...")
    print("  Taring...")

    tare_response = send_command(ser, f"CAL_TARE {bin_id}")
    print(f"  Tare reading: {tare_response}")

    # Step 2: Known weight
    print(f"\n  Step 2: Place {calibration_weight_g}g calibration weight in bin {bin_id}")
    input("  Press Enter when weight is placed...")
    print("  Measuring...")

    weight_response = send_command(ser, f"CAL_WEIGHT {bin_id} {int(calibration_weight_g * 1000)}")
    print(f"  Weight reading: {weight_response}")

    # Step 3: Verify
    print(f"\n  Step 3: Remove the calibration weight")
    input("  Press Enter when weight is removed...")

    verify_response = send_command(ser, f"CAL_VERIFY {bin_id}")
    print(f"  Verification reading: {verify_response}")

    # Step 4: Save calibration
    save_response = send_command(ser, f"CAL_SAVE {bin_id}")
    print(f"  Calibration saved: {save_response}")

    return {
        "bin_id": bin_id,
        "tare": tare_response,
        "weight": weight_response,
        "verify": verify_response,
    }


def main():
    parser = argparse.ArgumentParser(description="Calibrate MedSync pill station load cells")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--bins", type=int, default=8, help="Number of bins")
    parser.add_argument("--weight", type=float, default=10.0, help="Calibration weight in grams")
    parser.add_argument("--output", default="calibration_data.json", help="Output file")

    args = parser.parse_args()

    print("=" * 50)
    print("  MedSync Scale Calibration")
    print("=" * 50)
    print(f"  Port: {args.port}")
    print(f"  Baud: {args.baud}")
    print(f"  Bins: {args.bins}")
    print(f"  Calibration weight: {args.weight}g")

    # Connect to pill station
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
        time.sleep(2)  # Wait for connection
    except serial.SerialException as e:
        print(f"\n  ERROR: Cannot open serial port {args.port}")
        print(f"  {e}")
        print(f"\n  Make sure:")
        print(f"    - Pill station is connected via USB")
        print(f"    - STM32 is in calibration mode (hold button during boot)")
        print(f"    - No other program is using the serial port")
        sys.exit(1)

    # Ping device
    print("\n  Pinging pill station...")
    try:
        ping = send_command(ser, "PING", timeout=3.0)
        print(f"  Connected: {ping}")
    except Exception as e:
        print(f"  ERROR: No response from pill station: {e}")
        ser.close()
        sys.exit(1)

    # Get current calibration data
    print("\n  Reading current calibration data...")
    current_cal = send_command(ser, "CAL_READ_ALL")
    print(f"  Current: {current_cal}")

    # Calibrate each bin
    results = []
    for bin_id in range(args.bins):
        result = calibrate_bin(ser, bin_id, args.weight)
        results.append(result)

    # Save results
    output_path = Path(args.output)
    with open(output_path, 'w') as f:
        json.dump(results, f, indent=2)
    print(f"\n  Calibration data saved to {output_path}")

    # Print summary
    print(f"\n{'='*50}")
    print(f"  Calibration Summary")
    print(f"{'='*50}")
    for r in results:
        print(f"  Bin {r['bin_id']}: Tare={r['tare']}, Weight={r['weight']}")

    print(f"\n  All {args.bins} bins calibrated successfully!")

    ser.close()


if __name__ == "__main__":
    import sys
    main()
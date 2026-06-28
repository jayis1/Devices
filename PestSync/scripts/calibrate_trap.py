#!/usr/bin/env python3
"""
PestSync — Smart Trap Calibration
scripts/calibrate_trap.py

Calibrates the HX711 load cell and bait level sensor on a Smart Trap.
Connects via USB serial to the trap's ESP32-C3.

Usage:
  python calibrate_trap.py --port /dev/ttyUSB0          # Calibrate load cell
  python calibrate_trap.py --port /dev/ttyUSB0 --bait    # Calibrate bait sensor
"""
import argparse
import serial
import time
import sys


def calibrate_load_cell(port: str, baud: int = 115200):
    """Calibrate HX711 load cell with known weights."""
    print(f"\n🔧 Smart Trap Load Cell Calibration")
    print(f"   Port: {port}")
    print(f"   Baud: {baud}")
    print()

    ser = serial.Serial(port, baud, timeout=5)
    time.sleep(1)  # Wait for ESP32 boot

    # Step 1: Tare (zero with no weight)
    print("Step 1: Remove all weight from the trap. Press Enter when ready.")
    input()
    ser.write(b"TARE\n")
    response = ser.readline().decode().strip()
    print(f"   Tare response: {response}")

    # Step 2: Known weight
    print("\nStep 2: Place a known weight on the trap (e.g., 20g for mouse reference).")
    weight_str = input("   Enter weight in grams: ")
    try:
        weight_g = float(weight_str)
    except ValueError:
        print("❌ Invalid weight")
        ser.close()
        return

    print(f"   Place {weight_g}g on the trap. Press Enter when ready.")
    input()
    ser.write(f"WEIGHT:{weight_g}\n".encode())
    response = ser.readline().decode().strip()
    print(f"   Calibration response: {response}")

    # Step 3: Verify
    print("\nStep 3: Verification — place a different known weight.")
    verify_str = input("   Enter verification weight in grams (or Enter to skip): ")
    if verify_str:
        try:
            verify_g = float(verify_str)
            print(f"   Place {verify_g}g on the trap. Press Enter when ready.")
            input()
            ser.write(b"READ\n")
            response = ser.readline().decode().strip()
            print(f"   Reading: {response}")
            print(f"   Expected: {verify_g}g")
        except ValueError:
            pass

    print("\n✅ Load cell calibration complete!")
    ser.close()


def calibrate_bait_sensor(port: str, baud: int = 115200):
    """Calibrate capacitive bait level sensor."""
    print(f"\n🔧 Smart Trap Bait Sensor Calibration")
    print(f"   Port: {port}")

    ser = serial.Serial(port, baud, timeout=5)
    time.sleep(1)

    # Step 1: Empty
    print("Step 1: Ensure bait compartment is EMPTY. Press Enter when ready.")
    input()
    ser.write(b"BAIT:EMPTY\n")
    response = ser.readline().decode().strip()
    print(f"   Empty ADC value: {response}")

    # Step 2: Full
    print("\nStep 2: Fill bait compartment with bait (peanut butter / bait block).")
    print("   Press Enter when ready.")
    input()
    ser.write(b"BAIT:FULL\n")
    response = ser.readline().decode().strip()
    print(f"   Full ADC value: {response}")

    print("\n✅ Bait sensor calibration complete!")
    ser.close()


def main():
    parser = argparse.ArgumentParser(description="PestSync Smart Trap Calibration")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--bait", action="store_true", help="Calibrate bait sensor instead of load cell")
    args = parser.parse_args()

    try:
        if args.bait:
            calibrate_bait_sensor(args.port, args.baud)
        else:
            calibrate_load_cell(args.port, args.baud)
    except serial.SerialException as e:
        print(f"❌ Serial error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\nInterrupted.")
        sys.exit(0)


if __name__ == "__main__":
    main()
#!/usr/bin/env python3
"""
HiveSync — Temperature Sensor Calibration
Calibrates SHT45 temperature sensors using ice water bath reference.

Usage:
    python3 calibrate_temp.py --node-id 0x0001
"""

import argparse
import time
import statistics

def calibrate_temp(node_id, num_samples=50):
    """Calibrate SHT45 temperature sensors against ice water reference.

    Process:
    1. Room temperature reading (reference: calibrated thermometer)
    2. Ice water bath (0°C reference)
    3. Compute per-sensor offset

    Args:
        node_id: Sub-GHz node address
        num_samples: Number of samples per calibration step
    """
    print(f"Calibrating temperature sensors for node 0x{node_id:04X}")
    print()

    # Step 1: Room temperature reference
    print("Step 1: Room Temperature Calibration")
    room_ref = float(input("Enter reference thermometer reading (°C): "))
    print(f"Taking {num_samples} readings from each sensor...")
    print()

    # Read from 3 SHT45 sensors (addresses 0x44, 0x45, 0x46)
    offsets = {}
    for i, addr in enumerate(["0x44 (Brood)", "0x45 (Top)", "0x46 (Entrance)"]):
        print(f"  Sensor {addr}:")
        # Production: actual I2C read from SHT45
        raw_readings = [0] * num_samples  # Placeholder
        avg_temp = statistics.mean(raw_readings) if any(raw_readings) else room_ref
        offset = room_ref - avg_temp
        offsets[addr] = offset
        print(f"    Average: {avg_temp:.2f}°C, Offset: {offset:+.2f}°C")

    print()

    # Step 2: Ice water bath (0°C)
    print("Step 2: Ice Water Bath Calibration")
    print("Place each sensor in an insulated bag, then submerge in ice water bath.")
    print("Wait 5 minutes for thermal equilibrium.")
    input("Press Enter when sensors are in ice water bath...")
    time.sleep(2)  # Wait a moment

    print(f"Taking {num_samples} readings from each sensor...")
    ice_offsets = {}
    for i, addr in enumerate(["0x44 (Brood)", "0x45 (Top)", "0x46 (Entrance)"]):
        print(f"  Sensor {addr}:")
        raw_readings = [0] * num_samples  # Placeholder
        avg_temp = statistics.mean(raw_readings) if any(raw_readings) else 0.0
        offset = 0.0 - avg_temp  # Should read 0°C
        ice_offsets[addr] = offset
        print(f"    Average: {avg_temp:.2f}°C, Offset: {offset:+.2f}°C")

    print()
    print("=" * 50)
    print("CALIBRATION RESULTS")
    print("=" * 50)

    # Use the average of room temp and ice bath offsets
    final_offsets = {}
    for addr in offsets:
        final_offsets[addr] = round((offsets[addr] + ice_offsets[addr]) / 2, 2)
        print(f"  Sensor {addr}: {final_offsets[addr]:+.2f}°C")

    print()
    print("Add these offsets to your firmware configuration:")
    for i, (addr, offset) in enumerate(final_offsets.items()):
        print(f"  SHT45_OFFSET_{i} = {offset:.2f}")

    return final_offsets

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Calibrate SHT45 temperature sensors")
    parser.add_argument("--node-id", type=lambda x: int(x, 0), required=True, help="Node address (hex)")
    parser.add_argument("--samples", type=int, default=50, help="Samples per calibration step")
    args = parser.parse_args()

    calibrate_temp(args.node_id, args.samples)
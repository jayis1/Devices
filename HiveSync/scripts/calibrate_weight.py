#!/usr/bin/env python3
"""
HiveSync — Weight Sensor Calibration
Calibrates HX711 load cells for accurate hive weight measurement.

Usage:
    python3 calibrate_weight.py --node-id 0x0001 --empty-weight 15.0
"""

import argparse
import time
import statistics

# In production, this would use the actual HX711 library
# For now, this is a reference implementation

def read_hx711():
    """Read from HX711 (placeholder — replace with actual GPIO read)."""
    # Production: RPi.GPIO or spidev to read HX711
    # import RPi.GPIO as GPIO
    # ... read 24 bits from HX711
    return 0

def calibrate(node_id, empty_weight_kg, num_samples=100):
    """Calibrate HX711 load cell.

    Process:
    1. Remove all weight (empty hive body)
    2. Take N raw readings
    3. Compute offset (tare)
    4. Place known reference weight
    5. Take N raw readings with reference
    6. Compute scale factor

    Args:
        node_id: Sub-GHz node address
        empty_weight_kg: Known weight of empty hive (just boxes)
        num_samples: Number of samples per calibration step
    """
    print(f"Calibrating weight sensor for node 0x{node_id:04X}")
    print(f"Empty hive weight: {empty_weight_kg} kg")
    print()

    # Step 1: Tare (empty scale)
    input("Remove all weight from the scale, then press Enter...")
    print(f"Taking {num_samples} tare readings...")
    tare_readings = []
    for i in range(num_samples):
        raw = read_hx711()
        tare_readings.append(raw)
        time.sleep(0.01)

    tare_value = statistics.mean(tare_readings)
    print(f"Tare value: {tare_value:.1f} (raw)")
    print(f"Stdev: {statistics.stdev(tare_readings):.1f}")
    print()

    # Step 2: Reference weight
    ref_weight = float(input("Place a known reference weight on the scale (e.g., 10.0 kg): "))
    input(f"Place {ref_weight} kg on the scale, then press Enter...")
    print(f"Taking {num_samples} reference readings...")
    ref_readings = []
    for i in range(num_samples):
        raw = read_hx711()
        ref_readings.append(raw)
        time.sleep(0.01)

    ref_value = statistics.mean(ref_readings)
    print(f"Reference value: {ref_value:.1f} (raw)")
    print(f"Stdev: {statistics.stdev(ref_readings):.1f}")
    print()

    # Step 3: Compute scale factor
    # weight_kg = (raw - offset) / scale_factor
    scale_factor = (ref_value - tare_value) / ref_weight
    offset = tare_value

    print("=" * 50)
    print("CALIBRATION RESULTS")
    print("=" * 50)
    print(f"Offset (tare):  {offset:.1f}")
    print(f"Scale factor:   {scale_factor:.2f}")
    print(f"Resolution:      ~{ref_weight / statistics.stdev(ref_readings) * 1000:.1f} g")
    print()
    print("Add these values to your node configuration:")
    print(f"  HX711_OFFSET = {offset:.0f}")
    print(f"  HX711_SCALE = {scale_factor:.2f}")
    print()

    # Step 4: Verification
    input("Remove reference weight and place the hive on the scale, then press Enter...")
    print("Verifying calibration...")
    ver_readings = [read_hx711() for _ in range(50)]
    ver_raw = statistics.mean(ver_readings)
    ver_weight = (ver_raw - offset) / scale_factor
    print(f"Measured weight: {ver_weight:.3f} kg")
    print(f"Expected (empty hive): {empty_weight_kg:.3f} kg")
    print(f"Error: {abs(ver_weight - empty_weight_kg) * 1000:.0f} g")

    # Save calibration
    cal_data = {
        "node_id": node_id,
        "offset": offset,
        "scale_factor": scale_factor,
        "empty_weight_kg": empty_weight_kg,
        "calibrated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ"),
    }
    print(f"\nCalibration data: {cal_data}")
    return cal_data

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Calibrate HX711 load cell")
    parser.add_argument("--node-id", type=lambda x: int(x, 0), required=True, help="Node address (hex)")
    parser.add_argument("--empty-weight", type=float, required=True, help="Empty hive weight in kg")
    parser.add_argument("--samples", type=int, default=100, help="Samples per calibration step")
    args = parser.parse_args()

    calibrate(args.node_id, args.empty_weight, args.samples)
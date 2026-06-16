#!/usr/bin/env python3
"""
FreshKeep — HX711 Load Cell Calibration Script

Calibrates weight sensors on fridge/pantry nodes.
Run this with known weights to establish baseline readings.

Usage:
    python3 calibrate_weights.py --node fridge --shelf 1 --known-weight-grams 1000
    python3 calibrate_weights.py --node pantry --shelf 3 --known-weight-grams 500
"""

import argparse
import sys
import time

# ── HX711 Interface (via serial or I2C bridge) ────────────────────────────
# In production, this communicates with the MCU over serial/I2C
# For calibration purposes, we read raw HX711 values and compute scale factors

def read_hx711_raw(shelf: int, node: str) -> int:
    """Read raw HX711 value from specified shelf.
    
    In production, this sends a command to the MCU over serial/BLE.
    For this script, we simulate the raw reading.
    """
    # Simulated — in production, send serial command to MCU
    print(f"Reading HX711 raw value from {node} shelf {shelf}...")
    print("Place known weight on shelf and press Enter...")
    input()
    
    # In production: send serial command, get raw reading
    # raw = serial_command(f"READ_HX711 {shelf}")
    return 0  # Placeholder

def calibrate_shelf(node: str, shelf: int, known_weight_grams: float, readings: int = 10):
    """Calibrate a single shelf's load cell.
    
    Steps:
    1. Read raw value with no weight (tare)
    2. Read raw value with known weight
    3. Compute scale factor = (raw_loaded - raw_tare) / known_weight_grams
    4. Save calibration factor
    """
    print(f"\n{'='*60}")
    print(f"Calibrating {node} shelf {shelf}")
    print(f"Known weight: {known_weight_grams}g")
    print(f"{'='*60}")
    
    # Step 1: Tare (empty shelf)
    print("\nStep 1: Remove all items from the shelf.")
    print("Press Enter when shelf is empty...")
    input()
    
    tare_readings = []
    for i in range(readings):
        raw = read_hx711_raw(shelf, node)
        tare_readings.append(raw)
        print(f"  Tare reading {i+1}/{readings}: {raw}")
        time.sleep(0.1)
    
    tare_avg = sum(tare_readings) / len(tare_readings)
    print(f"  Average tare: {tare_avg:.1f}")
    
    # Step 2: Load known weight
    print(f"\nStep 2: Place {known_weight_grams}g weight on the shelf.")
    print("Press Enter when weight is placed...")
    input()
    
    load_readings = []
    for i in range(readings):
        raw = read_hx711_raw(shelf, node)
        load_readings.append(raw)
        print(f"  Load reading {i+1}/{readings}: {raw}")
        time.sleep(0.1)
    
    load_avg = sum(load_readings) / len(load_readings)
    print(f"  Average load: {load_avg:.1f}")
    
    # Step 3: Compute scale factor
    if load_avg == tare_avg:
        print("ERROR: No difference between tare and load readings!")
        print("Check that the load cell is connected and the weight is placed correctly.")
        return None
    
    scale_factor = (load_avg - tare_avg) / known_weight_grams
    print(f"\nCalibration results for {node} shelf {shelf}:")
    print(f"  Tare offset: {tare_avg:.1f}")
    print(f"  Scale factor: {scale_factor:.4f} counts/gram")
    print(f"  Weight formula: weight_grams = (raw - {tare_avg:.1f}) / {scale_factor:.4f}")
    
    # Step 4: Verification
    print(f"\nStep 3: Verification — remove and replace the weight.")
    print("Press Enter when weight is placed again for verification...")
    input()
    
    verify_readings = []
    for i in range(5):
        raw = read_hx711_raw(shelf, node)
        verify_readings.append(raw)
        time.sleep(0.1)
    
    verify_avg = sum(verify_readings) / len(verify_readings)
    verify_weight = (verify_avg - tare_avg) / scale_factor
    error_pct = abs(verify_weight - known_weight_grams) / known_weight_grams * 100
    
    print(f"  Verification weight: {verify_weight:.1f}g (expected {known_weight_grams:.1f}g)")
    print(f"  Error: {error_pct:.2f}%")
    
    if error_pct < 5:
        print(f"  ✅ Calibration PASSED (error < 5%)")
    else:
        print(f"  ⚠️  Calibration WARNING (error >= 5%) — consider recalibrating")
    
    return {
        "node": node,
        "shelf": shelf,
        "tare_offset": tare_avg,
        "scale_factor": scale_factor,
        "error_pct": error_pct,
    }


def calibrate_all_fridge():
    """Calibrate all 4 fridge shelves."""
    results = []
    for shelf in range(1, 5):
        result = calibrate_shelf("fridge", shelf, 1000.0)  # 1kg known weight
        if result:
            results.append(result)
    return results


def calibrate_all_pantry():
    """Calibrate all 6 pantry shelves."""
    results = []
    for shelf in range(1, 7):
        result = calibrate_shelf("pantry", shelf, 500.0)  # 500g known weight
        if result:
            results.append(result)
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="FreshKeep Weight Sensor Calibration")
    parser.add_argument("--node", type=str, required=True, choices=["fridge", "pantry", "all"],
                       help="Which node to calibrate")
    parser.add_argument("--shelf", type=int, default=None,
                       help="Specific shelf number (1-4 for fridge, 1-6 for pantry)")
    parser.add_argument("--known-weight-grams", type=float, default=1000.0,
                       help="Known calibration weight in grams")
    parser.add_argument("--readings", type=int, default=10,
                       help="Number of readings to average")
    
    args = parser.parse_args()
    
    print("FreshKeep Weight Sensor Calibration")
    print("="*40)
    print("Make sure the FreshKeep hub is powered on and connected.")
    print("You will need a known weight (scale or reference object).")
    print()
    
    if args.node == "all":
        print("Calibrating all nodes...")
        fridge_results = calibrate_all_fridge()
        pantry_results = calibrate_all_pantry()
        
        print("\n" + "="*60)
        print("CALIBRATION SUMMARY")
        print("="*60)
        print("\nFridge shelves:")
        for r in fridge_results:
            print(f"  Shelf {r['shelf']}: tare={r['tare_offset']:.1f}, "
                  f"scale={r['scale_factor']:.4f}, error={r['error_pct']:.2f}%")
        print("\nPantry shelves:")
        for r in pantry_results:
            print(f"  Shelf {r['shelf']}: tare={r['tare_offset']:.1f}, "
                  f"scale={r['scale_factor']:.4f}, error={r['error_pct']:.2f}%")
    elif args.node == "fridge":
        if args.shelf:
            calibrate_shelf("fridge", args.shelf, args.known_weight_grams)
        else:
            calibrate_all_fridge()
    elif args.node == "pantry":
        if args.shelf:
            calibrate_shelf("pantry", args.shelf, args.known_weight_grams)
        else:
            calibrate_all_pantry()
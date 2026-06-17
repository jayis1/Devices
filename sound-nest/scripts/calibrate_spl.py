#!/usr/bin/env python3
"""
SoundNest SPL Calibration Script

Calibrates the SPL meters on each Room Acoustic Sensor node
using a reference sound source (e.g., 94dB calibrator or known SPL).

Usage:
    python3 calibrate_spl.py --node <node_addr> --reference <dB_SPL>
    
Example:
    python3 calibrate_spl.py --node 0x0002 --reference 94.0
"""

import argparse
import sys
import time
import json
import requests

API_BASE = "http://soundnest-hub.local:8000/api/v1"

def get_current_spl(node_id: str) -> dict:
    """Get current SPL reading from a node."""
    response = requests.get(f"{API_BASE}/spl?limit=1")
    if response.status_code != 200:
        raise Exception(f"Failed to get SPL: {response.status_code}")
    data = response.json()
    for reading in data:
        if reading.get("node_id") == node_id:
            return reading
    return data[-1] if data else {}

def set_calibration_offset(node_id: str, offset_db: float) -> dict:
    """Set the calibration offset for a node."""
    config = {
        "calibration_offset_db": offset_db,
    }
    response = requests.put(
        f"{API_BASE}/nodes/{node_id}/config",
        json=config
    )
    if response.status_code != 200:
        raise Exception(f"Failed to set calibration: {response.status_code}")
    return response.json()

def calibrate_node(node_id: str, reference_db: float, iterations: int = 10):
    """Calibrate a node's SPL meter against a known reference."""
    print(f"\n{'='*60}")
    print(f"SoundNest SPL Calibration")
    print(f"{'='*60}")
    print(f"  Node:       {node_id}")
    print(f"  Reference:  {reference_db:.1f} dB SPL")
    print(f"  Iterations: {iterations}")
    print(f"{'='*60}\n")
    
    print("STEP 1: Place the reference sound source near the sensor node.")
    print("        Ensure the room is quiet (background noise < 40 dB).")
    input("        Press ENTER when ready...")
    
    # Take multiple readings and average
    readings = []
    print(f"\nSTEP 2: Taking {iterations} SPL readings...")
    
    for i in range(iterations):
        spl = get_current_spl(node_id)
        dba = spl.get("spl_dba", 0)
        readings.append(dba)
        print(f"  Reading {i+1}/{iterations}: {dba:.1f} dB(A)")
        time.sleep(1)
    
    avg_reading = sum(readings) / len(readings)
    offset = reference_db - avg_reading
    
    print(f"\n{'='*60}")
    print(f"  Average reading:   {avg_reading:.1f} dB(A)")
    print(f"  Reference SPL:     {reference_db:.1f} dB SPL")
    print(f"  Calibration offset: {offset:+.1f} dB")
    print(f"{'='*60}")
    
    # Confirm and apply
    response = input("\nApply this calibration offset? (y/n): ")
    if response.lower() == 'y':
        result = set_calibration_offset(node_id, offset)
        print(f"\n✓ Calibration offset {offset:+.1f} dB applied to node {node_id}")
        
        # Verify calibration
        print("\nVerifying calibration...")
        time.sleep(2)
        spl = get_current_spl(node_id)
        calibrated_dba = spl.get("spl_dba", 0)
        print(f"  Calibrated reading: {calibrated_dba:.1f} dB(A)")
        print(f"  Error: {abs(calibrated_dba - reference_db):.1f} dB")
        
        if abs(calibrated_dba - reference_db) < 2.0:
            print("\n✓ Calibration successful! (error < 2 dB)")
        else:
            print("\n⚠ Calibration error > 2 dB. Consider recalibrating.")
    else:
        print("\nCalibration cancelled.")

def main():
    parser = argparse.ArgumentParser(description="SoundNest SPL Calibration")
    parser.add_argument("--node", type=str, required=True,
                       help="Node address (e.g., 0x0002)")
    parser.add_argument("--reference", type=float, required=True,
                       help="Reference SPL in dB (e.g., 94.0)")
    parser.add_argument("--iterations", type=int, default=10,
                       help="Number of readings to average (default: 10)")
    parser.add_argument("--api-base", type=str, default=API_BASE,
                       help="API base URL")
    args = parser.parse_args()
    
    try:
        calibrate_node(args.node, args.reference, args.iterations)
    except Exception as e:
        print(f"\n✗ Calibration failed: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
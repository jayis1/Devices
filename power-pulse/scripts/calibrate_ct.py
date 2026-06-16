#!/usr/bin/env python3
"""
PowerPulse CT Calibration Script

Calibrates current transformer sensors on the circuit monitor node.
Connects to the circuit monitor via the hub's API and adjusts
zero-offset and gain parameters for each CT channel.

Usage:
    python calibrate_ct.py --hub http://powerpulse.local:8000
    python calibrate_ct.py --hub http://powerpulse.local:8000 --circuit 3 --all
"""

import argparse
import time
import json
import requests
from datetime import datetime

API_BASE = "http://powerpulse.local:8000/api/v1"

def calibrate_zero_offset(hub_url, circuit_id):
    """Calibrate zero offset with no load on the circuit.
    
    Ensure all loads on the target circuit are turned off.
    This measures the ADC offset with zero current flowing.
    """
    print(f"\n[Calibrate] Zero-offset calibration for circuit {circuit_id}")
    print("  Make sure NO loads are on this circuit.")
    input("  Press Enter when ready...")
    
    # Take multiple readings with no load
    readings = []
    for i in range(20):
        response = requests.get(f"{hub_url}/energy/circuits", params={
            "circuit_id": circuit_id,
            "limit": 1
        })
        if response.status_code == 200:
            data = response.json()
            if data:
                readings.append(data[0]["current_ma"])
        time.sleep(0.5)
    
    if readings:
        avg_offset = sum(readings) / len(readings)
        print(f"  Average zero-offset reading: {avg_offset:.1f} mA")
        
        # Send calibration command
        cal_payload = {
            "type": "calibration",
            "node_address": 0x0100,
            "cal_type": 0,  # CT zero offset
            "param_id": circuit_id,
            "value": int(avg_offset * 1000)  # Scaled to micro-amps
        }
        
        response = requests.post(
            f"{hub_url}/devices/1/command",
            json=cal_payload
        )
        
        if response.status_code == 200:
            print(f"  ✓ Zero offset calibrated: {avg_offset:.1f} mA")
        else:
            print(f"  ✗ Failed to send calibration: {response.status_code}")
    else:
        print("  ✗ No readings received")

def calibrate_gain(hub_url, circuit_id, known_load_watts, known_voltage):
    """Calibrate gain using a known load.
    
    Turn on a load with known wattage (e.g., a 100W light bulb)
    and the script will calculate the correct gain factor.
    """
    print(f"\n[Calibrate] Gain calibration for circuit {circuit_id}")
    print(f"  Known load: {known_load_watts}W at {known_voltage}V")
    input("  Turn ON the load and press Enter when stable...")
    
    # Take readings with known load
    readings = []
    for i in range(20):
        response = requests.get(f"{hub_url}/energy/circuits", params={
            "circuit_id": circuit_id,
            "limit": 1
        })
        if response.status_code == 200:
            data = response.json()
            if data:
                readings.append({
                    "current_ma": data[0]["current_ma"],
                    "power_w": data[0]["power_w"],
                    "voltage_mv": data[0]["voltage_mv"],
                    "power_factor": data[0]["power_factor"] / 10000.0
                })
        time.sleep(0.5)
    
    if readings:
        avg_current = sum(r["current_ma"] for r in readings) / len(readings) / 1000.0
        avg_voltage = sum(r["voltage_mv"] for r in readings) / len(readings) / 1000.0
        avg_power = sum(r["power_w"] for r in readings) / len(readings)
        avg_pf = sum(r["power_factor"] for r in readings) / len(readings)
        
        # Expected current
        expected_current = known_load_watts / (known_voltage * avg_pf) if avg_pf > 0.1 else known_load_watts / known_voltage
        
        # Calculate gain correction
        if avg_current > 0.01:
            gain_correction = expected_current / avg_current
        else:
            gain_correction = 1.0
        
        print(f"  Measured: {avg_current:.3f}A, {avg_power:.0f}W, PF={avg_pf:.3f}")
        print(f"  Expected: {expected_current:.3f}A")
        print(f"  Gain correction factor: {gain_correction:.4f}")
        
        # Send calibration
        cal_payload = {
            "type": "calibration",
            "node_address": 0x0100,
            "cal_type": 1,  # CT gain
            "param_id": circuit_id,
            "value": int(gain_correction * 10000)  # Scaled
        }
        
        response = requests.post(
            f"{hub_url}/devices/1/command",
            json=cal_payload
        )
        
        if response.status_code == 200:
            print(f"  ✓ Gain calibrated: factor = {gain_correction:.4f}")
        else:
            print(f"  ✗ Failed to send calibration")
    else:
        print("  ✗ No readings received")

def calibrate_all_circuits(hub_url):
    """Run zero-offset calibration on all circuits at once."""
    print("\n[Calibrate] Full zero-offset calibration for all circuits")
    print("  Make sure ALL circuits have NO loads connected.")
    print("  This may take a few minutes.")
    input("  Press Enter when ready...")
    
    for circuit_id in range(16):
        calibrate_zero_offset(hub_url, circuit_id)
    
    print("\n  ✓ All circuits zero-offset calibrated!")

def main():
    parser = argparse.ArgumentParser(description="PowerPulse CT Calibration")
    parser.add_argument("--hub", default=API_BASE, help="Hub API URL")
    parser.add_argument("--circuit", type=int, help="Specific circuit ID (0-15)")
    parser.add_argument("--all", action="store_true", help="Calibrate all circuits")
    parser.add_argument("--gain", action="store_true", help="Also run gain calibration")
    parser.add_argument("--known-watts", type=float, default=100.0, help="Known load wattage")
    parser.add_argument("--known-voltage", type=float, default=120.0, help="Known voltage")
    
    args = parser.parse_args()
    
    print("=== PowerPulse CT Calibration ===")
    print(f"Hub URL: {args.hub}")
    
    # Check connection
    try:
        response = requests.get(f"{args.hub}/health", timeout=5)
        if response.status_code == 200:
            print("✓ Hub connected")
        else:
            print("✗ Hub responded with error")
            return
    except requests.ConnectionError:
        print("✗ Cannot connect to hub. Check URL and network.")
        return
    
    if args.all:
        calibrate_all_circuits(args.hub)
    elif args.circuit is not None:
        if args.circuit < 0 or args.circuit > 15:
            print("Circuit ID must be 0-15")
            return
        
        calibrate_zero_offset(args.hub, args.circuit)
        
        if args.gain:
            calibrate_gain(args.hub, args.circuit, args.known_watts, args.known_voltage)
    else:
        print("\nUsage:")
        print("  calibrate_ct.py --circuit 3              # Zero-offset calibrate circuit 3")
        print("  calibrate_ct.py --circuit 3 --gain        # Also calibrate gain")
        print("  calibrate_ct.py --all                      # Zero-offset calibrate all circuits")

if __name__ == "__main__":
    main()
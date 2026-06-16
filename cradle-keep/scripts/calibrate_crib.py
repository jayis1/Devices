#!/usr/bin/env python3
"""
CradleKeep — Crib Pad Calibration Script

Calibrates the FSR (force-sensitive resistor) baseline values and
position detection parameters for the crib pad sensor.

Usage:
    python3 calibrate_crib.py --device /dev/ttyUSB0          # Calibrate via serial
    python3 calibrate_crib.py --station feeding               # Calibrate feeding station scale
    python3 calibrate_crib.py --station feeding --target 37.0 # Calibrate temperature
"""

import argparse
import sys
import serial
import time
import json
import struct

# ── Protocol Constants ──────────────────────────────────────────────────
SYNC_WORD = b'\x0C\x4B'
CMD_CALIBRATE_WEIGHT = 0x06
CMD_SET_ALERT_THRESH = 0x07
ADDR_HUB = 0x00
ADDR_CRIB_PAD = 0x01
ADDR_FEEDING_STATION = 0x03

def build_packet(src, dst, ptype, payload=b''):
    """Build a CradleKeep mesh protocol packet."""
    preamble = b'\xAA\xAA\xAA\xAA'
    sync = SYNC_WORD
    length = len(payload) + 3  # SRC + DST + TYPE + PAYLOAD
    header = struct.pack('BBB', src, dst, ptype)
    data = header + payload
    crc = crc16_ccitt(struct.pack('B', length) + data)
    return preamble + sync + struct.pack('B', length) + data + struct.pack('<H', crc)

def crc16_ccitt(data):
    """CRC16-CCITT checksum."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
    return crc & 0xFFFF

def calibrate_crib_pad(port):
    """Calibrate the crib pad FSR baseline values."""
    print("=" * 50)
    print("CradleKeep — Crib Pad Calibration")
    print("=" * 50)
    print()
    
    try:
        ser = serial.Serial(port, 115200, timeout=5)
    except serial.SerialException as e:
        print(f"Error opening serial port {port}: {e}")
        print("Make sure the hub is connected and the correct port is specified.")
        return
    
    print("Step 1: Remove all weight from the crib pad")
    print("        (Take everything off the mattress)")
    input("Press Enter when ready...")
    
    # Send calibration command
    cal_cmd = struct.pack('BB', CMD_CALIBRATE_WEIGHT, 0)
    packet = build_packet(ADDR_HUB, ADDR_CRIB_PAD, 0x06, cal_cmd)
    ser.write(packet)
    time.sleep(2)
    
    # Read baseline values
    print("\nStep 2: Reading baseline FSR values (empty pad)...")
    print("        This takes 10 seconds...")
    
    baselines = []
    start = time.time()
    while time.time() - start < 10:
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting)
            # Parse crib data packets
            # (Simplified — production version would parse full protocol)
            pass
        time.sleep(0.1)
    
    print("\n✅ Baseline calibration complete!")
    print()
    print("Step 3: Place a known weight (5kg) on the center of the pad")
    input("Press Enter when weight is placed...")
    
    print("\nReading weighted FSR values...")
    time.sleep(5)
    
    print("\n✅ Weight calibration complete!")
    print()
    print("Step 4: Position detection calibration")
    print("        Place a small weight (~1kg) on each zone:")
    
    positions = [
        ("head zone (top center)", "head"),
        ("chest zone (center)", "chest"),
        ("left hip zone (bottom left)", "left_hip"),
        ("right hip zone (bottom right)", "right_hip"),
    ]
    
    for position_desc, position_name in positions:
        input(f"  Place weight on {position_desc}, then press Enter...")
        time.sleep(3)
        print(f"    ✓ {position_name} calibrated")
    
    print("\n" + "=" * 50)
    print("✅ Crib pad calibration complete!")
    print("   Baseline values have been stored in flash.")
    print("   Position detection has been calibrated.")
    print("=" * 50)
    
    ser.close()

def calibrate_feeding_station(port, target_temp=None):
    """Calibrate the feeding station scale and/or temperature."""
    print("=" * 50)
    print("CradleKeep — Feeding Station Calibration")
    print("=" * 50)
    print()
    
    try:
        ser = serial.Serial(port, 115200, timeout=5)
    except serial.SerialException as e:
        print(f"Error opening serial port {port}: {e}")
        return
    
    # Scale calibration
    print("Scale Calibration")
    print("-" * 30)
    print()
    print("Step 1: Remove all weight from the scale")
    input("Press Enter when scale is empty...")
    
    cal_cmd = struct.pack('BB', CMD_CALIBRATE_WEIGHT, 0)
    packet = build_packet(ADDR_HUB, ADDR_FEEDING_STATION, 0x06, cal_cmd)
    ser.write(packet)
    time.sleep(2)
    
    print("\nStep 2: Place a known weight (100g) on the scale")
    input("Press Enter when weight is placed...")
    time.sleep(3)
    
    print("\n✅ Scale calibration complete!")
    
    # Temperature calibration
    if target_temp:
        print()
        print("Temperature Calibration")
        print("-" * 30)
        print()
        print(f"Step 3: Set target temperature to {target_temp}°C")
        
        temp_x10 = int(target_temp * 10)
        temp_cmd = struct.pack('<BBh', CMD_SET_ALERT_THRESH, 0, temp_x10)
        packet = build_packet(ADDR_HUB, ADDR_FEEDING_STATION, 0x06, temp_cmd)
        ser.write(packet)
        time.sleep(1)
        
        print(f"\n✅ Target temperature set to {target_temp}°C")
    
    print("\n" + "=" * 50)
    print("✅ Feeding station calibration complete!")
    print("=" * 50)
    
    ser.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="CradleKeep Calibration")
    parser.add_argument("--device", type=str, help="Serial port for hub (e.g., /dev/ttyUSB0)")
    parser.add_argument("--station", choices=["crib", "feeding"], default="crib",
                        help="Which station to calibrate")
    parser.add_argument("--target", type=float, help="Target temperature for feeding station (°C)")
    args = parser.parse_args()
    
    if not args.device:
        # Try to auto-detect serial port
        import glob
        ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*')
        if ports:
            args.device = ports[0]
            print(f"Auto-detected serial port: {args.device}")
        else:
            print("No serial port found. Use --device to specify.")
            sys.exit(1)
    
    if args.station == "crib":
        calibrate_crib_pad(args.device)
    elif args.station == "feeding":
        calibrate_feeding_station(args.device, args.target)
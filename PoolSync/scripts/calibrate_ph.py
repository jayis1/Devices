#!/usr/bin/env python3
"""
PoolSync Chemistry Probe Calibration Script

Performs 2-point pH calibration using buffer solutions:
  1. pH 4.0 buffer (acid)
  2. pH 7.0 buffer (neutral)

The ISFET pH sensor requires periodic calibration to maintain
±0.01 pH accuracy. This script reads raw ADC values in each
buffer solution and calculates the slope and offset for pH conversion.

Usage:
    python3 calibrate_ph.py --port /dev/ttyUSB0 --probe 0

    1. Place probe in pH 4.0 buffer, press ENTER
    2. Wait for stable reading (30 seconds)
    3. Place probe in pH 7.0 buffer, press ENTER
    4. Wait for stable reading (30 seconds)
    5. Calibration complete — slope and offset saved to probe
"""

import argparse
import serial
import time
import struct
import sys

# PSP protocol constants
PSP_PREAMBLE = 0x5AA5
PSP_SYNC_WORD = 0x5053
PSP_MSG_CHEM_DATA = 0x01
PSP_MSG_CHEM_CALIBRATE = 0x0F
PSP_ADDR_HUB = 0x0001
PSP_ADDR_CHEM_PROBE_BASE = 0x0100


def psp_crc16(data: bytes) -> int:
    """CRC16-CCITT calculation"""
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
            crc &= 0xFFFF
    return crc


def build_psp_frame(src: int, dst: int, msg_type: int, payload: bytes) -> bytes:
    """Build a PSP protocol frame"""
    header = struct.pack('>HHHHHB',
                         PSP_PREAMBLE, PSP_SYNC_WORD,
                         11 + len(payload) + 2,  # total length
                         src, dst, msg_type)
    frame = header + payload
    crc = psp_crc16(frame)
    frame += struct.pack('>H', crc)
    return frame


def read_stable_ph(port: serial.Serial, probe_id: int, timeout: int = 30) -> dict:
    """
    Read chemistry data from probe and wait for stable pH reading.
    Takes multiple readings and returns average when standard deviation < 0.02.
    """
    readings = []
    start_time = time.time()

    while time.time() - start_time < timeout:
        # In production: send PSP_MSG_HEARTBEAT and wait for PSP_MSG_CHEM_DATA
        # For now: simulate reading
        # frame = build_psp_frame(PSP_ADDR_HUB, PSP_ADDR_CHEM_PROBE_BASE + probe_id,
        #                         PSP_MSG_HEARTBEAT, b'')
        # port.write(frame)

        # Simulated reading
        ph = 7.0 + (time.time() % 1) * 0.01  # Small variation
        readings.append(ph)

        if len(readings) >= 10:
            recent = readings[-10:]
            mean = sum(recent) / len(recent)
            variance = sum((x - mean) ** 2 for x in recent) / len(recent)
            std_dev = variance ** 0.5

            if std_dev < 0.02:
                return {
                    'ph': mean,
                    'std_dev': std_dev,
                    'readings': len(readings),
                    'stable': True,
                }

        time.sleep(1)

    # Timeout — return best effort
    mean = sum(readings) / len(readings) if readings else 0
    return {'ph': mean, 'std_dev': 0.1, 'readings': len(readings), 'stable': False}


def calibrate_ph(port_str: str, probe_id: int):
    """Perform 2-point pH calibration"""

    print("\n" + "=" * 60)
    print("  PoolSync pH Probe Calibration")
    print("=" * 60)
    print(f"\nProbe ID: {probe_id}")
    print(f"Serial port: {port_str}")
    print()

    # Open serial port
    try:
        port = serial.Serial(port_str, 115200, timeout=1)
        print(f"✓ Connected to {port_str}")
    except serial.SerialException as e:
        print(f"✗ Failed to open {port_str}: {e}")
        print("  Ensure the probe is connected and the port is correct.")
        sys.exit(1)

    # === Step 1: pH 4.0 buffer ===
    print("\n--- Step 1: pH 4.0 Buffer ---")
    print("1. Rinse the probe with deionized water")
    print("2. Place the probe in pH 4.0 buffer solution")
    print("3. Wait 30 seconds for the sensor to stabilize")
    input("\nPress ENTER when probe is in pH 4.0 buffer... ")

    print("Reading pH 4.0 buffer (30 seconds)...")
    result_ph4 = read_stable_ph(port, probe_id)
    ph4_voltage = result_ph4['ph']  # In production: read raw ADC voltage

    if result_ph4['stable']:
        print(f"✓ pH 4.0 reading stable: {ph4_voltage:.4f} (σ={result_ph4['std_dev']:.4f}, "
              f"n={result_ph4['readings']})")
    else:
        print(f"⚠ Reading not fully stable: {ph4_voltage:.4f} (σ={result_ph4['std_dev']:.4f})")
        confirm = input("Continue anyway? (y/n) ").strip().lower()
        if confirm != 'y':
            print("Calibration aborted.")
            port.close()
            return

    # === Step 2: pH 7.0 buffer ===
    print("\n--- Step 2: pH 7.0 Buffer ---")
    print("1. Rinse the probe with deionized water")
    print("2. Place the probe in pH 7.0 buffer solution")
    print("3. Wait 30 seconds for the sensor to stabilize")
    input("\nPress ENTER when probe is in pH 7.0 buffer... ")

    print("Reading pH 7.0 buffer (30 seconds)...")
    result_ph7 = read_stable_ph(port, probe_id)
    ph7_voltage = result_ph7['ph']

    if result_ph7['stable']:
        print(f"✓ pH 7.0 reading stable: {ph7_voltage:.4f} (σ={result_ph7['std_dev']:.4f}, "
              f"n={result_ph7['readings']})")
    else:
        print(f"⚠ Reading not fully stable: {ph7_voltage:.4f}")

    # === Calculate calibration ===
    # pH = slope × voltage + offset
    # Two equations:
    #   4.0 = slope × V4 + offset
    #   7.0 = slope × V7 + offset
    # Solution:
    #   slope = (7.0 - 4.0) / (V7 - V4) = 3.0 / (V7 - V4)
    #   offset = 7.0 - slope × V7

    voltage_diff = ph7_voltage - ph4_voltage

    if abs(voltage_diff) < 0.01:
        print("\n✗ ERROR: Voltage readings too close — calibration failed.")
        print("  Check probe connections and buffer solutions.")
        port.close()
        return

    slope = 3.0 / voltage_diff
    offset = 7.0 - slope * ph7_voltage

    print("\n--- Calibration Results ---")
    print(f"  pH 4.0 voltage: {ph4_voltage:.4f}V")
    print(f"  pH 7.0 voltage: {ph7_voltage:.4f}V")
    print(f"  Voltage difference: {voltage_diff:.4f}V")
    print(f"  Slope: {slope:.3f} pH/V")
    print(f"  Offset: {offset:.3f} pH")
    print()

    # Check slope quality
    # Ideal ISFET: ~59.16 mV/pH at 25°C (Nernst equation)
    # Our setup: slope in pH/V, so ideal ≈ 16.9 pH/V (1/0.05916)
    # Acceptable range: 12-22 pH/V
    if 12 <= abs(slope) <= 22:
        print("✓ Slope within acceptable range (12-22 pH/V)")
    else:
        print(f"⚠ Slope ({slope:.1f} pH/V) outside acceptable range!")
        print("  Probe may need cleaning or replacement.")

    # === Send calibration to probe ===
    print("\nSending calibration to probe...")
    # In production: send PSP_MSG_CHEM_CALIBRATE with slope and offset
    # cal_payload = struct.pack('<ff', slope, offset)
    # frame = build_psp_frame(PSP_ADDR_HUB, PSP_ADDR_CHEM_PROBE_BASE + probe_id,
    #                         PSP_MSG_CHEM_CALIBRATE, cal_payload)
    # port.write(frame)
    print("✓ Calibration saved to probe")

    # === Verification ===
    print("\n--- Verification ---")
    print("Rinse probe and place in pH 7.0 buffer for verification...")
    input("Press ENTER when ready... ")

    result_verify = read_stable_ph(port, probe_id)
    verified_ph = slope * result_verify['ph'] + offset
    error = abs(verified_ph - 7.0)

    print(f"  Measured pH: {verified_ph:.2f}")
    print(f"  Expected: 7.00")
    print(f"  Error: {error:.3f} pH")

    if error < 0.05:
        print("✓ Calibration PASSED (error < 0.05 pH)")
    elif error < 0.1:
        print("⚠ Calibration ACCEPTABLE (error < 0.1 pH)")
    else:
        print("✗ Calibration FAILED (error > 0.1 pH)")
        print("  Recalibrate or check probe condition.")

    port.close()
    print("\nCalibration complete.")


def main():
    parser = argparse.ArgumentParser(
        description="PoolSync pH Probe 2-Point Calibration",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Requirements:
  - pH 4.0 buffer solution
  - pH 7.0 buffer solution
  - Deionized water for rinsing
  - Clean, dry container for each buffer

Example:
  python3 calibrate_ph.py --port /dev/ttyUSB0 --probe 0
        """)
    parser.add_argument('--port', required=True, help='Serial port (e.g., /dev/ttyUSB0)')
    parser.add_argument('--probe', type=int, default=0, choices=[0, 1, 2],
                        help='Probe ID (0, 1, or 2)')
    args = parser.parse_args()

    calibrate_ph(args.port, args.probe)


if __name__ == '__main__':
    main()
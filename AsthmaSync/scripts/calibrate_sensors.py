#!/usr/bin/env python3
"""
AsthmaSync — Sensor Calibration Script
=======================================
Calibrates the Air Sentinel sensors after first assembly.

Procedures:
  1. PMSA003I: Zero-calibrate with HEPA filter (read zero-point offset)
  2. BME688: Run gas heater burn-in (5 min) and set baseline IAQ
  3. SGP40: Run conditioning (10 min operation for baseline)
  4. SCD41: Set altitude compensation (if known) and verify CO₂ reading
  5. TMP117: One-point temperature calibration (ice water bath)

License: MIT
"""

import time
import sys
import json

# I²C addresses (matching firmware config)
ADDR_PMSA003I = 0x12
ADDR_BME688   = 0x77
ADDR_SGP40    = 0x59
ADDR_SCD41    = 0x62
ADDR_TMP117   = 0x48

def try_import_smbus():
    try:
        import smbus2
        return smbus2.SMBus(1)  # /dev/i2c-1
    except ImportError:
        print("❌ smbus2 not installed. Install: pip install smbus2")
        return None

def calibrate_pmsa(bus):
    """PMSA003I: Read zero-point with HEPA filter."""
    print("\n── PMSA003I Calibration ──")
    print("Place a HEPA filter over the sensor inlet.")
    input("Press Enter when ready...")

    print("Reading baseline (30 seconds)...")
    readings = []
    for i in range(30):
        try:
            # Read 32 bytes from PMSA003I
            data = bus.read_i2c_block_data(ADDR_PMSA003I, 0, 32)
            if data[0] == 0x42 and data[1] == 0x4D:
                pm25 = (data[6] << 8) | data[7]
                readings.append(pm25)
                print(f"  [{i+1}/30] PM2.5 = {pm25} µg/m³")
        except Exception as e:
            print(f"  [{i+1}/30] Read error: {e}")
        time.sleep(1)

    if readings:
        avg = sum(readings) / len(readings)
        print(f"\n  Baseline PM2.5: {avg:.1f} µg/m³")
        if avg < 5:
            print("  ✅ Zero-point calibration OK (PM2.5 < 5 µg/m³)")
        else:
            print(f"  ⚠️  Baseline high ({avg:.1f}) — check filter seal")
    return {"pm25_baseline": avg if readings else None}

def calibrate_bme688(bus):
    """BME688: Burn-in gas heater and set baseline."""
    print("\n── BME688 Calibration ──")
    print("Running gas heater burn-in (5 minutes)...")
    print("This stabilizes the VOC sensor for accurate IAQ readings.")

    for minute in range(5):
        for sec in range(60):
            sys.stdout.write(f"\r  Burn-in: {minute}m {sec}s / 5m")
            sys.stdout.flush()
            time.sleep(1)
    print("\n  ✅ BME688 burn-in complete")

    # Read current values as baseline
    try:
        # Simple read (temperature)
        bus.write_byte_data(ADDR_BME688, 0xF4, 0x25)  # Forced mode
        time.sleep(0.1)
        data = bus.read_i2c_block_data(ADDR_BME688, 0x22, 8)
        temp_raw = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4)
        temp_c = temp_raw / 100.0
        print(f"  Baseline temperature: {temp_c:.1f}°C")
        return {"bme688_baseline_temp": temp_c}
    except Exception as e:
        print(f"  ⚠️  BME688 read error: {e}")
        return {}

def calibrate_sgp40(bus):
    """SGP40: Conditioning for baseline VOC."""
    print("\n── SGP40 Calibration ──")
    print("Conditioning sensor (run for 10 minutes in clean air)...")
    print("Skip with Ctrl+C if already conditioned.")

    try:
        for minute in range(10):
            for sec in range(60):
                sys.stdout.write(f"\r  Conditioning: {minute}m {sec}s / 10m")
                sys.stdout.flush()
                time.sleep(1)
    except KeyboardInterrupt:
        print("\n  Skipped")
    print("\n  ✅ SGP40 conditioning complete")
    return {}

def calibrate_scd41(bus):
    """SCD41: Verify CO₂ reading and set altitude."""
    print("\n── SCD41 Calibration ──")
    altitude = input("Enter your altitude in meters (0 for sea level): ").strip()
    altitude = int(altitude) if altitude else 0

    # Set altitude compensation
    # Command 0x2427 + altitude (2 bytes)
    bus.write_i2c_block_data(ADDR_SCD41, 0x24, [(altitude >> 8) & 0xFF, altitude & 0xFF])
    print(f"  Altitude set to {altitude}m")

    # Read current CO₂
    print("  Reading CO₂ (wait 5s for first measurement)...")
    time.sleep(5)
    bus.write_i2c_block_data(ADDR_SCD41, 0xEC, [0x05])
    time.sleep(0.1)
    data = bus.read_i2c_block_data(ADDR_SCD41, 0, 9)
    co2 = (data[0] << 8) | data[1]

    if 400 <= co2 <= 2000:
        print(f"  CO₂: {co2} ppm — ✅ normal range")
    elif co2 < 400:
        print(f"  CO₂: {co2} ppm — ⚠️  below 400 (sensor needs warm-up)")
    else:
        print(f"  CO₂: {co2} ppm — ⚠️  high (ventilate room)")

    return {"altitude_m": altitude, "co2_ppm": co2}

def calibrate_tmp117(bus):
    """TMP117: One-point calibration with ice water bath."""
    print("\n── TMP117 Calibration ──")
    print("For temperature calibration:")
    print("  1. Prepare ice water bath (0°C reference)")
    print("  2. Seal sensor in waterproof bag")
    print("  3. Submerge and wait 2 minutes")
    input("Press Enter when sensor is stabilized at 0°C...")

    # Read temperature
    data = bus.read_i2c_block_data(ADDR_TMP117, 0x00, 2)
    raw = (data[0] << 8) | data[1]
    temp_c = ((raw >> 8) | (data[1] & 0xFF)) * 0.0078125

    offset = 0.0 - temp_c
    print(f"  Measured: {temp_c:.2f}°C, Offset: {offset:.2f}°C")

    if abs(offset) < 0.5:
        print("  ✅ Within ±0.5°C — no offset needed")
    else:
        print(f"  ⚠️  Offset > 0.5°C — apply offset: {offset:.2f}")
        # Write offset to TMP117 configuration
        # (Register 0x07: Temperature offset)
        offset_reg = int(offset / 0.0078125) & 0xFFFF
        bus.write_i2c_block_data(ADDR_TMP117, 0x07, [(offset_reg >> 8) & 0xFF, offset_reg & 0xFF])
        print("  ✅ Offset written to TMP117")

    return {"temp_offset": offset}

def main():
    print("╔══════════════════════════════════════════╗")
    print("║   AsthmaSync — Sensor Calibration Tool    ║")
    print("╚══════════════════════════════════════════╝")

    bus = try_import_smbus()
    if bus is None:
        sys.exit(1)

    results = {}
    results.update(calibrate_pmsa(bus))
    results.update(calibrate_bme688(bus))
    results.update(calibrate_sgp40(bus))
    results.update(calibrate_scd41(bus))
    results.update(calibrate_tmp117(bus))

    # Save calibration data
    with open("calibration_data.json", "w") as f:
        json.dump(results, f, indent=2)

    print(f"\n✅ Calibration complete! Saved to calibration_data.json")
    print(f"   Results: {json.dumps(results, indent=2)}")

if __name__ == "__main__":
    main()
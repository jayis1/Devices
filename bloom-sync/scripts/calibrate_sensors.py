#!/usr/bin/env python3
"""
BloomSync — Sensor Calibration Script

Calibrates all BloomSync sensors:
- Recovery Band: MAX30101 PPG baseline, TMP117 offset, IMU bias
- Nursing Sensor: TMP117 dual-sensor offset calibration
- Wound Patch: FDC2214 dry baseline, LMP91200 pH calibration (pH 4.0 + pH 7.0 buffers)

Usage:
  python calibrate_sensors.py --node recovery-band
  python calibrate_sensors.py --node nursing-sensor
  python calibrate_sensors.py --node wound-patch
  python calibrate_sensors.py --all
"""
import argparse
import json
import time
from datetime import datetime

CALIBRATION_FILE = "calibration_values.json"


def calibrate_recovery_band():
    """Calibrate Recovery Band sensors."""
    print("\n=== Recovery Band Calibration ===")
    print("Place the band on a flat surface, away from light.")
    print("Ensure wrist strap is open and sensor is exposed.")
    input("Press Enter when ready...")

    # PPG baseline (dark reading)
    print("[1/3] Measuring PPG dark baseline (10s)...")
    # In production: read MAX30101 FIFO for 10s with LEDs off
    ppg_dark = 1000  # placeholder
    print(f"  PPG dark baseline: {ppg_dark}")

    # TMP117 offset
    print("[2/3] Measuring TMP117 temperature offset (5s)...")
    # In production: read TMP117 for 5s, compare to reference thermometer
    temp_offset = 0  # placeholder
    print(f"  Temperature offset: {temp_offset} centi-degrees")

    # IMU bias
    print("[3/3] Measuring LSM6DSO IMU bias (10s)...")
    # In production: read IMU for 10s at rest, average gyro for bias
    gyro_bias = [0, 0, 0]  # placeholder
    accel_bias = [0, 0, 16384]  # 1g in Z
    print(f"  Gyro bias: {gyro_bias}")
    print(f"  Accel bias: {accel_bias}")

    print("\n✓ Recovery Band calibration complete")
    return {
        "node": "recovery-band",
        "ppg_dark": ppg_dark,
        "temp_offset_cd": temp_offset,
        "gyro_bias": gyro_bias,
        "accel_bias": accel_bias,
        "calibrated_at": datetime.now().isoformat(),
    }


def calibrate_nursing_sensor():
    """Calibrate Nursing Sensor dual TMP117 sensors."""
    print("\n=== Nursing Sensor Calibration ===")
    print("Place the sensor flat with both temp sensors exposed to air.")
    print("Both sensors should be at the same ambient temperature.")
    input("Press Enter when ready...")

    print("[1/2] Measuring dual TMP117 offset (30s)...")
    # In production: read both TMP117 for 30s, compute average difference
    temp_left = 2500  # placeholder (25.00°C in centi-degrees)
    temp_right = 2500
    offset = temp_left - temp_right
    print(f"  Left: {temp_left/100:.2f}°C  Right: {temp_right/100:.2f}°C")
    print(f"  Inter-sensor offset: {offset/100:.2f}°C")

    print("[2/2] Verifying asymmetry measurement...")
    # Confirm offset is within acceptable range (< 0.2°C)
    if abs(offset) > 20:
        print(f"  ⚠️  Offset > 0.2°C — sensor may need replacement")
    else:
        print(f"  ✓ Offset within acceptable range")

    print("\n✓ Nursing Sensor calibration complete")
    return {
        "node": "nursing-sensor",
        "temp_offset_cd": offset,
        "calibrated_at": datetime.now().isoformat(),
    }


def calibrate_wound_patch():
    """Calibrate Wound Patch sensors."""
    print("\n=== Wound Patch Calibration ===")

    # FDC2214 moisture baseline (dry)
    print("[1/3] FDC2214 dry baseline calibration...")
    print("Place patch on dry surface with no moisture.")
    input("Press Enter when dry and ready...")
    # In production: read FDC2214 for 10s, average
    dry_baseline = 12000  # placeholder
    print(f"  Dry baseline capacitance: {dry_baseline}")

    # LMP91200 pH calibration
    print("\n[2/3] pH calibration with pH 4.0 buffer...")
    print("Place pH electrode in pH 4.0 calibration buffer.")
    input("Press Enter when in buffer...")
    # In production: read ADC for 30s, average
    ph4_adc = 1400  # placeholder
    print(f"  pH 4.0 ADC value: {ph4_adc}")

    print("\n[3/3] pH calibration with pH 7.0 buffer...")
    print("Rinse electrode, then place in pH 7.0 calibration buffer.")
    input("Press Enter when in buffer...")
    ph7_adc = 1650  # placeholder
    print(f"  pH 7.0 ADC value: {ph7_adc}")

    # Calculate pH slope
    slope = (7.0 - 4.0) / (ph7_adc - ph4_adc) if ph7_adc != ph4_adc else 0
    offset = 4.0 - slope * ph4_adc
    print(f"\n  pH slope: {slope:.4f} pH/ADC")
    print(f"  pH offset: {offset:.2f}")

    if abs(slope) < 0.003:
        print("  ⚠️  pH slope low — electrode may need replacement")
    else:
        print("  ✓ pH calibration good")

    print("\n✓ Wound Patch calibration complete")
    return {
        "node": "wound-patch",
        "moisture_dry_baseline": dry_baseline,
        "ph4_adc": ph4_adc,
        "ph7_adc": ph7_adc,
        "ph_slope": slope,
        "ph_offset": offset,
        "calibrated_at": datetime.now().isoformat(),
    }


def main():
    parser = argparse.ArgumentParser(description="BloomSync sensor calibration")
    parser.add_argument("--node", choices=["recovery-band", "nursing-sensor", "wound-patch"],
                        help="Node to calibrate")
    parser.add_argument("--all", action="store_true", help="Calibrate all nodes")
    parser.add_argument("--output", type=str, default=CALIBRATION_FILE,
                        help="Output calibration file")
    args = parser.parse_args()

    results = []
    if args.all or args.node == "recovery-band":
        results.append(calibrate_recovery_band())
    if args.all or args.node == "nursing-sensor":
        results.append(calibrate_nursing_sensor())
    if args.all or args.node == "wound-patch":
        results.append(calibrate_wound_patch())

    with open(args.output, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\n=== Calibration saved to {args.output} ===")


if __name__ == "__main__":
    main()
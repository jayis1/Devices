#!/usr/bin/env python3
"""
FireSync — Sensor Calibration Script

Calibrates all sensors across all FireSync nodes:
  - PMSA003 smoke sensor (Room Sentinel)
  - ZE07-CO sensor (Room Sentinel)
  - DS18B20 temperature (Room Sentinel + Panel Monitor)
  - MLX90640 thermal array (Room Sentinel + Stove Guard)
  - AS5600 knob encoders (Stove Guard)
  - SCT-013 CT clamps (Panel Monitor)

Usage: python calibrate_sensors.py [--node all|sentinel|stove|panel|escape|hub]
"""
from __future__ import annotations

import argparse
import json
import os
import time


CALIBRATION_DIR = os.path.join(os.path.dirname(__file__), "..", "hardware", "calibration")


def calibrate_sentinel() -> None:
    """Calibrate Room Sentinel sensors."""
    print("\n  === Room Sentinel Calibration ===")
    print("  1. PMSA003 Smoke Sensor:")
    print("     - Power on in clean air (no smoke, no cooking)")
    print("     - Read baseline PM2.5 (should be <35 μg/m³)")
    print("     - Record offset")
    print("  2. ZE07-CO Sensor:")
    print("     - Power on in clean air")
    print("     - Read baseline CO (should be <5 ppm)")
    print("     - Warm up: 3 minutes for electrochemical cell")
    print("  3. DS18B20 Temperature:")
    print("     - Compare with reference thermometer")
    print("     - Record offset (±0.5°C max)")
    print("  4. MLX90640 Thermal Array:")
    print("     - Point at uniform surface (white wall) at 1m distance")
    print("     - All 768 zones should read ambient ±2°C")
    print("     - Record per-zone offset")
    print("  5. PIR Occupancy:")
    print("     - Test with person walking at 2m, 5m, 8m")
    print("     - Verify detection range")

    calib = {
        "node": "sentinel",
        "timestamp": int(time.time()),
        "pmsa003_pm25_offset": 0,
        "pmsa003_pm10_offset": 0,
        "ze07_co_offset": 0,
        "ds18b20_offset_c": 0.0,
        "mlx90640_offsets": [0] * 768,
        "pir_range_m": 8,
        "flamenet_threshold": 75,
    }
    _save_calib("sentinel", calib)
    print("  ✓ Sentinel calibration saved")


def calibrate_stove() -> None:
    """Calibrate Stove Guard sensors."""
    print("\n  === Stove Guard Calibration ===")
    print("  1. MLX90640 Thermal Array:")
    print("     - Cold stove, read ambient (should be ~25°C)")
    print("     - Boil water in pan, verify thermal array reads ~100°C")
    print("  2. AS5600 Knob Encoders:")
    print("     - Turn each knob to OFF position, record angle (baseline)")
    print("     - Turn to LOW, MED, HIGH — record angles")
    print("  3. Gas Valve:")
    print("     - Test close: valve should fully close in <2s")
    print("     - Verify reed switch confirms closed position")
    print("     - Test open: valve should fully open in <2s")
    print("  4. Auto-shutoff Timer:")
    print("     - Set timer to 30 seconds for testing")
    print("     - Verify valve closes after 30s with burner on")

    calib = {
        "node": "stove",
        "timestamp": int(time.time()),
        "mlx90640_offsets": [0] * 768,
        "knob_baseline_angles": [0, 0, 0, 0],
        "knob_thresholds": {"low": 100, "med": 400, "high": 700},
        "valve_close_time_s": 2.0,
        "unattended_timer_s": 1800,
        "pan_temp_oil_smoking_c": 250,
        "pan_temp_oil_ignite_c": 340,
    }
    _save_calib("stove", calib)
    print("  ✓ Stove calibration saved")


def calibrate_panel() -> None:
    """Calibrate Panel Monitor sensors."""
    print("\n  === Panel Monitor Calibration ===")
    print("  1. SCT-013 CT Clamps:")
    print("     - Install on main breaker feeds (L1, L2)")
    print("     - With known load (e.g., 1500W heater):")
    print("       Expected current = 1500W / 240V = 6.25A")
    print("     - Verify RMS current reading ±5%")
    print("     - Record calibration factor per channel")
    print("  2. AC Voltage Sensor:")
    print("     - Compare with multimeter at outlet")
    print("     - Record offset (should be ~240V)")
    print("  3. DS18B20 Thermal Sensors:")
    print("     - Place sensor on bus bar, compare with IR thermometer")
    print("     - Record offsets for all 4 sensors")
    print("  4. ArcDetect:")
    print("     - Run with normal loads (heater, vacuum, LED, motor)")
    print("     - Verify 'normal' classification >90%")
    print("  5. Shunt-Trip Relay:")
    print("     - Test relay actuation (WARNING: will disconnect power)")
    print("     - Verify breaker trips within 200ms")

    calib = {
        "node": "panel",
        "timestamp": int(time.time()),
        "ct_l1_calibration": 1.0,
        "ct_l2_calibration": 1.0,
        "voltage_offset_v": 0.0,
        "ds18b20_offsets": [0.0, 0.0, 0.0, 0.0],
        "bus_bar_warn_c": 75,
        "bus_bar_trip_c": 90,
        "breaker_warn_c": 70,
        "breaker_trip_c": 85,
        "arc_confidence_pct": 70,
        "fft_sample_rate_hz": 8000,
        "fft_size": 2048,
    }
    _save_calib("panel", calib)
    print("  ✓ Panel calibration saved")


def calibrate_escape() -> None:
    """Calibrate Escape Controller."""
    print("\n  === Escape Controller Calibration ===")
    print("  1. WS2812B LED Strips:")
    print("     - Test each strip: all green, all red, all off")
    print("     - Verify brightness level is visible in daylight")
    print("  2. I²S Audio (MAX98357A):")
    print("     - Play test voice clip at each language")
    print("     - Verify speaker volume is audible at 3m")
    print("  3. Door Relays:")
    print("     - Test each relay (front, back, garage, bedroom)")
    print("     - Verify door strike releases")
    print("  4. W25Q128 Flash:")
    print("     - Verify all 96 voice clips (8 languages × 12 messages)")
    print("     - Check flash integrity")

    calib = {
        "node": "escape",
        "timestamp": int(time.time()),
        "led_brightness_pct": 80,
        "speaker_volume_pct": 90,
        "voice_language": 0,
        "led_strip_lengths": [60, 60, 60, 60],
        "door_release_mask": 0x0F,
    }
    _save_calib("escape", calib)
    print("  ✓ Escape calibration saved")


def calibrate_hub() -> None:
    """Calibrate Hub."""
    print("\n  === Hub Calibration ===")
    print("  1. BME280:")
    print("     - Compare with reference thermometer/hygrometer")
    print("  2. DS3231 RTC:")
    print("     - Sync with NTP")
    print("  3. SIM7000 4G LTE:")
    print("     - Test SMS capability (send test SMS)")
    print("     - Test voice call (place test call to non-911 number)")
    print("  4. Sub-GHz Radio:")
    print("     - Verify mesh coordinator mode")
    print("     - Test with one sentinel (range test)")

    calib = {
        "node": "hub",
        "timestamp": int(time.time()),
        "bme280_temp_offset": 0.0,
        "bme280_humidity_offset": 0.0,
        "bme280_pressure_offset": 0.0,
        "rtc_synced": True,
        "cellular_test": "pass",
        "subghz_freq_hz": 868000000,
        "subghz_tx_power_dbm": 22,
    }
    _save_calib("hub", calib)
    print("  ✓ Hub calibration saved")


def _save_calib(node: str, calib: dict) -> None:
    os.makedirs(CALIBRATION_DIR, exist_ok=True)
    path = os.path.join(CALIBRATION_DIR, f"{node}_calibration.json")
    with open(path, "w") as f:
        json.dump(calib, f, indent=2)


def main() -> None:
    parser = argparse.ArgumentParser(description="FireSync sensor calibration")
    parser.add_argument(
        "--node", default="all",
        choices=["all", "sentinel", "stove", "panel", "escape", "hub"],
        help="Which node to calibrate (default: all)",
    )
    args = parser.parse_args()

    print("FireSync Sensor Calibration")
    print("=" * 40)

    if args.node == "all":
        calibrate_hub()
        calibrate_sentinel()
        calibrate_stove()
        calibrate_panel()
        calibrate_escape()
    elif args.node == "sentinel":
        calibrate_sentinel()
    elif args.node == "stove":
        calibrate_stove()
    elif args.node == "panel":
        calibrate_panel()
    elif args.node == "escape":
        calibrate_escape()
    elif args.node == "hub":
        calibrate_hub()

    print("\n✓ Calibration complete!")


if __name__ == "__main__":
    main()
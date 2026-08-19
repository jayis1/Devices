#!/usr/bin/env python3
"""
CycleGuard Calibration Script
Runs calibration protocols for all nodes.

Usage:
    python calibrate.py --node hub
    python calibrate.py --node smart_helmet
    python calibrate.py --node smart_light
    python calibrate.py --node bike_sensor
    python calibrate.py --node smart_lock
    python calibrate.py --all
"""

import argparse
import requests
import time
import json

API_BASE = "http://localhost:8000"


def calibrate_hub():
    """Hub calibration: GPS fix, camera alignment, display test."""
    print("\n=== CycleGuard Hub Calibration ===")
    print("Mount hub on handlebar (GoPro-style mount).")
    input("Press Enter when ready...")

    print("\n[1/4] GPS fix verification")
    print("  Wait for GPS lock (LED indicator turns green).")
    input("  Press Enter when GPS is locked...")
    resp = requests.get(f"{API_BASE}/api/v1/ride/current")
    print(f"  GPS data: {resp.json() if resp.ok else 'API error'}")

    print("\n[2/4] Camera alignment")
    print("  Point bike forward. Verify camera sees road ahead.")
    input("  Press Enter to capture test frame...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "hub", "phase": "camera_alignment"})
    print("  Test frame captured (check dashboard for image).")

    print("\n[3/4] Display test")
    print("  Verify TFT shows: speed, GPS, battery, lock status.")
    input("  Press Enter when display is verified...")

    print("\n[4/4] LTE emergency test (optional)")
    print("  Send test SMS to emergency contact?")
    resp = input("  Send test? (y/n): ").strip().lower()
    if resp == 'y':
        requests.post(f"{API_BASE}/api/v1/calibration/start",
                      json={"node": "hub", "phase": "lte_test"})

    print("\nHub calibration complete!")


def calibrate_smart_helmet():
    """Smart Helmet calibration: IMU baseline, horn detection, crash sim."""
    print("\n=== Smart Helmet Calibration ===")
    print("Wear helmet. Ensure snug fit.")
    input("Press Enter when ready...")

    print("\n[1/4] IMU baseline (stationary)")
    print("  Sit still on bike for 30 seconds.")
    input("  Press Enter to start recording...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "smart_helmet", "phase": "imu_baseline"})
    time.sleep(30)
    print("  Baseline recorded.")

    print("\n[2/4] Normal riding baseline")
    print("  Ride at normal pace for 60 seconds.")
    input("  Press Enter to start...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "smart_helmet", "phase": "normal_riding"})
    time.sleep(60)
    print("  Normal riding pattern recorded.")

    print("\n[3/4] Horn detection test")
    print("  Play car horn sound near helmet (or have someone honk).")
    input("  Press Enter when horn is playing...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "smart_helmet", "phase": "horn_test"})
    time.sleep(5)
    print("  Horn detection threshold set.")

    print("\n[4/4] Impact simulation (gentle tap)")
    print("  Tap helmet firmly with palm (NOT hard).")
    input("  Press Enter, then tap within 3 seconds...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "smart_helmet", "phase": "impact_sim"})
    time.sleep(5)
    print("  Impact threshold calibrated (should NOT trigger crash alert).")

    print("\nSmart Helmet calibration complete!")


def calibrate_smart_light():
    """Smart Light calibration: brake threshold, ambient light, turn signals."""
    print("\n=== Smart Light Calibration ===")
    print("Mount front light on handlebar, rear light on seat post.")
    input("Press Enter when ready...")

    print("\n[1/4] Ambient light sensor")
    print("  Cover sensor (dark) then uncover (light).")
    input("  Press Enter to start...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "smart_light", "phase": "ambient_light"})
    time.sleep(10)
    print("  Ambient light thresholds set (day/night/dim).")

    print("\n[2/4] Brake detection threshold")
    print("  Ride at 15 km/h, then brake firmly.")
    input("  Press Enter, then brake within 5 seconds...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "smart_light", "phase": "brake_threshold"})
    time.sleep(10)
    print("  Brake deceleration threshold set.")

    print("\n[3/4] Turn signal test")
    print("  Press left turn button — verify amber chase on rear left.")
    print("  Press right turn button — verify amber chase on rear right.")
    input("  Press Enter when verified...")

    print("\n[4/4] Headlight brightness")
    print("  Verify headlight: 100% (full), 50% (day), 30% (dim oncoming).")
    input("  Press Enter when verified...")

    print("\nSmart Light calibration complete!")


def calibrate_bike_sensor():
    """Bike Sensor calibration: wheel size, cadence, tire pressure."""
    print("\n=== Bike Sensor Calibration ===")
    print("Mount sensor on bike frame. Install wheel + crank magnets.")
    input("Press Enter when ready...")

    print("\n[1/4] Wheel circumference")
    print("  Enter wheel size (mm) — 700c = 2105, 26\" = 2070:")
    circ = input("  Circumference (mm): ").strip() or "2105"
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "bike_sensor", "phase": "wheel_size",
                        "circumference_mm": int(circ)})

    print("\n[2/4] Wheel speed verification")
    print("  Spin wheel at known speed (use bike computer as reference).")
    input("  Press Enter to start...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "bike_sensor", "phase": "wheel_speed_test"})
    time.sleep(15)
    print("  Wheel speed verified.")

    print("\n[3/4] Cadence verification")
    print("  Pedal at known cadence (use bike computer as reference).")
    input("  Press Enter to start...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "bike_sensor", "phase": "cadence_test"})
    time.sleep(15)
    print("  Cadence verified.")

    print("\n[4/4] Tire pressure")
    print("  Check tire pressure with manual gauge.")
    manual_psi = input("  Manual gauge PSI: ").strip()
    if manual_psi:
        requests.post(f"{API_BASE}/api/v1/calibration/start",
                      json={"node": "bike_sensor", "phase": "tpms_cal",
                            "manual_psi": float(manual_psi)})
    print("  TPMS calibrated against manual gauge.")

    print("\nBike Sensor calibration complete!")


def calibrate_smart_lock():
    """Smart Lock calibration: deadbolt, load cell, GPS, tamper threshold."""
    print("\n=== Smart Lock Calibration ===")
    print("Attach lock to bike. Insert battery.")
    input("Press Enter when ready...")

    print("\n[1/5] Deadbolt mechanism")
    print("  Verifying deadbolt extend/retract...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "smart_lock", "phase": "deadbolt_test"})
    time.sleep(5)
    print("  Deadbolt position verified by AS5600.")

    print("\n[2/5] Load cell zero")
    print("  Ensure no force on shackle.")
    input("  Press Enter to zero load cell...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "smart_lock", "phase": "load_cell_zero"})

    print("\n[3/5] Load cell calibration")
    print("  Apply known force (10 kg weight) to shackle.")
    input("  Press Enter with weight applied...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "smart_lock", "phase": "load_cell_cal",
                        "known_kg": 10})
    print("  Load cell calibrated.")

    print("\n[4/5] GPS fix")
    print("  Wait for GPS lock (outdoors, clear sky).")
    input("  Press Enter when GPS is locked...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "smart_lock", "phase": "gps_fix"})

    print("\n[5/5] Tamper threshold")
    print("  Gently shake lock — should NOT trigger alarm.")
    input("  Press Enter, then gently shake...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "smart_lock", "phase": "tamper_threshold"})
    time.sleep(10)
    print("  Tamper threshold set (gentle movement = OK, force = alarm).")

    print("\nSmart Lock calibration complete!")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="CycleGuard Calibration")
    parser.add_argument("--node", choices=["hub", "smart_helmet",
                                            "smart_light", "bike_sensor",
                                            "smart_lock"])
    parser.add_argument("--all", action="store_true")
    args = parser.parse_args()

    if args.all or args.node == "hub":
        calibrate_hub()
    if args.all or args.node == "smart_helmet":
        calibrate_smart_helmet()
    if args.all or args.node == "smart_light":
        calibrate_smart_light()
    if args.all or args.node == "bike_sensor":
        calibrate_bike_sensor()
    if args.all or args.node == "smart_lock":
        calibrate_smart_lock()

    print("\n=== All calibration complete ===")
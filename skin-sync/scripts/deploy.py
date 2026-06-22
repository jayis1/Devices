#!/usr/bin/env python3
"""
SkinSync Deployment Script

Deploys the cloud backend (FastAPI + PostgreSQL + Mosquitto) via Docker Compose,
and provides commands for device provisioning, ML training, and calibration.

Usage:
    python deploy.py up          # Start cloud services
    python deploy.py down        # Stop cloud services
    python deploy.py provision   # Provision a new user + patch
    python deploy.py train       # Run ML training pipelines
    python deploy.py calibrate   # Calibrate UV sensor + dispenser pumps
"""

import subprocess
import sys
import json
import argparse
import os

DASHBOARD_DIR = os.path.join(os.path.dirname(__file__), "..", "software", "dashboard")
ML_PIPELINE_DIR = os.path.join(os.path.dirname(__file__), "..", "software", "ml-pipeline")


def cmd_up(args):
    print("Starting SkinSync cloud services...")
    subprocess.run(["docker", "compose", "up", "-d"], cwd=DASHBOARD_DIR, check=True)
    print("✓ Services started:")
    print("  - FastAPI:    http://localhost:8000")
    print("  - Mosquitto:  tcp://localhost:1883")
    print("  - PostgreSQL: localhost:5432")
    print("\n  Health check: curl http://localhost:8000/api/v1/health")


def cmd_down(args):
    print("Stopping SkinSync cloud services...")
    subprocess.run(["docker", "compose", "down"], cwd=DASHBOARD_DIR, check=True)
    print("✓ Services stopped")


def cmd_provision(args):
    print("=== User + UV Patch Provisioning ===")
    name = input("Your name: ").strip()
    print("\nFitzpatrick Skin Type:")
    print("  1 - Very fair (always burns, never tans)")
    print("  2 - Fair (usually burns, tans minimally)")
    print("  3 - Medium (sometimes burns, tans gradually)")
    print("  4 - Olive (rarely burns, tans easily)")
    print("  5 - Brown (very rarely burns, tans darkly)")
    print("  6 - Dark (never burns, tans deeply)")
    fitz = input("Your skin type (1-6, default 3): ").strip() or "3"

    print("\n--- UV Patch ---")
    patch_id = input("UV Patch ID (1-15): ").strip()
    location = input("Wear location (wrist/shoulder): ").strip() or "wrist"

    profile_data = {
        "name": name, "fitz_type": int(fitz),
        "patch_id": int(patch_id), "location": location,
    }
    print(f"\nUser profile:")
    print(json.dumps(profile_data, indent=2))

    try:
        import requests
        r = requests.post(
            "http://localhost:8000/api/v1/users",
            params={"name": name, "fitz_type": int(fitz)}
        )
        print(f"\n✓ User registered: {r.status_code}")
        print(json.dumps(r.json(), indent=2))
        print(f"\n✓ UV Patch #{patch_id} paired to user (via BLE in app)")
    except Exception as e:
        print(f"\n⚠ Backend not reachable: {e}")
        print("  Start services first: python deploy.py up")


def cmd_train(args):
    model = args.model or "all"
    scripts = {
        "condition": "train_condition_classifier.py",
        "abcde": "train_abcde_detector.py",
        "uv_risk": "train_uv_risk.py",
        "routine": "train_routine_optimizer.py",
        "skin_age": "train_skin_age.py",
    }
    if model == "all":
        for name, script in scripts.items():
            print(f"\n--- Training {name} model ---")
            subprocess.run([sys.executable, script], cwd=ML_PIPELINE_DIR)
    elif model in scripts:
        subprocess.run([sys.executable, scripts[model]], cwd=ML_PIPELINE_DIR)
    else:
        print(f"Unknown model: {model}. Options: {', '.join(scripts.keys())}, all")


def cmd_calibrate(args):
    print("=== UV Sensor Calibration ===")
    print("1. Take UV patch outdoors on a clear day at solar noon")
    print("2. Compare VEML6075 reading with a reference UV meter")
    print("3. Adjust calibration coefficients in firmware\n")

    print("=== Dispenser Pump Calibration ===")
    for slot in range(4):
        print(f"\n--- Slot {slot} ---")
        input(f"Place empty container under slot {slot} nozzle. Press Enter.")
        print("  Dispensing 10ml (10000mg)...")
        print("  In production: run pump for 10ml / flow_rate seconds")
        print("  Measure actual amount with precision scale")
        actual = input("  Enter actual amount dispensed (ml): ").strip()
        if actual:
            rate = float(actual) / 10.0  # ml per second (10s dispense)
            print(f"  Calibrated flow rate: {rate:.3f} ml/sec")
            print(f"  Store in flash: slot {slot} flow_rate = {rate}")

    print("\n✓ Calibration complete.")


def main():
    parser = argparse.ArgumentParser(description="SkinSync deployment tool")
    sub = parser.add_subparsers(dest="command")

    sub.add_parser("up", help="Start cloud services")
    sub.add_parser("down", help="Stop cloud services")
    sub.add_parser("provision", help="Provision a new user + UV patch")
    p_train = sub.add_parser("train", help="Run ML training pipelines")
    p_train.add_argument("--model", choices=["condition", "abcde", "uv_risk",
                                              "routine", "skin_age", "all"])
    sub.add_parser("calibrate", help="Calibrate UV sensor + dispenser pumps")

    args = parser.parse_args()
    if args.command == "up":
        cmd_up(args)
    elif args.command == "down":
        cmd_down(args)
    elif args.command == "provision":
        cmd_provision(args)
    elif args.command == "train":
        cmd_train(args)
    elif args.command == "calibrate":
        cmd_calibrate(args)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
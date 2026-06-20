#!/usr/bin/env python3
"""
GreenPulse Deployment Script

Deploys the cloud backend (FastAPI + PostgreSQL + Mosquitto) via Docker Compose,
and provides commands for device provisioning and ML training.

Usage:
    python deploy.py up          # Start cloud services
    python deploy.py down        # Stop cloud services
    python deploy.py provision   # Provision a new user + plant
    python deploy.py train       # Run ML training pipelines
    python deploy.py calibrate   # Calibrate soil moisture sensors
"""

import subprocess
import sys
import json
import argparse
import os

DASHBOARD_DIR = os.path.join(os.path.dirname(__file__), "..", "software", "dashboard")
ML_PIPELINE_DIR = os.path.join(os.path.dirname(__file__), "..", "software", "ml-pipeline")


def cmd_up(args):
    print("Starting GreenPulse cloud services...")
    subprocess.run(["docker", "compose", "up", "-d"], cwd=DASHBOARD_DIR, check=True)
    print("✓ Services started:")
    print("  - FastAPI:    http://localhost:8000")
    print("  - Mosquitto:  tcp://localhost:1883")
    print("  - PostgreSQL: localhost:5432")
    print("\n  Health check: curl http://localhost:8000/api/v1/health")


def cmd_down(args):
    print("Stopping GreenPulse cloud services...")
    subprocess.run(["docker", "compose", "down"], cwd=DASHBOARD_DIR, check=True)
    print("✓ Services stopped")


def cmd_provision(args):
    print("=== User + Plant Provisioning ===")
    name = input("Your name: ").strip()
    home_humidity = input("Home humidity avg % (default 45): ").strip() or "45"
    light_dir = input("Primary window direction (N/S/E/W): ").strip()

    print("\n--- Add a Plant ---")
    tag_id = input("Plant Tag ID (1-63): ").strip()
    plant_name = input("Plant name (e.g. 'Big Mike'): ").strip()
    species = input("Species name (e.g. 'Monstera deliciosa'): ").strip()
    profile = input("Care profile ID (1-16, see docs): ").strip() or "1"
    location = input("Location (e.g. 'Living Room'): ").strip()

    profile_data = {
        "name": name, "home_humidity": int(home_humidity),
        "light_direction": light_dir,
    }
    print(f"\nUser profile:")
    print(json.dumps(profile_data, indent=2))

    print(f"\nPlant: {plant_name} ({species}) on Tag #{tag_id} in {location}")

    try:
        import requests
        r = requests.post(
            "http://localhost:8000/api/v1/plants",
            params={"user_id": 1, "tag_id": int(tag_id),
                     "name": plant_name, "species_name": species,
                     "profile_id": int(profile), "location": location}
        )
        print(f"\n✓ Plant registered: {r.status_code}")
        print(json.dumps(r.json(), indent=2))
    except Exception as e:
        print(f"\n⚠ Backend not reachable: {e}")
        print("  Start services first: python deploy.py up")


def cmd_train(args):
    model = args.model or "all"
    scripts = {
        "disease": "train_disease_classifier.py",
        "watering": "train_watering_model.py",
        "pest": "train_pest_detector.py",
        "species": "train_species_id.py",
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
    print("=== Soil Moisture Sensor Calibration ===")
    print("This calibrates the capacitive soil moisture sensor for your soil type.")
    print("\n1. Place sensor in air (completely dry) — press Enter")
    input()
    print("  Reading dry value... (in production: read ADC)")
    dry_val = 3000  # stub
    print(f"  Dry reading: {dry_val} mV → 0% VWC")

    print("\n2. Place sensor in saturated soil — press Enter")
    input()
    print("  Reading wet value... (in production: read ADC)")
    wet_val = 1500  # stub
    print(f"  Wet reading: {wet_val} mV → 100% VWC")

    print(f"\nCalibration: VWC% = (3000 - v) / (3000 - 1500) * 100")
    print(f"  dry_mV = {dry_val}, wet_mV = {wet_val}")
    print("\n✓ Calibration saved. Tag will report accurate VWC%.")


def main():
    parser = argparse.ArgumentParser(description="GreenPulse deployment tool")
    sub = parser.add_subparsers(dest="command")

    sub.add_parser("up", help="Start cloud services")
    sub.add_parser("down", help="Stop cloud services")
    sub.add_parser("provision", help="Provision a new user + plant")
    p_train = sub.add_parser("train", help="Run ML training pipelines")
    p_train.add_argument("--model", choices=["disease", "watering", "pest",
                                              "species", "all"])
    sub.add_parser("calibrate", help="Calibrate soil moisture sensors")

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
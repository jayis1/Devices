#!/usr/bin/env python3
"""
PawSync Deployment Script

Deploys the cloud backend (FastAPI + PostgreSQL + Mosquitto) via Docker Compose,
and provides commands for device provisioning and configuration.

Usage:
    python deploy.py up          # Start cloud services
    python deploy.py down        # Stop cloud services
    python deploy.py provision   # Provision a new pet profile
    python deploy.py train       # Run ML training pipelines
    python deploy.py baseline    # Force baseline recalculation
"""

import subprocess
import sys
import json
import argparse
import os

DASHBOARD_DIR = os.path.join(os.path.dirname(__file__), "..", "software", "dashboard")
ML_PIPELINE_DIR = os.path.join(os.path.dirname(__file__), "..", "software", "ml-pipeline")


def cmd_up(args):
    """Start cloud services via Docker Compose."""
    print("Starting PawSync cloud services...")
    subprocess.run(
        ["docker", "compose", "up", "-d"],
        cwd=DASHBOARD_DIR,
        check=True,
    )
    print("✓ Services started:")
    print("  - FastAPI:  http://localhost:8000")
    print("  - Mosquitto: tcp://localhost:1883")
    print("  - PostgreSQL: localhost:5432")
    print("\n  Health check: curl http://localhost:8000/api/v1/health")


def cmd_down(args):
    """Stop cloud services."""
    print("Stopping PawSync cloud services...")
    subprocess.run(
        ["docker", "compose", "down"],
        cwd=DASHBOARD_DIR,
        check=True,
    )
    print("✓ Services stopped")


def cmd_provision(args):
    """Provision a new pet profile."""
    print("=== Pet Provisioning ===")
    name = input("Pet name: ").strip()
    species = input("Species (dog/cat): ").strip().lower()
    breed = input("Breed: ").strip()
    weight = input("Current weight (kg): ").strip()
    target = input("Target weight (kg, Enter=same): ").strip()
    rfid = input("RFID UID (hex, Enter=skip): ").strip()

    if not target:
        target = weight

    profile = {
        "name": name,
        "species": species,
        "breed": breed,
        "weight_g": int(float(weight) * 1000),
        "target_weight_g": int(float(target) * 1000),
        "rfid_uid": rfid if rfid else None,
    }

    print(f"\nPet profile:")
    print(json.dumps(profile, indent=2))

    # Register with backend
    import requests
    try:
        r = requests.post(
            "http://localhost:8000/api/v1/pets",
            json=profile,
            timeout=5,
        )
        if r.ok:
            print(f"\n✓ Pet '{name}' registered (ID: {r.json().get('id', '?')})")
        else:
            print(f"\n✗ Registration failed: {r.status_code}")
            print("  (Ensure backend is running: python deploy.py up)")
    except Exception as e:
        print(f"\n⚠ Could not reach backend: {e}")
        print("  Profile saved locally for later sync.")
        with open(f"pet_profile_{name}.json", "w") as f:
            json.dump(profile, f, indent=2)


def cmd_train(args):
    """Run ML training pipelines."""
    model = args.model or "all"

    scripts = {
        "wellness": "train_wellness_score.py",
        "activity": "train_activity_classifier.py",
        "vocalization": "train_vocalization.py",
        "lameness": "train_lameness.py",
    }

    if model == "all":
        for name, script in scripts.items():
            print(f"\n=== Training {name} model ===")
            subprocess.run(
                [sys.executable, script],
                cwd=ML_PIPELINE_DIR,
                check=True,
            )
    elif model in scripts:
        print(f"\n=== Training {model} model ===")
        subprocess.run(
            [sys.executable, scripts[model]],
            cwd=ML_PIPELINE_DIR,
            check=True,
        )
    else:
        print(f"Unknown model: {model}. Available: {', '.join(scripts.keys())}")


def cmd_baseline(args):
    """Force baseline recalculation for a pet."""
    pet_id = args.pet_id or 1
    print(f"Recalculating baseline for pet {pet_id}...")
    import requests
    try:
        r = requests.post(
            f"http://localhost:8000/api/v1/pet/{pet_id}/baseline/recalculate",
            timeout=5,
        )
        print(f"✓ {r.json()}" if r.ok else f"✗ {r.status_code}")
    except Exception as e:
        print(f"⚠ Could not reach backend: {e}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="PawSync deployment tool")
    subparsers = parser.add_subparsers(dest="command")

    subparsers.add_parser("up", help="Start cloud services")
    subparsers.add_parser("down", help="Stop cloud services")

    prov_parser = subparsers.add_parser("provision", help="Provision a new pet")
    train_parser = subparsers.add_parser("train", help="Run ML training")
    train_parser.add_argument("--model", choices=["all", "wellness", "activity", "vocalization", "lameness"],
                              default="all")

    base_parser = subparsers.add_parser("baseline", help="Force baseline recalculation")
    base_parser.add_argument("--pet-id", type=int, default=1)

    args = parser.parse_args()

    commands = {
        "up": cmd_up,
        "down": cmd_down,
        "provision": cmd_provision,
        "train": cmd_train,
        "baseline": cmd_baseline,
    }

    if args.command in commands:
        commands[args.command](args)
    else:
        parser.print_help()
#!/usr/bin/env python3
"""
CalmGrid Deployment Script

Deploys the cloud backend (FastAPI + PostgreSQL + Mosquitto) via Docker Compose,
and provides commands for device provisioning and configuration.

Usage:
    python deploy.py up          # Start cloud services
    python deploy.py down        # Stop cloud services
    python deploy.py provision   # Provision a new user profile
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
    print("Starting CalmGrid cloud services...")
    subprocess.run(["docker", "compose", "up", "-d"], cwd=DASHBOARD_DIR, check=True)
    print("✓ Services started:")
    print("  - FastAPI:    http://localhost:8000")
    print("  - Mosquitto:  tcp://localhost:1883")
    print("  - PostgreSQL: localhost:5432")
    print("\n  Health check: curl http://localhost:8000/api/v1/health")


def cmd_down(args):
    """Stop cloud services."""
    print("Stopping CalmGrid cloud services...")
    subprocess.run(["docker", "compose", "down"], cwd=DASHBOARD_DIR, check=True)
    print("✓ Services stopped")


def cmd_provision(args):
    """Provision a new user profile."""
    print("=== User Provisioning ===")
    name = input("Name: ").strip()
    age = input("Age: ").strip()
    sex = input("Sex (M/F/other): ").strip()
    work = input("Work pattern (office/remote/hybrid/shift): ").strip()
    sleep = input("Sleep target (hours, default 8): ").strip() or "8"

    profile = {
        "name": name,
        "age": int(age),
        "sex": sex,
        "work_pattern": work,
        "sleep_target_h": int(sleep),
    }

    print(f"\nUser profile:")
    print(json.dumps(profile, indent=2))

    try:
        import requests
        r = requests.post(
            "http://localhost:8000/api/v1/ingest/vitals",
            params={"user_id": 1, "hr": 0, "hrv_ms": 0, "eda_scl": 0,
                    "eda_scr": 0, "temp_c": 0, "activity": 0, "steps": 0, "battery": 100}
        )
        print(f"\n✓ Backend reachable: {r.status_code}")
    except Exception as e:
        print(f"\n⚠ Backend not reachable: {e}")
        print("  Start services first: python deploy.py up")


def cmd_train(args):
    """Run ML training pipelines."""
    model = args.model or "all"
    scripts = {
        "stress": "train_stress_model.py",
        "activity": "train_activity_classifier.py",
        "prosody": "train_prosody.py",
        "burnout": "train_burnout.py",
    }
    if model == "all":
        for name, script in scripts.items():
            print(f"\n--- Training {name} model ---")
            subprocess.run([sys.executable, script], cwd=ML_PIPELINE_DIR)
    elif model in scripts:
        subprocess.run([sys.executable, scripts[model]], cwd=ML_PIPELINE_DIR)
    else:
        print(f"Unknown model: {model}. Options: {', '.join(scripts.keys())}, all")


def cmd_baseline(args):
    """Force baseline recalculation."""
    print("Forcing baseline recalculation...")
    script = os.path.join(ML_PIPELINE_DIR, "personal_baseline.py")
    subprocess.run([sys.executable, script], check=True)


def main():
    parser = argparse.ArgumentParser(description="CalmGrid deployment tool")
    sub = parser.add_subparsers(dest="command")

    sub.add_parser("up", help="Start cloud services")
    sub.add_parser("down", help="Stop cloud services")
    sub.add_parser("provision", help="Provision a new user profile")
    p_train = sub.add_parser("train", help="Run ML training pipelines")
    p_train.add_argument("--model", choices=["stress", "activity", "prosody", "burnout", "all"])
    sub.add_parser("baseline", help="Force baseline recalculation")

    args = parser.parse_args()
    if args.command == "up":
        cmd_up(args)
    elif args.command == "down":
        cmd_down(args)
    elif args.command == "provision":
        cmd_provision(args)
    elif args.command == "train":
        cmd_train(args)
    elif args.command == "baseline":
        cmd_baseline(args)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
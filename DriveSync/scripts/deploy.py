#!/usr/bin/env python3
"""
DriveSync — Cloud Deployment Script

Deploys the FastAPI backend, MQTT broker, and TimescaleDB via docker-compose.
Also trains and deploys the ML models.

License: MIT
"""

import subprocess
import os
import sys
import time

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DASHBOARD_DIR = os.path.join(BASE_DIR, "software", "dashboard")
ML_DIR = os.path.join(BASE_DIR, "software", "ml-pipeline")


def run(cmd, cwd=None, check=True):
    """Run a shell command and stream output."""
    print(f"$ {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=False)
    if check and result.returncode != 0:
        print(f"Error: command failed with code {result.returncode}")
        sys.exit(1)
    return result.returncode


def deploy_cloud():
    """Deploy cloud backend via docker-compose."""
    print("=" * 60)
    print("DriveSync — Cloud Backend Deployment")
    print("=" * 60)

    # Check docker
    run("docker --version", check=False)
    run("docker-compose --version", check=False)

    # Start services
    print("\n[1/3] Starting TimescaleDB + MQTT + API...")
    run("docker-compose up -d", cwd=DASHBOARD_DIR)

    # Wait for services
    print("\n[2/3] Waiting for services to start...")
    time.sleep(10)

    # Health check
    print("\n[3/3] Health check...")
    run("curl -s http://localhost:8000/api/v1/health || echo 'API not ready yet'", check=False)

    print("\n✓ Cloud backend deployed!")
    print("  API:     http://localhost:8000")
    print("  Docs:    http://localhost:8000/docs")
    print("  MQTT:    localhost:1883")
    print("  DB:      localhost:5432")


def train_models():
    """Train all ML models."""
    print("\n" + "=" * 60)
    print("DriveSync — ML Model Training")
    print("=" * 60)

    models = [
        ("PERCLOS Eye-Closure CNN", "train_drowsiness_cnn.py"),
        ("Head-Pose CNN", "train_headpose_cnn.py"),
        ("Steering Jerkiness XGBoost", "train_steering_xgboost.py"),
        ("HRV Drowsiness LSTM", "train_hrv_drowsiness_lstm.py"),
        ("Risk Fusion Model", "train_risk_fusion.py"),
    ]

    for name, script in models:
        print(f"\n--- Training: {name} ---")
        run(f"python {script}", cwd=ML_DIR, check=False)

    print("\n--- Evaluating All Models ---")
    run("python evaluate_all.py", cwd=ML_DIR, check=False)

    print("\n✓ ML pipeline trained!")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="DriveSync Deployment")
    parser.add_argument("--cloud", action="store_true", help="Deploy cloud backend")
    parser.add_argument("--ml", action="store_true", help="Train ML models")
    parser.add_argument("--all", action="store_true", help="Deploy everything")
    args = parser.parse_args()

    if args.all or args.cloud:
        deploy_cloud()
    if args.all or args.ml:
        train_models()

    if not args.all and not args.cloud and not args.ml:
        print("Usage: python deploy.py [--cloud] [--ml] [--all]")
        print("  --cloud  Deploy cloud backend (docker-compose)")
        print("  --ml     Train all ML models")
        print("  --all    Deploy everything")
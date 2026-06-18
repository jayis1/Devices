#!/usr/bin/env python3
"""
deploy.py — ThermoGrid deployment script.

Deploys the cloud backend (Docker), initializes the database, and
optionally triggers initial ML model training.

Usage:
  python deploy.py                    # full deploy (backend + ML)
  python deploy.py --backend-only     # just the FastAPI backend
  python deploy.py --ml-only          # just train ML models
"""

import argparse
import subprocess
import os
import sys

BACKEND_DIR = os.path.join(os.path.dirname(__file__), "..", "software", "dashboard")
ML_DIR = os.path.join(os.path.dirname(__file__), "..", "software", "ml-pipeline")


def deploy_backend():
    """Deploy the FastAPI backend + Mosquitto MQTT broker via Docker Compose."""
    print("=== Deploying ThermoGrid Backend (Docker) ===")
    compose_file = os.path.join(BACKEND_DIR, "docker-compose.yml")

    if not os.path.exists(compose_file):
        print(f"[ERROR] docker-compose.yml not found at {compose_file}")
        return False

    # Write mosquitto config if not present
    mosq_conf = os.path.join(BACKEND_DIR, "mosquitto.conf")
    if not os.path.exists(mosq_conf):
        with open(mosq_conf, "w") as f:
            f.write("listener 1883\nallow_anonymous true\n")
        print("[OK] Created mosquitto.conf")

    # Docker compose up
    try:
        print("[INFO] Running docker compose up...")
        result = subprocess.run(
            ["docker", "compose", "-f", compose_file, "up", "-d", "--build"],
            capture_output=True, text=True
        )
        if result.returncode == 0:
            print("[OK] Backend deployed successfully")
            print("  Dashboard API: http://localhost:8000")
            print("  MQTT broker:    localhost:1883")
        else:
            print(f"[ERROR] Docker compose failed: {result.stderr}")
            return False
    except FileNotFoundError:
        print("[ERROR] Docker not found. Install Docker first.")
        return False

    return True


def train_ml_models():
    """Train initial ML models (thermal forecast + comfort + routine)."""
    print("\n=== Training ML Models ===")

    # Install requirements
    req_file = os.path.join(ML_DIR, "requirements.txt")
    print(f"[INFO] Installing ML requirements from {req_file}...")
    try:
        subprocess.run([sys.executable, "-m", "pip", "install", "-r", req_file],
                       check=True, capture_output=True)
    except subprocess.CalledProcessError as e:
        print(f"[WARN] Some packages may not have installed: {e}")
        print("       (This is expected if torch/xgboost are not available in this env)")

    # Train thermal forecast model
    print("\n[INFO] Training thermal forecast model...")
    thermal_script = os.path.join(ML_DIR, "train_thermal_forecast.py")
    try:
        result = subprocess.run(
            [sys.executable, thermal_script],
            cwd=ML_DIR, capture_output=True, text=True, timeout=300
        )
        print(result.stdout[-500:] if len(result.stdout) > 500 else result.stdout)
        if result.stderr:
            print("[STDERR]", result.stderr[-300:])
    except subprocess.TimeoutExpired:
        print("[WARN] Thermal forecast training timed out (300s)")
    except Exception as e:
        print(f"[WARN] Thermal forecast training failed: {e}")

    # Train comfort model
    print("\n[INFO] Training comfort model...")
    comfort_script = os.path.join(ML_DIR, "train_comfort_model.py")
    try:
        result = subprocess.run(
            [sys.executable, comfort_script],
            cwd=ML_DIR, capture_output=True, text=True, timeout=120
        )
        print(result.stdout[-500:] if len(result.stdout) > 500 else result.stdout)
    except Exception as e:
        print(f"[WARN] Comfort model training failed: {e}")

    # Train routine model
    print("\n[INFO] Training routine/occupancy model...")
    routine_script = os.path.join(ML_DIR, "train_routine_model.py")
    try:
        result = subprocess.run(
            [sys.executable, routine_script],
            cwd=ML_DIR, capture_output=True, text=True, timeout=120
        )
        print(result.stdout[-500:] if len(result.stdout) > 500 else result.stdout)
    except Exception as e:
        print(f"[WARN] Routine model training failed: {e}")

    print("\n✓ ML model training complete (check models/ directory)")


def main():
    parser = argparse.ArgumentParser(description="ThermoGrid deployment")
    parser.add_argument("--backend-only", action="store_true",
                        help="Deploy only the FastAPI backend")
    parser.add_argument("--ml-only", action="store_true",
                        help="Train only the ML models")

    args = parser.parse_args()

    if args.ml_only:
        train_ml_models()
    elif args.backend_only:
        deploy_backend()
    else:
        success = deploy_backend()
        if success:
            train_ml_models()

    print("\n=== Deployment Complete ===")
    print("Next steps:")
    print("  1. Set up the hub (USB-C power, connect to WiFi via app)")
    print("  2. Install room sensors (one per room)")
    print("  3. Install zone actuators (one per heating zone)")
    print("  4. Pair comfort tag via app")
    print("  5. Run calibration: python scripts/calibrate_thermal.py --all")


if __name__ == "__main__":
    main()
#!/usr/bin/env python3
"""
SightSync — Full System Deployment Script

Deploys the complete SightSync system:
1. Trains all ML models
2. Converts models for edge deployment
3. Copies models to firmware directory
4. Builds firmware for all nodes
5. Starts cloud backend

License: MIT
"""

import subprocess
import os
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..")

def run(cmd, cwd=ROOT):
    print(f"  $ {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd)
    if result.returncode != 0:
        print(f"  ✗ Command failed: {cmd}")
        return False
    return True

def deploy_all():
    print("=== SightSync Full System Deployment ===\n")

    # 1. Train ML models
    print("1. Training ML models...")
    ml_dir = os.path.join(ROOT, "software", "ml-pipeline")
    for script in ["train_fatigue_index.py", "train_blink_anomaly.py",
                   "train_myopia_risk.py", "train_circadian_rl.py",
                   "train_posture_cnn.py", "train_dry_eye_risk.py"]:
        run(f"python {script}", cwd=ml_dir)

    # 2. Convert models
    print("\n2. Converting models for edge deployment...")
    run("python convert_models.py", cwd=ml_dir)

    # 3. Copy models to firmware
    print("\n3. Copying models to firmware directory...")
    run("python ../scripts/deploy.py", cwd=ml_dir)

    # 4. Build firmware
    print("\n4. Building firmware...")
    fw_dir = os.path.join(ROOT, "firmware")
    for env in ["hub", "desk-sentinel", "eye-tag", "lamp-node"]:
        run(f"pio run -e {env}", cwd=fw_dir)

    # 5. Start cloud backend
    print("\n5. Starting cloud backend...")
    dash_dir = os.path.join(ROOT, "software", "dashboard")
    run("docker compose up -d", cwd=dash_dir)

    print("\n=== SightSync deployment complete! ===")
    print("  - Cloud API: http://localhost:8000")
    print("  - MQTT broker: localhost:1883")
    print("  - TimescaleDB: localhost:5432")
    print("  - Flash firmware: pio run -e <env> -t upload")

if __name__ == "__main__":
    deploy_all()
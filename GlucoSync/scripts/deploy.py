#!/usr/bin/env python3
"""
GlucoSync — Deployment Script

Deploys the GlucoSync cloud backend (Docker Compose) and flashes firmware.

Usage:
    python deploy.py --backend     # Deploy cloud backend
    python deploy.py --hub          # Flash hub firmware
    python deploy.py --scanner      # Flash meal scanner firmware
    python deploy.py --band         # Flash activity band
    python deploy.py --pen          # Flash pen tag
    python deploy.py --all          # Deploy everything

License: MIT
"""

import subprocess
import sys
import os
import argparse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def deploy_backend():
    """Deploy cloud backend with Docker Compose."""
    print("=== Deploying GlucoSync Cloud Backend ===")
    dashboard_dir = os.path.join(ROOT, "software", "dashboard")
    print(f"Working directory: {dashboard_dir}")

    # Check docker
    result = subprocess.run(["docker", "--version"], capture_output=True)
    if result.returncode != 0:
        print("ERROR: Docker not installed")
        return False

    # Build and start
    result = subprocess.run(
        ["docker-compose", "up", "-d", "--build"],
        cwd=dashboard_dir
    )
    if result.returncode == 0:
        print("✓ Backend deployed. API at http://localhost:8000")
        print("  Docs at http://localhost:8000/docs")
        return True
    else:
        print("✗ Backend deployment failed")
        return False


def flash_hub():
    """Flash Metabolic Hub firmware (ESP32-S3)."""
    print("=== Flashing GlucoSync Metabolic Hub ===")
    hub_dir = os.path.join(ROOT, "firmware", "hub")
    print(f"Working directory: {hub_dir}")

    result = subprocess.run(["idf.py", "build", "flash", "monitor"], cwd=hub_dir)
    return result.returncode == 0


def flash_scanner():
    """Flash Meal Scanner firmware (ESP32-S3)."""
    print("=== Flashing GlucoSync Meal Scanner ===")
    scanner_dir = os.path.join(ROOT, "firmware", "meal-scanner")
    result = subprocess.run(["idf.py", "build", "flash", "monitor"], cwd=scanner_dir)
    return result.returncode == 0


def flash_band():
    """Flash Activity Band firmware (nRF52840)."""
    print("=== Flashing GlucoSync Activity Band ===")
    fw_dir = os.path.join(ROOT, "firmware")
    result = subprocess.run(
        ["platformio", "run", "-e", "activity_band", "--target", "upload"],
        cwd=fw_dir
    )
    return result.returncode == 0


def flash_pen():
    """Flash Insulin Pen Tag firmware (nRF52840)."""
    print("=== Flashing GlucoSync Insulin Pen Tag ===")
    fw_dir = os.path.join(ROOT, "firmware")
    result = subprocess.run(
        ["platformio", "run", "-e", "pen_tag", "--target", "upload"],
        cwd=fw_dir
    )
    return result.returncode == 0


def main():
    parser = argparse.ArgumentParser(description="GlucoSync deployment tool")
    parser.add_argument("--backend", action="store_true", help="Deploy cloud backend")
    parser.add_argument("--hub", action="store_true", help="Flash hub firmware")
    parser.add_argument("--scanner", action="store_true", help="Flash meal scanner")
    parser.add_argument("--band", action="store_true", help="Flash activity band")
    parser.add_argument("--pen", action="store_true", help="Flash pen tag")
    parser.add_argument("--all", action="store_true", help="Deploy everything")
    args = parser.parse_args()

    if args.all:
        args.backend = True
        args.hub = True
        args.scanner = True
        args.band = True
        args.pen = True

    success = True

    if args.backend:
        success &= deploy_backend()
    if args.hub:
        success &= flash_hub()
    if args.scanner:
        success &= flash_scanner()
    if args.band:
        success &= flash_band()
    if args.pen:
        success &= flash_pen()

    if not any([args.backend, args.hub, args.scanner, args.band, args.pen, args.all]):
        parser.print_help()
        sys.exit(1)

    print(f"\n{'✓ All deployments successful' if success else '✗ Some deployments failed'}")


if __name__ == "__main__":
    main()
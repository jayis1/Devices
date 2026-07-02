#!/usr/bin/env python3
"""
JointSync — Deployment Script

Sets up the complete JointSync cloud backend.

Usage: python deploy.py [--local | --docker]
"""

import subprocess
import sys
import os
import argparse

def deploy_docker():
    """Deploy using Docker Compose."""
    print("=== JointSync Docker Deployment ===")
    dashboard_dir = os.path.join(os.path.dirname(__file__), "..", "software", "dashboard")

    print("Starting services with Docker Compose...")
    subprocess.run(["docker-compose", "up", "-d", "--build"],
                   cwd=dashboard_dir, check=True)

    print("\nWaiting for services to start...")
    import time
    time.sleep(10)

    # Health check
    try:
        import requests
        resp = requests.get("http://localhost:8000/health", timeout=5)
        if resp.status_code == 200:
            print("✓ API is healthy")
        else:
            print(f"✗ API health check failed: {resp.status_code}")
    except Exception as e:
        print(f"⚠ Could not reach API (may need more time): {e}")

    print("\n✓ JointSync backend deployed!")
    print("  API: http://localhost:8000")
    print("  API docs: http://localhost:8000/docs")
    print("  MQTT: localhost:1883")
    print("  Database: localhost:5432")

def deploy_local():
    """Deploy locally (requires PostgreSQL + Mosquitto installed)."""
    print("=== JointSync Local Deployment ===")
    dashboard_dir = os.path.join(os.path.dirname(__file__), "..", "software", "dashboard")

    print("Installing Python dependencies...")
    subprocess.run([sys.executable, "-m", "pip", "install", "-r",
                   os.path.join(dashboard_dir, "requirements.txt")], check=True)

    print("\nStarting API server...")
    subprocess.run([sys.executable, "-m", "uvicorn", "main:app",
                   "--host", "0.0.0.0", "--port", "8000"],
                  cwd=dashboard_dir)

def main():
    parser = argparse.ArgumentParser(description="Deploy JointSync backend")
    parser.add_argument("--docker", action="store_true", help="Deploy with Docker Compose")
    parser.add_argument("--local", action="store_true", help="Deploy locally")
    args = parser.parse_args()

    if args.docker:
        deploy_docker()
    elif args.local:
        deploy_local()
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
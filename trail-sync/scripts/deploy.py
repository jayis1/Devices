#!/usr/bin/env python3
"""
TrailSync — Deployment Script

Deploys the TrailSync cloud dashboard (FastAPI backend)
and configures MQTT broker for hub communication.

Usage:
    python deploy.py [--env production|staging|dev]

SPDX-License-Identifier: MIT
"""
import argparse
import subprocess
import sys
import os

def run(cmd: str, check: bool = True):
    """Run a shell command."""
    print(f"  $ {cmd}")
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout.strip())
    if result.stderr and result.returncode != 0:
        print(f"  STDERR: {result.stderr.strip()}")
    if check and result.returncode != 0:
        print(f"ERROR: Command failed with exit code {result.returncode}")
        sys.exit(1)
    return result


def deploy_dashboard(env: str):
    """Deploy the FastAPI dashboard backend."""
    backend_dir = os.path.join(os.path.dirname(__file__), "..", "software", "dashboard", "backend")
    compose_dir = os.path.join(os.path.dirname(__file__), "..", "software", "dashboard")

    print(f"\n{'='*60}")
    print(f"TrailSync Dashboard — {env} Deployment")
    print(f"{'='*60}")

    # Build Docker image
    print("\n1. Building Docker image...")
    run(f"cd {backend_dir} && docker build -t trailsync-dashboard:{env} .")

    # Start services
    print("\n2. Starting services...")
    env_file = f".env.{env}" if os.path.exists(f"{compose_dir}/.env.{env}") else ""
    run(f"cd {compose_dir} && docker compose --env-file {env_file} up -d")

    # Wait for services
    print("\n3. Waiting for services...")
    run("sleep 5")

    # Health check
    print("\n4. Health check...")
    result = run("curl -s http://localhost:8023/health", check=False)
    if "ok" in result.stdout:
        print("✓ Dashboard is running on http://localhost:8023")
    else:
        print("⚠ Dashboard may not be ready yet. Check logs with: docker compose logs")


def deploy_mqtt(env: str):
    """Configure MQTT broker for hub communication."""
    print(f"\n5. MQTT broker status...")
    result = run("docker compose ps mosquitto", check=False)
    if "mosquitto" in result.stdout and "running" in result.stdout.lower():
        print("✓ MQTT broker (Mosquitto) is running on port 1883")
    else:
        print("⚠ MQTT broker may not be running")


def main():
    parser = argparse.ArgumentParser(description="TrailSync deployment")
    parser.add_argument("--env", choices=["production", "staging", "dev"],
                       default="dev", help="Deployment environment")
    args = parser.parse_args()

    print("TrailSync Deployment Script")
    print("=" * 40)
    print(f"Environment: {args.env}")

    deploy_dashboard(args.env)
    deploy_mqtt(args.env)

    print(f"\n{'='*60}")
    print("Deployment complete!")
    print("  Dashboard:  http://localhost:8023")
    print("  API docs:   http://localhost:8023/docs")
    print("  MQTT:       localhost:1883")
    print("  WebSocket:  ws://localhost:8023/ws/v1/live")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
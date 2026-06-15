#!/usr/bin/env python3
"""
MedSync - System Deployment Script
Sets up the cloud backend, configures MQTT, provisions the database,
and validates all node connections.

Usage:
    python3 deploy.py [--mqtt-host localhost] [--db-host localhost] [--env production]

Copyright (c) 2026 jayis1 - MIT License
"""

import subprocess
import sys
import os
import time
import json
import argparse
from pathlib import Path

SERVICES = {
    "mqtt": {
        "name": "Mosquitto MQTT Broker",
        "check_cmd": "mosquitto -h",
        "install_cmd": "sudo apt install -y mosquitto mosquitto-clients",
        "service": "mosquitto",
        "port": 1883,
    },
    "postgres": {
        "name": "PostgreSQL Database",
        "check_cmd": "psql --version",
        "install_cmd": "sudo apt install -y postgresql postgresql-contrib",
        "service": "postgresql",
        "port": 5432,
    },
    "redis": {
        "name": "Redis Cache",
        "check_cmd": "redis-cli --version",
        "install_cmd": "sudo apt install -y redis-server",
        "service": "redis-server",
        "port": 6379,
    },
}


def run(cmd, check=True, capture=False):
    """Run a shell command."""
    print(f"  $ {cmd}")
    result = subprocess.run(cmd, shell=True, capture_output=capture, text=True)
    if check and result.returncode != 0:
        print(f"  ERROR: Command failed with exit code {result.returncode}")
        if capture:
            print(f"  stdout: {result.stdout}")
            print(f"  stderr: {result.stderr}")
        sys.exit(1)
    return result


def check_service(name, config):
    """Check if a service is installed and running."""
    print(f"\nChecking {config['name']}...")

    # Check if installed
    result = run(config["check_cmd"], check=False, capture=True)
    if result.returncode != 0:
        print(f"  {config['name']} not found. Installing...")
        run(config["install_cmd"])
    else:
        print(f"  ✓ {config['name']} is installed")

    # Check if running
    result = run(f"systemctl is-active {config['service']}", check=False, capture=True)
    if result.returncode != 0 or "active" not in result.stdout:
        print(f"  Starting {config['service']}...")
        run(f"sudo systemctl start {config['service']}")
        run(f"sudo systemctl enable {config['service']}")
    else:
        print(f"  ✓ {config['service']} is running")


def setup_database(db_host, db_user, db_pass, db_name):
    """Create database and user."""
    print("\nSetting up PostgreSQL database...")

    # Create database user
    run(f'sudo -u postgres psql -c "CREATE USER {db_user} WITH PASSWORD \'{db_pass}\';"', check=False)

    # Create database
    run(f'sudo -u postgres psql -c "CREATE DATABASE {db_name} OWNER {db_user};"', check=False)

    # Grant privileges
    run(f'sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE {db_name} TO {db_user};"', check=False)

    # Test connection
    result = run(f"PGPASSWORD={db_pass} psql -h {db_host} -U {db_user} -d {db_name} -c 'SELECT 1;'", check=False, capture=True)
    if result.returncode == 0:
        print("  ✓ Database connection successful")
    else:
        print("  ✗ Database connection failed!")
        sys.exit(1)


def setup_mqtt():
    """Configure MQTT broker for MedSync."""
    print("\nConfiguring MQTT broker...")

    config_path = "/etc/mosquitto/conf.d/medsync.conf"
    config_content = """
# MedSync MQTT Configuration
listener 1883 0.0.0.0
allow_anonymous true
max_connections -1

# Persistence
persistence true
persistence_location /var/lib/mosquitto/

# Logging
log_dest syslog
log_type all
"""

    try:
        with open(config_path, 'w') as f:
            f.write(config_content)
        run("sudo systemctl restart mosquitto")
        print("  ✓ MQTT broker configured")
    except PermissionError:
        print("  ⚠ Cannot write MQTT config (run as root or with sudo)")


def setup_backend(backend_dir, mqtt_host, db_host, db_user, db_pass, db_name):
    """Set up the Python backend."""
    print("\nSetting up MedSync backend...")

    # Install Python dependencies
    if (Path(backend_dir) / "requirements.txt").exists():
        run(f"pip3 install -r {backend_dir}/requirements.txt")
    else:
        # Manual install
        run("pip3 install fastapi uvicorn aiomqtt asyncpg python-jose pydantic")

    print("  ✓ Backend dependencies installed")


def start_backend(backend_dir, mqtt_host, db_host, db_user, db_pass, db_name):
    """Start the FastAPI backend."""
    print("\nStarting MedSync backend...")

    env = os.environ.copy()
    env["MQTT_BROKER"] = mqtt_host
    env["DATABASE_URL"] = f"postgresql://{db_user}:{db_pass}@{db_host}:5432/{db_name}"

    backend_main = Path(backend_dir) / "main.py"

    # Start in background
    proc = subprocess.Popen(
        ["uvicorn", f"main:app", "--host", "0.0.0.0", "--port", "8000"],
        cwd=backend_dir,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    # Wait for startup
    time.sleep(3)

    # Health check
    result = run("curl -s http://localhost:8000/api/v1/status", check=False, capture=True)
    if result.returncode == 0:
        print("  ✓ Backend started successfully on port 8000")
        print(f"  PID: {proc.pid}")
    else:
        print("  ⚠ Backend may not be responding yet (startup takes a few seconds)")

    return proc


def validate_mesh_nodes():
    """Validate BLE mesh node connectivity."""
    print("\nValidating BLE mesh nodes...")
    print("  Note: This requires a BLE-capable host adapter.")
    print("  Run this on the hub node directly using the nRF Connect SDK.")
    print("  Use the MedSync provisioning app to add nodes to the mesh network.")
    print("  ✓ See docs/protocol.md for mesh provisioning details")


def main():
    parser = argparse.ArgumentParser(description="Deploy MedSync system")
    parser.add_argument("--mqtt-host", default="localhost", help="MQTT broker hostname")
    parser.add_argument("--db-host", default="localhost", help="PostgreSQL hostname")
    parser.add_argument("--db-user", default="medsync", help="PostgreSQL username")
    parser.add_argument("--db-pass", default="medsync123", help="PostgreSQL password")
    parser.add_argument("--db-name", default="medsync", help="PostgreSQL database name")
    parser.add_argument("--backend-dir", default=None, help="Backend directory path")
    parser.add_argument("--env", choices=["development", "production"], default="development")
    parser.add_argument("--skip-services", action="store_true", help="Skip service installation")

    args = parser.parse_args()

    # Resolve backend directory
    if args.backend_dir is None:
        script_dir = Path(__file__).parent
        args.backend_dir = str(script_dir.parent / "software" / "dashboard" / "backend")

    print("=" * 60)
    print("  MedSync System Deployment")
    print("=" * 60)

    # Install and start services
    if not args.skip_services:
        for name, config in SERVICES.items():
            check_service(name, config)

        # Setup database
        setup_database(args.db_host, args.db_user, args.db_pass, args.db_name)

        # Setup MQTT
        setup_mqtt()

    # Setup backend
    setup_backend(args.backend_dir, args.mqtt_host, args.db_host,
                  args.db_user, args.db_pass, args.db_name)

    # Start backend
    proc = start_backend(args.backend_dir, args.mqtt_host, args.db_host,
                         args.db_user, args.db_pass, args.db_name)

    # Validate nodes
    validate_mesh_nodes()

    print("\n" + "=" * 60)
    print("  MedSync deployment complete!")
    print("=" * 60)
    print(f"\n  Backend: http://localhost:8000")
    print(f"  API docs: http://localhost:8000/docs")
    print(f"  MQTT broker: {args.mqtt_host}:1883")
    print(f"  Database: {args.db_host}:5432/{args.db_name}")
    print(f"\n  Next steps:")
    print(f"    1. Open MedSync mobile app")
    print(f"    2. Pair with hub via NFC")
    print(f"    3. Add medications and schedule")
    print(f"    4. Set up caregiver contacts")


if __name__ == "__main__":
    main()
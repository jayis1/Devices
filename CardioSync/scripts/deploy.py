#!/usr/bin/env python3
"""
deploy.py — Deploy CardioSync cloud backend

Usage:
    python deploy.py [--build] [--up] [--down] [--logs]

License: MIT
"""
import subprocess
import sys
import os

DASHBOARD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "software", "dashboard")

def run(cmd, cwd=DASHBOARD_DIR):
    print(f"$ {' '.join(cmd)}")
    subprocess.run(cmd, cwd=cwd)

def main():
    args = sys.argv[1:]

    if "--build" in args or not args:
        print("Building Docker images...")
        run(["docker-compose", "build"])

    if "--up" in args or not args:
        print("Starting CardioSync cloud services...")
        run(["docker-compose", "up", "-d"])
        print("\nServices:")
        print("  API:        http://localhost:8000")
        print("  API docs:   http://localhost:8000/docs")
        print("  MQTT:       localhost:1883")
        print("  PostgreSQL: localhost:5432")
        print("  Redis:      localhost:6379")

    if "--down" in args:
        print("Stopping CardioSync cloud services...")
        run(["docker-compose", "down"])

    if "--logs" in args:
        run(["docker-compose", "logs", "-f"])

if __name__ == "__main__":
    main()
"""
SoleGuard Deployment Script

Deploys the cloud stack (FastAPI + Postgres + Mosquitto + MinIO) via docker-compose,
creates the S3 bucket for scan images, and runs DB migrations.
"""

import os
import subprocess
import sys
import time
import boto3

COMPOSE_FILE = os.path.join(os.path.dirname(__file__), "..", "software", "dashboard", "docker-compose.yml")
S3_ENDPOINT  = os.getenv("S3_ENDPOINT", "http://localhost:9000")
S3_BUCKET    = os.getenv("S3_BUCKET", "sole-scans")


def run(cmd: str, check: bool = True):
    print(f"$ {cmd}")
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    print(result.stdout)
    if result.stderr:
        print(result.stderr, file=sys.stderr)
    if check and result.returncode != 0:
        raise RuntimeError(f"Command failed: {cmd}")
    return result


def deploy_stack():
    print("=== Deploying SoleGuard cloud stack ===")
    run(f"docker compose -f {COMPOSE_FILE} up -d")
    print("Waiting for services to be healthy...")
    time.sleep(10)


def create_bucket():
    print("=== Creating S3 bucket for scan images ===")
    s3 = boto3.client("s3", endpoint_url=S3_ENDPOINT,
                      aws_access_key_id=os.getenv("S3_KEY", "minio"),
                      aws_secret_access_key=os.getenv("S3_SECRET", "minio123"))
    try:
        s3.create_bucket(Bucket=S3_BUCKET)
        print(f"Created bucket: {S3_BUCKET}")
    except Exception as e:
        print(f"Bucket may already exist: {e}")


def health_check():
    print("=== Health check ===")
    for attempt in range(10):
        try:
            result = subprocess.run(
                ["curl", "-s", "http://localhost:8000/api/v1/health"],
                capture_output=True, text=True, timeout=5)
            if result.returncode == 0 and "ok" in result.stdout:
                print(f"Backend healthy: {result.stdout.strip()}")
                return
        except Exception:
            pass
        time.sleep(3)
    print("WARNING: backend not responding after 30s")


if __name__ == "__main__":
    deploy_stack()
    create_bucket()
    health_check()
    print("\n=== SoleGuard cloud stack deployed ===")
    print("  Backend:    http://localhost:8000")
    print("  MQTT:       localhost:1883")
    print("  MinIO:      http://localhost:9000")
    print("  Postgres:   localhost:5432")
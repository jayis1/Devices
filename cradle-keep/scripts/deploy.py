#!/usr/bin/env python3
"""
CradleKeep — Deployment Script

Sets up the CradleKeep cloud dashboard (FastAPI + PostgreSQL + Mosquitto MQTT).

Usage:
    python3 deploy.py --local     # Deploy locally with Docker
    python3 deploy.py --cloud     # Deploy to cloud (AWS/GCP)
"""

import subprocess
import argparse
import os
import sys

def run_cmd(cmd, cwd=None):
    """Run a shell command and print output."""
    print(f"Running: {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout)
    if result.returncode != 0:
        print(f"ERROR: {result.stderr}")
        sys.exit(1)
    return result

def deploy_local():
    """Deploy CradleKeep dashboard locally using Docker Compose."""
    dashboard_dir = os.path.join(os.path.dirname(__file__), "..", "software", "dashboard")
    
    print("🚀 Deploying CradleKeep locally with Docker Compose...")
    print("=" * 50)
    
    # Build and start services
    print("\n1️⃣  Building Docker images...")
    run_cmd("docker-compose build", cwd=dashboard_dir)
    
    print("\n2️⃣  Starting services...")
    run_cmd("docker-compose up -d", cwd=dashboard_dir)
    
    print("\n3️⃣  Waiting for PostgreSQL to be ready...")
    import time
    time.sleep(5)
    
    print("\n4️⃣  Creating database tables...")
    # Tables are auto-created by FastAPI lifespan
    
    print("\n" + "=" * 50)
    print("✅ CradleKeep is running!")
    print()
    print("  📊 Dashboard:  http://localhost:8000")
    print("  🔌 API docs:   http://localhost:8000/docs")
    print("  📡 MQTT:       localhost:1883")
    print("  🗄️  PostgreSQL: localhost:5432")
    print()
    print("  📱 Mobile app: Connect to hub via BLE")
    print("=" * 50)

def deploy_cloud():
    """Deploy CradleKeep to cloud (AWS ECS or GCP Cloud Run)."""
    print("☁️  Cloud deployment requires:")
    print("   - AWS CLI configured (for ECS) or gcloud CLI (for Cloud Run)")
    print("   - Docker installed and logged in")
    print("   - Domain name configured (optional)")
    print()
    print("This is a placeholder for cloud deployment.")
    print("In production, you would:")
    print("   1. Push Docker images to ECR/GCR")
    print("   2. Set up RDS Postgres or Cloud SQL")
    print("   3. Deploy API to ECS Fargate or Cloud Run")
    print("   4. Deploy frontend to S3+CloudFront or Firebase Hosting")
    print("   5. Set up Mosquitto on ECS or use AWS IoT Core")
    print("   6. Configure SSL/TLS certificates")
    print("   7. Set up monitoring (CloudWatch or Stackdriver)")
    print("   8. Configure push notifications (APNs/FCM)")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="CradleKeep Deployment")
    parser.add_argument("--local", action="store_true", help="Deploy locally with Docker")
    parser.add_argument("--cloud", action="store_true", help="Deploy to cloud")
    args = parser.parse_args()
    
    if args.local:
        deploy_local()
    elif args.cloud:
        deploy_cloud()
    else:
        parser.print_help()
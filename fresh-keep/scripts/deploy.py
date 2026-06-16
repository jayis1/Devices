#!/usr/bin/env python3
"""
FreshKeep — Deployment Script

Sets up the FreshKeep cloud dashboard (FastAPI + PostgreSQL + Mosquitto MQTT).

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
    """Deploy FreshKeep dashboard locally using Docker Compose."""
    dashboard_dir = os.path.join(os.path.dirname(__file__), "..", "software", "dashboard")
    
    print("🚀 Deploying FreshKeep locally with Docker Compose...")
    print("=" * 50)
    
    # Create docker-compose.yml
    compose_content = """
version: '3.8'

services:
  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_DB: freshkeep
      POSTGRES_USER: freshkeep
      POSTGRES_PASSWORD: freshkeep
    ports:
      - "5432:5432"
    volumes:
      - postgres_data:/var/lib/postgresql/data
    restart: unless-stopped

  mosquitto:
    image: eclipse-mosquitto:2
    ports:
      - "1883:1883"
      - "9001:9001"
    volumes:
      - ./mosquitto.conf:/mosquitto/config/mosquitto.conf
    restart: unless-stopped

  api:
    build:
      context: ./backend
      dockerfile: Dockerfile
    ports:
      - "8000:8000"
    environment:
      DATABASE_URL: postgresql+asyncpg://freshkeep:freshkeep@postgres/freshkeep
      MQTT_BROKER: mosquitto
      MQTT_PORT: 1883
    depends_on:
      - postgres
      - mosquitto
    restart: unless-stopped

  frontend:
    build:
      context: ./frontend
      dockerfile: Dockerfile
    ports:
      - "3000:3000"
    depends_on:
      - api
    restart: unless-stopped

volumes:
  postgres_data:
"""
    
    with open(os.path.join(dashboard_dir, "docker-compose.yml"), "w") as f:
        f.write(compose_content)
    
    # Create mosquitto config
    mosquitto_conf = """
listener 1883
allow_anonymous true
listener 9001
protocol websockets
"""
    with open(os.path.join(dashboard_dir, "mosquitto.conf"), "w") as f:
        f.write(mosquitto_conf)
    
    # Create Dockerfile for API
    dockerfile_api = """
FROM python:3.11-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . .
EXPOSE 8000
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
"""
    os.makedirs(os.path.join(dashboard_dir, "backend"), exist_ok=True)
    with open(os.path.join(dashboard_dir, "backend", "Dockerfile"), "w") as f:
        f.write(dockerfile_api)
    
    # Start services
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
    print("✅ FreshKeep is running!")
    print()
    print("  📊 Dashboard:  http://localhost:3000")
    print("  🔌 API:        http://localhost:8000")
    print("  📡 MQTT:       localhost:1883")
    print("  🗄️  PostgreSQL: localhost:5432")
    print()
    print("  API docs:      http://localhost:8000/docs")
    print("=" * 50)

def deploy_cloud():
    """Deploy FreshKeep to cloud (AWS ECS or GCP Cloud Run)."""
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

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="FreshKeep Deployment")
    parser.add_argument("--local", action="store_true", help="Deploy locally with Docker")
    parser.add_argument("--cloud", action="store_true", help="Deploy to cloud")
    args = parser.parse_args()
    
    if args.local:
        deploy_local()
    elif args.cloud:
        deploy_cloud()
    else:
        parser.print_help()
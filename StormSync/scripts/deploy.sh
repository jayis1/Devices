#!/bin/bash
# StormSync — Cloud Deployment Script
# Deploys FastAPI backend, MQTT broker, InfluxDB, and PostgreSQL via Docker Compose

set -euo pipefail

echo "=========================================="
echo "  StormSync Cloud Backend Deployment"
echo "=========================================="

if ! command -v docker &>/dev/null; then
    echo "ERROR: Docker not installed. Install: https://docs.docker.com/get-docker/"
    exit 1
fi

if ! command -v docker-compose &>/dev/null && ! docker compose version &>/dev/null; then
    echo "ERROR: Docker Compose not installed."
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_DIR="${SCRIPT_DIR}/../software/dashboard"

cat > "${DEPLOY_DIR}/docker-compose.yml" << 'COMPOSE'
version: '3.8'

services:
  api:
    build: .
    ports:
      - "8000:8000"
    environment:
      - MQTT_BROKER_HOST=mosquitto
      - INFLUXDB_URL=http://influxdb:8086
      - INFLUXDB_TOKEN=${INFLUXDB_TOKEN:-stormsync-token}
      - INFLUXDB_ORG=stormsync
      - INFLUXDB_BUCKET=stormsync_telemetry
      - POSTGRES_DSN=postgresql://stormsync:***@postgres:5432/stormsync
      - JWT_SECRET=${JWT_SECRET:-change-in-production}
    depends_on:
      - mosquitto
      - influxdb
      - postgres
    restart: unless-stopped

  mosquitto:
    image: eclipse-mosquitto:2
    ports:
      - "1883:1883"
      - "9001:9001"
    volumes:
      - ./mosquitto.conf:/mosquitto/config/mosquitto.conf
      - mosquitto_data:/mosquitto/data
    restart: unless-stopped

  influxdb:
    image: influxdb:2.7
    ports:
      - "8086:8086"
    environment:
      - DOCKER_INFLUXDB_INIT_MODE=setup
      - DOCKER_INFLUXDB_INIT_USERNAME=stormsync
      - DOCKER_INFLUXDB_INIT_PASSWORD=stormsync123
      - DOCKER_INFLUXDB_INIT_ORG=stormsync
      - DOCKER_INFLUXDB_INIT_BUCKET=stormsync_telemetry
      - DOCKER_INFLUXDB_INIT_ADMIN_TOKEN=stormsync-token
    volumes:
      - influxdb_data:/var/lib/influxdb2
    restart: unless-stopped

  postgres:
    image: postgres:16
    ports:
      - "5432:5432"
    environment:
      - POSTGRES_DB=stormsync
      - POSTGRES_USER=stormsync
      - POSTGRES_PASSWORD=stormsync
    volumes:
      - postgres_data:/var/lib/postgresql/data
    restart: unless-stopped

volumes:
  mosquitto_data:
  influxdb_data:
  postgres_data:
COMPOSE

cat > "${DEPLOY_DIR}/mosquitto.conf" << 'MOSQ'
listener 1883
allow_anonymous true
listener 9001
protocol websockets
MOSQ

cat > "${DEPLOY_DIR}/Dockerfile" << 'DOCKER'
FROM python:3.11-slim

WORKDIR /app
COPY pyproject.toml .
RUN pip install --no-cache-dir -e .
COPY . .

EXPOSE 8000
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
DOCKER

echo ""
echo "[1/4] Building and starting containers..."
cd "${DEPLOY_DIR}"
docker compose up -d --build

echo ""
echo "[2/4] Waiting for services to start..."
sleep 10

echo ""
echo "[3/4] Health check..."
if curl -sf http://localhost:8000/health > /dev/null 2>&1; then
    echo "  ✓ API: healthy"
else
    echo "  ✗ API: not responding (may need more time)"
fi

if curl -sf http://localhost:8086/health > /dev/null 2>&1; then
    echo "  ✓ InfluxDB: healthy"
else
    echo "  ~ InfluxDB: starting..."
fi

echo ""
echo "[4/4] Service endpoints:"
echo "  API:        http://localhost:8000"
echo "  API docs:   http://localhost:8000/docs"
echo "  MQTT:       localhost:1883"
echo "  InfluxDB:   http://localhost:8086"
echo "  PostgreSQL: localhost:5432 (stormsync/stormsync)"
echo ""
echo "=========================================="
echo "  Deployment complete!"
echo "=========================================="
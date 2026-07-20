#!/usr/bin/env bash
# MosquitoSync — Cloud Deployment Script
#
# Deploys the FastAPI backend + MQTT broker + InfluxDB + PostgreSQL
# using Docker Compose on a Linux server.
#
# Usage: ./deploy.sh [production|staging]

set -euo pipefail

ENVIRONMENT="${1:-staging}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "============================================="
echo "  MosquitoSync — Cloud Deployment ($ENVIRONMENT)"
echo "============================================="

# Check dependencies
command -v docker >/dev/null 2>&1 || { echo "ERROR: docker not installed"; exit 1; }
command -v docker-compose >/dev/null 2>&1 || { echo "ERROR: docker-compose not installed"; exit 1; }

# Create docker-compose.yml if it doesn't exist
COMPOSE_FILE="$PROJECT_DIR/scripts/docker-compose.yml"
if [ ! -f "$COMPOSE_FILE" ]; then
    echo "[deploy] Creating docker-compose.yml..."
    cat > "$COMPOSE_FILE" << 'YAML'
version: "3.8"
services:
  api:
    build: ../software/dashboard
    ports: ["8080:8080"]
    environment:
      - JWT_SECRET=${JWT_SECRET:-mosquitosync-dev-secret}
      - POSTGRES_URL=postgres://mosquito:mosquito@db:5432/mosquitosync
      - INFLUX_URL=http://influxdb:8086
      - MQTT_BROKER=tcp://mosquitto:1883
    depends_on: [db, influxdb, mosquitto]
    restart: unless-stopped

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: mosquito
      POSTGRES_PASSWORD: mosquito
      POSTGRES_DB: mosquitosync
    volumes: ["pgdata:/var/lib/postgresql/data"]
    restart: unless-stopped

  influxdb:
    image: influxdb:2.7-alpine
    environment:
      DOCKER_INFLUXDB_INIT_MODE: setup
      DOCKER_INFLUXDB_INIT_USERNAME: mosquito
      DOCKER_INFLUXDB_INIT_PASSWORD: mosquitosync123
      DOCKER_INFLUXDB_INIT_ORG: mosquitosync
      DOCKER_INFLUXDB_INIT_BUCKET: telemetry
    volumes: ["influxdata:/var/lib/influxdb2"]
    restart: unless-stopped

  mosquitto:
    image: eclipse-mosquitto:2
    ports: ["1883:1883", "9001:9001"]
    volumes:
      - type: bind
        source: ./mosquitto.conf
        target: /mosquitto/config/mosquitto.conf
    restart: unless-stopped

volumes:
  pgdata:
  influxdata:
YAML

    cat > "$PROJECT_DIR/scripts/mosquitto.conf" << 'CONF'
listener 1883
allow_anonymous true
persistence true
persistence_location /mosquitto/data/
log_dest stdout
CONF
fi

# Build and start
echo "[deploy] Building containers..."
cd "$PROJECT_DIR/scripts"
docker-compose -f docker-compose.yml build

echo "[deploy] Starting services ($ENVIRONMENT)..."
docker-compose -f docker-compose.yml up -d

# Wait for services
echo "[deploy] Waiting for services to be ready..."
sleep 5

# Health check
API_HEALTH=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/api/v1/devices 2>/dev/null || echo "000")
if [ "$API_HEALTH" = "200" ]; then
    echo "[deploy] ✅ FastAPI backend: healthy (http://localhost:8080)"
else
    echo "[deploy] ⚠️  FastAPI backend: not ready yet (HTTP $API_HEALTH)"
fi

echo ""
echo "MosquitoSync cloud deployed!"
echo "  API:       http://localhost:8080"
echo "  MQTT:      tcp://localhost:1883"
echo "  InfluxDB:  http://localhost:8086"
echo "  PostgreSQL: localhost:5432"
echo ""
echo "Logs: docker-compose -f scripts/docker-compose.yml logs -f"
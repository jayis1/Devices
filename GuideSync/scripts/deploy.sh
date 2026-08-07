#!/bin/bash
# GuideSync — Cloud Deployment Script
# Deploys the FastAPI backend + MQTT broker + databases via Docker Compose
#
# Usage: ./deploy.sh [--build] [--dev]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DASHBOARD_DIR="$PROJECT_DIR/software/dashboard"

echo "╔══════════════════════════════════════════╗"
echo "║  GuideSync Cloud Deployment              ║"
echo "╚══════════════════════════════════════════╝"

# Check Docker
if ! command -v docker &> /dev/null; then
    echo "✗ Docker not installed. Install Docker first."
    exit 1
fi

# Check docker compose
if docker compose version &> /dev/null; then
    COMPOSE="docker compose"
elif command -v docker-compose &> /dev/null; then
    COMPOSE="docker-compose"
else
    echo "✗ Docker Compose not installed."
    exit 1
fi

BUILD_FLAG=""
DEV_FLAG=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --build) BUILD_FLAG="--build"; shift ;;
        --dev) DEV_FLAG="--env-file .env.dev"; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Create docker-compose.yml if not exists
if [ ! -f "$DASHBOARD_DIR/docker-compose.yml" ]; then
    echo "Creating docker-compose.yml..."
    cat > "$DASHBOARD_DIR/docker-compose.yml" << 'EOF'
version: '3.8'

services:
  api:
    build: .
    ports:
      - "8080:8080"
    environment:
      - JWT_SECRET=${JWT_SECRET:-guidesync-secret}
      - MQTT_BROKER=mosquitto:1883
      - POSTGRES_URL=postgresql://guidesync:guidesync@db:5432/guidesync
      - INFLUX_URL=http://influxdb:8086
    depends_on:
      - mosquitto
      - db
      - influxdb
    restart: unless-stopped

  mosquitto:
    image: eclipse-mosquitto:2
    ports:
      - "1883:1883"
      - "9001:9001"
    volumes:
      - ./mosquitto.conf:/mosquitto/config/mosquitto.conf
    restart: unless-stopped

  db:
    image: postgres:16-alpine
    environment:
      - POSTGRES_DB=guidesync
      - POSTGRES_USER=guidesync
      - POSTGRES_PASSWORD=guidesync
    volumes:
      - pgdata:/var/lib/postgresql/data
    ports:
      - "5432:5432"
    restart: unless-stopped

  influxdb:
    image: influxdb:2-alpine
    environment:
      - DOCKER_INFLUXDB_INIT_MODE=setup
      - DOCKER_INFLUXDB_INIT_USERNAME=guidesync
      - DOCKER_INFLUXDB_INIT_PASSWORD=guidesync123
      - DOCKER_INFLUXDB_INIT_ORG=guidesync
      - DOCKER_INFLUXDB_INIT_BUCKET=telemetry
    volumes:
      - influxdata:/var/lib/influxdb2
    ports:
      - "8086:8086"
    restart: unless-stopped

volumes:
  pgdata:
  influxdata:
EOF
fi

# Create Dockerfile if not exists
if [ ! -f "$DASHBOARD_DIR/Dockerfile" ]; then
    echo "Creating Dockerfile..."
    cat > "$DASHBOARD_DIR/Dockerfile" << 'EOF'
FROM python:3.11-slim

WORKDIR /app
COPY pyproject.toml .
RUN pip install --no-cache-dir fastapi uvicorn[standard] pydantic paho-mqtt python-jose[cryptography] psycopg2-binary influxdb-client websockets
COPY . .

EXPOSE 8080
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8080"]
EOF
fi

# Create mosquitto config if not exists
if [ ! -f "$DASHBOARD_DIR/mosquitto.conf" ]; then
    echo "Creating mosquitto.conf..."
    cat > "$DASHBOARD_DIR/mosquitto.conf" << 'EOF'
listener 1883
allow_anonymous true
listener 9001
protocol websockets
EOF
fi

echo "Starting GuideSync cloud services..."
cd "$DASHBOARD_DIR"
$COMPOSE up -d $BUILD_FLAG $DEV_FLAG

echo ""
echo "✓ GuideSync cloud deployed!"
echo "  API:         http://localhost:8080"
echo "  MQTT:        localhost:1883"
echo "  PostgreSQL:  localhost:5432"
echo "  InfluxDB:    http://localhost:8086"
echo ""
echo "Test: curl http://localhost:8080/api/v1/devices"
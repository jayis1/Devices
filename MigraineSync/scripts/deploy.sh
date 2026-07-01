#!/bin/bash
# MigraineSync — Deploy Cloud Backend
# ====================================
# Deploys the FastAPI backend + TimescaleDB + MQTT broker
# using Docker Compose.
#
# License: MIT

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DASHBOARD_DIR="$SCRIPT_DIR/../software/dashboard"

echo "============================================"
echo "  MigraineSync — Cloud Deployment"
echo "============================================"

# Check Docker
if ! command -v docker &> /dev/null; then
    echo "ERROR: Docker not installed. Install Docker first."
    exit 1
fi

if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
    echo "ERROR: Docker Compose not installed."
    exit 1
fi

# Create docker-compose.yml if it doesn't exist
COMPOSE_FILE="$DASHBOARD_DIR/docker-compose.yml"
if [ ! -f "$COMPOSE_FILE" ]; then
    cat > "$COMPOSE_FILE" << 'EOF'
version: '3.8'

services:
  backend:
    build: .
    ports:
      - "8000:8000"
    environment:
      - DATABASE_URL=postgresql://migrainesync:migrainesync@db:5432/migrainesync
      - MQTT_BROKER=mosquitto
      - MQTT_PORT=1883
    depends_on:
      - db
      - mosquitto
    restart: unless-stopped

  db:
    image: timescale/timescaledb:latest-pg15
    environment:
      - POSTGRES_DB=migrainesync
      - POSTGRES_USER=migrainesync
      - POSTGRES_PASSWORD=migrainesync
    volumes:
      - migrainesync_db:/var/lib/postgresql/data
    ports:
      - "5432:5432"
    restart: unless-stopped

  mosquitto:
    image: eclipse-mosquitto:2
    ports:
      - "1883:1883"
      - "9001:9001"
    volumes:
      - ./mosquitto.conf:/mosquitto/config/mosquitto.conf
    restart: unless-stopped

volumes:
  migrainesync_db:
EOF

    # Create mosquitto config
    cat > "$DASHBOARD_DIR/mosquitto.conf" << 'EOF'
listener 1883
allow_anonymous true
listener 9001
protocol websockets
EOF
fi

echo "Starting Docker Compose..."
cd "$DASHBOARD_DIR"
docker compose up -d --build

echo ""
echo "============================================"
echo "  Deployment complete!"
echo "  Backend:  http://localhost:8000"
echo "  API docs: http://localhost:8000/docs"
echo "  Database: localhost:5432"
echo "  MQTT:     localhost:1883"
echo "============================================"
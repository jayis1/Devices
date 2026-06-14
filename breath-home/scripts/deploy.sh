#!/bin/bash
# BreathHome - Deployment Script
# Deploys the cloud backend (FastAPI + MQTT + PostgreSQL + ML worker)

set -e

echo "========================================="
echo "  BreathHome Cloud Deployment"
echo "========================================="

# Check dependencies
command -v docker >/dev/null 2>&1 || { echo "Error: docker not found"; exit 1; }
command -v docker-compose >/dev/null 2>&1 || command -v docker >/dev/null 2>&1 || { echo "Error: docker-compose not found"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DASHBOARD_DIR="$SCRIPT_DIR/../software/dashboard"

echo ""
echo "[1/5] Creating Mosquitto configuration..."
mkdir -p "$DASHBOARD_DIR/mosquitto_data"
cat > "$DASHBOARD_DIR/mosquitto.conf" << 'EOF'
listener 1883
allow_anonymous false
password_file /mosquitto/config/passwords

listener 9001
protocol websockets
allow_anonymous false
password_file /mosquitto/config/passwords

persistence true
persistence_location /mosquitto/data/
log_dest stdout
EOF

echo "[2/5] Creating MQTT password file..."
docker run --rm -v "$DASHBOARD_DIR/mosquitto_data:/mosquitto/config" \
  eclipse-mosquitto:2 \
  mosquitto_passwd -b /mosquitto/config/passwords breathhome breathhome_secret_2026

echo "[3/5] Creating environment file..."
cat > "$DASHBOARD_DIR/.env" << 'EOF'
DATABASE_URL=postgresql://breathhome:breathhome_secret_2026@db:5432/breathhome
MQTT_BROKER=mosquitto
MQTT_PORT=1883
MQTT_USERNAME=breathhome
MQTT_PASSWORD=breathhome_secret_2026
SECRET_KEY=change-this-in-production
EOF

echo "[4/5] Building Docker images..."
cd "$DASHBOARD_DIR"
docker-compose build

echo "[5/5] Starting services..."
docker-compose up -d

echo ""
echo "========================================="
echo "  BreathHome Cloud is running!"
echo "========================================="
echo ""
echo "  Dashboard API:  http://localhost:8000"
echo "  API Docs:       http://localhost:8000/docs"
echo "  MQTT Broker:     mqtt://localhost:1883"
echo "  MQTT WebSocket:  ws://localhost:9001"
echo "  PostgreSQL:      localhost:5432"
echo ""
echo "  Default credentials:"
echo "    MQTT: breathhome / breathhome_secret_2026"
echo "    DB:   breathhome / breathhome_secret_2026"
echo ""
echo "  To view logs: docker-compose logs -f"
echo "  To stop:       docker-compose down"
echo ""
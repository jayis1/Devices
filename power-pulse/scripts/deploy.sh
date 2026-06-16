#!/bin/bash
# PowerPulse Deployment Script
# Sets up the cloud backend (FastAPI + TimescaleDB + Mosquitto MQTT)

set -e

echo "=== PowerPulse Cloud Deployment ==="

# Check Docker
if ! command -v docker &> /dev/null; then
    echo "Error: Docker is not installed"
    exit 1
fi

if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
    echo "Error: Docker Compose is not installed"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DASHBOARD_DIR="$(dirname "$SCRIPT_DIR")/software/dashboard"

echo ""
echo "[1/4] Creating Mosquitto config..."
mkdir -p "$DASHBOARD_DIR/mosquitto_data"
mkdir -p "$DASHBOARD_DIR/mosquitto_log"
cat > "$DASHBOARD_DIR/mosquitto.conf" << 'EOF'
listener 1883
allow_anonymous true
listener 9001
protocol websockets
max_connections -1
persistence true
persistence_location /mosquitto/data/
log_dest file /mosquitto/log/mosquitto.log
EOF

echo "[2/4] Building API image..."
cd "$DASHBOARD_DIR"
docker build -t powerpulse-api:latest .

echo "[3/4] Starting services..."
docker compose up -d

echo "[4/4] Waiting for services to be ready..."
echo -n "Waiting for TimescaleDB..."
for i in $(seq 1 30); do
    if docker compose exec db pg_isready -U powerpulse &> /dev/null; then
        echo " Ready!"
        break
    fi
    echo -n "."
    sleep 1
done

echo -n "Waiting for MQTT broker..."
for i in $(seq 1 30); do
    if docker compose exec mosquitto mosquitto_pub -t "test" -m "test" &> /dev/null; then
        echo " Ready!"
        break
    fi
    echo -n "."
    sleep 1
done

echo -n "Waiting for API..."
for i in $(seq 1 30); do
    if curl -s http://localhost:8000/health | grep -q "ok"; then
        echo " Ready!"
        break
    fi
    echo -n "."
    sleep 1
done

echo ""
echo "=== PowerPulse Cloud Deployment Complete ==="
echo ""
echo "Services:"
echo "  API:       http://localhost:8000"
echo "  API docs:  http://localhost:8000/docs"
echo "  MQTT:      mqtt://localhost:1883"
echo "  MQTT WS:   ws://localhost:9001"
echo "  Database:  localhost:5432"
echo ""
echo "To view logs:  docker compose logs -f"
echo "To stop:        docker compose down"
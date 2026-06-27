#!/bin/bash
# Deploy CompostSync cloud backend
# Usage: ./scripts/deploy_backend.sh

set -e

echo "=== CompostSync Backend Deployment ==="

# Check dependencies
command -v python3 >/dev/null || { echo "Python3 required"; exit 1; }
command -v docker >/dev/null || { echo "Docker required for DB/MQTT"; exit 1; }

# Create virtualenv
cd "$(dirname "$0")/../software/dashboard"
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# Start TimescaleDB
echo "Starting TimescaleDB..."
docker run -d --name compostsync-db \
  -e POSTGRES_PASSWORD=compost \
  -e POSTGRES_USER=compost \
  -e POSTGRES_DB=compostsync \
  -p 5432:5432 \
  timescale/timescaledb:latest 2>/dev/null || echo "DB container already running"

# Start Mosquitto MQTT
echo "Starting Mosquitto MQTT broker..."
docker run -d --name compostsync-mqtt \
  -p 1883:1883 \
  eclipse-mosquitto 2>/dev/null || echo "MQTT container already running"

# Wait for services
echo "Waiting for services to start..."
sleep 5

# Start backend
echo "Starting FastAPI backend..."
export DATABASE_URL="postgresql://compost:compost@localhost/compostsync"
export MQTT_HOST="localhost"
export MQTT_PORT="1883"
export MODEL_DIR="../ml-pipeline/models"

uvicorn main:app --host 0.0.0.0 --port 8000 --reload &

echo ""
echo "=== Backend running at http://localhost:8000 ==="
echo "=== API docs at http://localhost:8000/docs ==="
echo ""
echo "To start ML pipeline in another terminal:"
echo "  cd software/ml-pipeline && python inference.py"
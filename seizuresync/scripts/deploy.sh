#!/bin/bash
# SeizureSync — Deploy backend services
# Usage: ./deploy.sh [dev|prod]
set -e
ENV="${1:-dev}"
echo "Deploying SeizureSync backend ($ENV)..."

# Start Mosquitto MQTT broker
if ! pgrep mosquitto > /dev/null; then
    echo "Starting Mosquitto..."
    mosquitto -d
fi

# Start Redis (for Celery)
if ! pgrep redis-server > /dev/null; then
    echo "Starting Redis..."
    redis-server --daemonize yes
fi

# Start TimescaleDB
if ! pgrep postgres > /dev/null; then
    echo "Starting PostgreSQL/TimescaleDB..."
    # Assumes TimescaleDB is installed
    pg_ctlcluster 15 main start 2>/dev/null || true
fi

# Start FastAPI backend
cd software/dashboard
source .venv/bin/activate 2>/dev/null || true
pip install -r requirements.txt -q

if [ "$ENV" = "prod" ]; then
    uvicorn main:app --host 0.0.0.0 --port 8000 --workers 4 &
else
    uvicorn main:app --host 0.0.0.0 --port 8000 --reload &
fi

echo "SeizureSync backend deployed at http://localhost:8000"
echo "API docs at http://localhost:8000/docs"
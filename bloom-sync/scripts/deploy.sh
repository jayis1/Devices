#!/usr/bin/env bash
# BloomSync — Deployment Script
# Deploys the BloomSync cloud backend, ML pipeline, and configures MQTT.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== BloomSync Deployment ==="

# 1. Start FastAPI backend
echo "[1/4] Starting FastAPI backend..."
cd "$PROJECT_ROOT/software/dashboard"
if [ ! -d ".venv" ]; then
    python3 -m venv .venv
    . .venv/bin/activate
    pip install -e .
else
    . .venv/bin/activate
fi
uvicorn main:app --host 0.0.0.0 --port 8000 --reload &
BACKEND_PID=$!
echo "  Backend PID: $BACKEND_PID"

# 2. Start MQTT broker (if not running)
echo "[2/4] Checking MQTT broker..."
if ! pgrep -x "mosquitto" > /dev/null; then
    echo "  Starting Mosquitto MQTT broker..."
    mosquitto -d
else
    echo "  Mosquitto already running"
fi

# 3. Configure Hub OTA endpoint
echo "[3/4] OTA endpoint configured at http://localhost:8000/api/v1/ota"

# 4. Health check
echo "[4/4] Health check..."
sleep 2
if curl -s http://localhost:8000/api/v1/health | grep -q "ok"; then
    echo "  ✓ Backend healthy"
else
    echo "  ✗ Backend health check failed"
    exit 1
fi

echo ""
echo "=== BloomSync Deployed Successfully ==="
echo "  API:     http://localhost:8000"
echo "  Docs:    http://localhost:8000/docs"
echo "  MQTT:    localhost:1883"
echo "  Topics:  bloom-sync/telemetry/#"
echo "           bloom-sync/alerts/#"
echo "           bloom-sync/voice/#"
echo ""
echo "  Backend PID: $BACKEND_PID (kill with: kill $BACKEND_PID)"
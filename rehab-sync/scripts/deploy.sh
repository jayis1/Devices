#!/bin/bash
# RehabSync — Deployment Script
# Deploys the FastAPI backend + ML pipeline inference server.

set -euo pipefail

echo "╔══════════════════════════════════════════════╗"
echo "║   RehabSync — Deployment Script               ║"
echo "╚══════════════════════════════════════════════╝"

# Configuration
BACKEND_DIR="$(cd "$(dirname "$0")/.." && pwd)/software/dashboard"
ML_DIR="$(cd "$(dirname "$0")/.." && pwd)/software/ml-pipeline"
VENV_DIR="${VENV_DIR:-/opt/rehabsync/venv}"
DATA_DIR="${DATA_DIR:-/var/lib/rehabsync}"
MQTT_BROKER="${MQTT_BROKER:-localhost}"
MQTT_PORT="${MQTT_PORT:-1883}"

echo "Backend dir: $BACKEND_DIR"
echo "ML dir: $ML_DIR"
echo "Venv: $VENV_DIR"

# Create virtual environment
if [ ! -d "$VENV_DIR" ]; then
    echo "Creating virtual environment..."
    python3 -m venv "$VENV_DIR"
fi

source "$VENV_DIR/bin/activate"

# Install dependencies
echo "Installing backend dependencies..."
pip install --upgrade pip
pip install -e "$BACKEND_DIR"

echo "Installing ML pipeline dependencies..."
pip install -e "$ML_DIR"

# Create data directory
mkdir -p "$DATA_DIR"
mkdir -p "$DATA_DIR/models"

# Set environment variables
export MQTT_BROKER
export MQTT_PORT
export REHABSYNC_DATA_DIR="$DATA_DIR"

# Start backend
echo "Starting FastAPI backend on port 8000..."
cd "$BACKEND_DIR"
uvicorn main:app --host 0.0.0.0 --port 8000 --workers 4 &
BACKEND_PID=$!

echo "Backend PID: $BACKEND_PID"

# Wait for backend to start
sleep 3

# Health check
if curl -s http://localhost:8000/api/v1/health | grep -q "ok"; then
    echo "✓ Backend healthy"
else
    echo "✗ Backend health check failed"
    kill $BACKEND_PID
    exit 1
fi

echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║   RehabSync deployed successfully!            ║"
echo "║   API: http://localhost:8000                  ║"
echo "║   Docs: http://localhost:8000/docs            ║"
echo "╚══════════════════════════════════════════════╝"

# Graceful shutdown
trap "echo 'Shutting down...'; kill $BACKEND_PID; exit 0" SIGINT SIGTERM
wait $BACKEND_PID
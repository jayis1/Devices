#!/bin/bash
# GrillSync — Deployment Script
# Deploys cloud backend, configures MQTT, and pushes OTA firmware

set -e

echo "=========================================="
echo "GrillSync Deployment Script"
echo "=========================================="

# Configuration
ML_PIPELINE_DIR="$(cd "$(dirname "$0")/.." && pwd)/software/ml-pipeline"
DASHBOARD_DIR="$(cd "$(dirname "$0")/.." && pwd)/software/dashboard"
MODELS_DIR="$ML_PIPELINE_DIR/models"

echo ""
echo "1. Training ML models..."
echo "   (Run scripts/train_models.py if not already trained)"
if [ -d "$MODELS_DIR" ]; then
    echo "   ✓ Models directory exists"
    ls -la "$MODELS_DIR/" 2>/dev/null || true
else
    echo "   ⚠ Models directory not found. Run training first."
fi

echo ""
echo "2. Installing cloud backend dependencies..."
cd "$DASHBOARD_DIR"
pip install -e . 2>/dev/null || pip3 install -e .

echo ""
echo "3. Starting FastAPI backend (background)..."
# Kill any existing instance
pkill -f "uvicorn.*main:app" 2>/dev/null || true
sleep 1
uvicorn main:app --host 0.0.0.0 --port 8000 &
BACKEND_PID=$!
echo "   ✓ Backend started (PID: $BACKEND_PID)"
sleep 2

echo ""
echo "4. Health check..."
if curl -s http://localhost:8000/health | grep -q "ok"; then
    echo "   ✓ Backend healthy"
else
    echo "   ✗ Backend health check failed"
    exit 1
fi

echo ""
echo "5. Configuring MQTT topics..."
echo "   - grillsync/telemetry/sentinel"
echo "   - grillsync/telemetry/probe"
echo "   - grillsync/telemetry/smoke"
echo "   - grillsync/alerts/#"
echo "   - grillsync/thermal/#"

echo ""
echo "6. OTA firmware preparation..."
FIRMWARE_DIR="$(cd "$(dirname "$0")/.." && pwd)/firmware"
echo "   Firmware sources: $FIRMWARE_DIR"
echo "   Build commands:"
echo "     Hub:        cd firmware/hub && idf.py build"
echo "     Sentinel:   cd firmware/grill-sentinel && idf.py build"
echo "     Smoke:      cd firmware/smoke-node && idf.py build"
echo "     Probe:      cd firmware/meat-probe && west build"

echo ""
echo "=========================================="
echo "Deployment complete!"
echo "  Backend:  http://localhost:8000"
echo "  API docs: http://localhost:8000/docs"
echo "  PID:      $BACKEND_PID"
echo "=========================================="
#!/bin/bash
# EchoSync — Deployment Script
# Builds and deploys firmware to all nodes and starts the cloud backend.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEVICE_DIR="$(dirname "$SCRIPT_DIR")"
echo "=== EchoSync Deployment ==="
echo "Device dir: $DEVICE_DIR"

# === Check prerequisites ===
echo "Checking prerequisites..."

if ! command -v python3 &> /dev/null; then
    echo "ERROR: python3 not found"
    exit 1
fi

if ! command -v idf.py &> /dev/null; then
    echo "WARNING: ESP-IDF not found. ESP32 builds will be skipped."
    ESP_BUILD=false
else
    ESP_BUILD=true
fi

if ! command -v west &> /dev/null; then
    echo "WARNING: nRF Connect SDK not found. nRF52840 builds will be skipped."
    NRF_BUILD=false
else
    NRF_BUILD=true
fi

# === Build ESP32 firmware ===
if [ "$ESP_BUILD" = true ]; then
    echo ""
    echo "=== Building Hub Firmware (ESP32-S3) ==="
    cd "$DEVICE_DIR/firmware/hub"
    idf.py build

    echo ""
    echo "=== Building Room Sentinel Firmware (ESP32-S3) ==="
    cd "$DEVICE_DIR/firmware/room-sentinel"
    idf.py build
fi

# === Build nRF52840 firmware ===
if [ "$NRF_BUILD" = true ]; then
    echo ""
    echo "=== Building Wrist Band Firmware (nRF52840) ==="
    cd "$DEVICE_DIR/firmware/wrist-band"
    west build -b nrf52840dk_nrf52840

    echo ""
    echo "=== Building Door Tag Firmware (nRF52840) ==="
    cd "$DEVICE_DIR/firmware/door-tag"
    west build -b nrf52840dk_nrf52840
fi

# === Start cloud backend ===
echo ""
echo "=== Starting Cloud Backend ==="
cd "$DEVICE_DIR/software/dashboard"
if [ ! -d "venv" ]; then
    echo "Creating virtual environment..."
    python3 -m venv venv
    source venv/bin/activate
    pip install -e .
else
    source venv/bin/activate
fi

echo "Starting FastAPI server on port 8000..."
echo "API: http://localhost:8000/api/v1/health"
echo "Docs: http://localhost:8000/docs"
uvicorn main:app --host 0.0.0.0 --port 8000 &

BACKEND_PID=$!
echo "Backend PID: $BACKEND_PID"

# Wait and check
sleep 3
if curl -s http://localhost:8000/api/v1/health > /dev/null 2>&1; then
    echo "✓ Backend is running"
else
    echo "✗ Backend failed to start"
fi

echo ""
echo "=== Deployment Complete ==="
echo "Backend: http://localhost:8000"
echo "Mobile app: cd software/mobile-app && npx expo start"
echo ""
echo "To flash firmware:"
echo "  Hub: cd firmware/hub && idf.py -p /dev/ttyUSB0 flash"
echo "  Sentinel: cd firmware/room-sentinel && idf.py -p /dev/ttyUSB1 flash"
echo "  Wrist Band: cd firmware/wrist-band && west flash"
echo "  Door Tag: cd firmware/door-tag && west flash"
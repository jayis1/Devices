#!/bin/bash
# ErgoFlow — Deployment Script
# Sets up the FastAPI backend, MQTT broker, and systemd services
#
# Usage: ./deploy.sh [backend|mqtt|all]
# Copyright (c) 2026 jayis1. MIT License.

set -e

ERGOFLOW_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BACKEND_DIR="${ERGOFLOW_DIR}/software/dashboard"
VENV_DIR="${ERGOFLOW_DIR}/.venv"
MQTT_CONF="/etc/mosquitto/conf.d/ergoflow.conf"
SYSTEMD_DIR="/etc/systemd/system"

echo "═══════════════════════════════════════════"
echo "  ErgoFlow Deployment Utility"
echo "═══════════════════════════════════════════"
echo ""
echo "Project directory: ${ERGOFLOW_DIR}"
echo ""

deploy_backend() {
    echo "━━━ Deploying FastAPI Backend ━━━"
    echo ""

    # Install Python dependencies
    echo "Creating virtual environment..."
    python3 -m venv "${VENV_DIR}"
    source "${VENV_DIR}/bin/activate"

    echo "Installing Python dependencies..."
    pip install --upgrade pip
    pip install -r "${BACKEND_DIR}/requirements.txt"

    # Create systemd service
    echo "Creating systemd service..."
    cat > "${SYSTEMD_DIR}/ergoflow-api.service" << EOF
[Unit]
Description=ErgoFlow FastAPI Backend
After=network.target mosquitto.service

[Service]
Type=simple
User=root
WorkingDirectory=${BACKEND_DIR}
ExecStart=${VENV_DIR}/bin/uvicorn main:app --host 0.0.0.0 --port 8000 --workers 2
Restart=always
RestartSec=5
Environment=PYTHONPATH=${BACKEND_DIR}

[Install]
WantedBy=multi-user.target
EOF

    systemctl daemon-reload
    systemctl enable ergoflow-api
    systemctl restart ergoflow-api

    echo ""
    echo "✅ Backend deployed and running on port 8000"
}

deploy_mqtt() {
    echo "━━━ Deploying MQTT Broker ━━━"
    echo ""

    # Install Mosquitto
    echo "Installing Mosquitto MQTT broker..."
    apt-get update -qq
    apt-get install -y -qq mosquitto mosquitto-clients

    # Configure
    echo "Configuring Mosquitto..."
    mkdir -p /etc/mosquitto/conf.d
    cat > "${MQTT_CONF}" << EOF
# ErgoFlow MQTT Configuration
listener 1883 0.0.0.0
allow_anonymous true
max_connections 1000

# WebSocket listener for mobile app
listener 9001 0.0.0.0
protocol websockets
allow_anonymous true
EOF

    systemctl enable mosquitto
    systemctl restart mosquitto

    echo ""
    echo "✅ MQTT broker running on port 1883 (TCP) and 9001 (WebSocket)"
}

deploy_all() {
    deploy_mqtt
    echo ""
    deploy_backend
    echo ""
    echo "═══════════════════════════════════════════"
    echo "  ✅ ErgoFlow fully deployed!"
    echo "═══════════════════════════════════════════"
    echo ""
    echo "  Backend: http://localhost:8000"
    echo "  API docs: http://localhost:8000/docs"
    echo "  MQTT:     localhost:1883"
    echo "  WebSocket: ws://localhost:9001"
    echo ""
    echo "  Services:"
    echo "    sudo systemctl status ergoflow-api"
    echo "    sudo systemctl status mosquitto"
    echo ""
}

case "${1:-all}" in
    backend)
        deploy_backend
        ;;
    mqtt)
        deploy_mqtt
        ;;
    all)
        deploy_all
        ;;
    *)
        echo "Usage: $0 [backend|mqtt|all]"
        exit 1
        ;;
esac
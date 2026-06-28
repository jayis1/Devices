#!/bin/bash
# PestSync — Deploy Backend
# scripts/deploy_backend.sh
#
# Deploys the FastAPI backend + MQTT broker + ML inference service.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DASHBOARD_DIR="$SCRIPT_DIR/../software/dashboard"
ML_DIR="$SCRIPT_DIR/../software/ml-pipeline"

echo "🚀 PestSync Backend Deployment"
echo ""

# 1. Install system dependencies
echo "📦 Installing system dependencies..."
apt-get update -qq
apt-get install -y -qq mosquitto mosquitto-clients postgresql python3-pip python3-venv

# 2. Setup database
echo ""
echo "🗄️  Setting up PostgreSQL..."
sudo -u postgres psql -c "CREATE USER pestsync WITH PASSWORD 'pestsync' SUPERUSER;" 2>/dev/null || true
sudo -u postgres psql -c "CREATE DATABASE pestsync OWNER pestsync;" 2>/dev/null || true

# 3. Setup Python virtual environment for dashboard
echo ""
echo "🐍 Setting up Python environment for dashboard..."
python3 -m venv "$DASHBOARD_DIR/venv"
source "$DASHBOARD_DIR/venv/bin/activate"
pip install -q -r "$DASHBOARD_DIR/requirements.txt"

# 4. Setup Python virtual environment for ML pipeline
echo ""
echo "🧠 Setting up Python environment for ML pipeline..."
python3 -m venv "$ML_DIR/venv"
source "$ML_DIR/venv/bin/activate"
pip install -q -r "$ML_DIR/requirements.txt"

# 5. Configure Mosquitto
echo ""
echo "📡 Configuring Mosquitto MQTT broker..."
cat > /etc/mosquitto/conf.d/pestsync.conf << 'EOF'
listener 1883
allow_anonymous true
persistence true
persistence_location /var/lib/mosquitto/
EOF
systemctl restart mosquitto

# 6. Start backend
echo ""
echo "🌐 Starting FastAPI backend..."
source "$DASHBOARD_DIR/venv/bin/activate"
cd "$DASHBOARD_DIR"
nohup uvicorn main:app --host 0.0.0.0 --port 8000 > /var/log/pestsync_backend.log 2>&1 &
echo "   Backend running on port 8000 (PID: $!)"

# 7. Start ML inference service
echo ""
echo "🧠 Starting ML inference service..."
source "$ML_DIR/venv/bin/activate"
cd "$ML_DIR"
nohup python inference.py > /var/log/pestsync_ml.log 2>&1 &
echo "   ML service running (PID: $!)"

echo ""
echo "✅ PestSync backend deployed!"
echo "   API: http://localhost:8000"
echo "   Docs: http://localhost:8000/docs"
echo "   MQTT: localhost:1883"
echo ""
echo "   Logs: /var/log/pestsync_backend.log, /var/log/pestsync_ml.log"
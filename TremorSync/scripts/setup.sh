#!/bin/bash
# TremorSync Setup Script
# Installs dependencies and starts cloud backend

set -e

echo "=== TremorSync Setup ==="

# Check prerequisites
check_cmd() {
    if ! command -v $1 &> /dev/null; then
        echo "ERROR: $1 not found. Please install it."
        exit 1
    fi
}

check_cmd python3
check_cmd node
check_cmd docker

echo "[1/5] Setting up Python environment..."
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip

echo "[2/5] Installing dashboard dependencies..."
cd software/dashboard
pip install -r requirements.txt
cd ../..

echo "[3/5] Installing ML pipeline dependencies..."
cd software/ml-pipeline
pip install -r requirements.txt
cd ../..

echo "[4/5] Starting cloud backend (Docker)..."
cd software/dashboard
if [ -f docker-compose.yml ]; then
    docker-compose up -d
else
    echo "  No docker-compose.yml — starting backend directly"
    python3 main.py &
fi
cd ../..

echo "[5/5] Setting up mobile app..."
cd software/mobile-app
if [ -f package.json ]; then
    npm install
fi
cd ../..

echo ""
echo "=== Setup Complete ==="
echo "Cloud API: http://localhost:8000"
echo "API docs:  http://localhost:8000/docs"
echo ""
echo "Next steps:"
echo "  1. Flash firmware: cd firmware && pio run -e hub -t upload"
echo "  2. Train models:   cd software/ml-pipeline && python train_tremor_net.py"
echo "  3. Run mobile app: cd software/mobile-app && npx react-native run-android"
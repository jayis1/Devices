#!/bin/bash
# PostureSync Setup Script
# Installs dependencies and starts the cloud backend

set -e

echo "=== PostureSync Setup ==="

# Check prerequisites
echo "Checking prerequisites..."
command -v python3 >/dev/null 2>&1 || { echo "Error: python3 required"; exit 1; }
command -v node >/dev/null 2>&1 || { echo "Warning: node not found (needed for mobile app)"; }
command -v docker >/dev/null 2>&1 || { echo "Warning: docker not found (needed for backend)"; }

# Create virtual environment for ML pipeline
echo "Setting up ML pipeline environment..."
cd software/ml-pipeline
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
deactivate
cd ../../

# Create virtual environment for dashboard
echo "Setting up dashboard environment..."
cd software/dashboard
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
deactivate
cd ../../

# Setup mobile app
if command -v node >/dev/null 2>&1; then
    echo "Setting up mobile app..."
    cd software/mobile-app
    npm install
    cd ../../
fi

# Install PlatformIO for firmware
if ! command -v pio >/dev/null 2>&1; then
    echo "Installing PlatformIO..."
    python3 -m pip install platformio
fi

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Next steps:"
echo "  1. Start cloud backend:  cd software/dashboard && source venv/bin/activate && python main.py"
echo "  2. Train ML models:      cd software/ml-pipeline && source venv/bin/activate && python train_posture_cnn.py"
echo "  3. Flash firmware:       cd firmware && pio run -e hub -t upload"
echo "  4. Run mobile app:       cd software/mobile-app && npx react-native run-android"
echo ""
echo "See docs/ for detailed assembly and API documentation."
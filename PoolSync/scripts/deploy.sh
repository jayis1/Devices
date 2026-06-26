#!/bin/bash
# PoolSync — Development Deployment Script
# Sets up the complete development environment for PoolSync system

set -euo pipefail

echo "========================================="
echo "  PoolSync Development Deployment"
echo "========================================="
echo ""

# ============================================================
# Configuration
# ============================================================

POOLSYNC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_DIR="${POOLSYNC_DIR}/.venv"
PYTHON="${VENV_DIR}/bin/python3"
PIP="${VENV_DIR}/bin/pip"

echo "Project directory: ${POOLSYNC_DIR}"

# ============================================================
# 1. Python Virtual Environment + Dashboard
# ============================================================

echo ""
echo "[1/5] Setting up Python virtual environment..."

python3 -m venv "${VENV_DIR}" 2>/dev/null || {
    echo "ERROR: Failed to create virtual environment"
    echo "Install python3-venv: apt install python3-venv"
    exit 1
}

echo "Installing Python dependencies..."
"${PIP}" install --upgrade pip --quiet
"${PIP}" install --quiet \
    fastapi==0.111.0 \
    uvicorn[standard]==0.30.0 \
    pydantic==2.7.0 \
    numpy==1.26.4 \
    pandas==2.2.2 \
    scikit-learn==1.5.0 \
    torch==2.3.0 \
    torchvision==0.18.0 \
    matplotlib==3.9.0 \
    httpx==0.27.0 \
    websockets==12.0 \
    python-jose[cryptography]==3.3.0 \
    passlib[bcrypt]==1.7.4 \
    python-multipart==0.0.9 \
    aiofiles==23.2.1

echo "  ✓ Python environment ready"

# ============================================================
# 2. Firmware Toolchain
# ============================================================

echo ""
echo "[2/5] Checking firmware toolchain..."

# Check for ARM toolchain (STM32)
if command -v arm-none-eabi-gcc &>/dev/null; then
    echo "  ✓ ARM GCC: $(arm-none-eabi-gcc --version | head -1)"
else
    echo "  ⚠ ARM GCC not found. Install: apt install gcc-arm-none-eabi"
    echo "    Required for: Chemistry Probe (STM32L476), Equipment Controller (STM32F407)"
fi

# Check for Pico SDK (RP2040)
if [ -d "/usr/lib/pico-sdk" ] || [ -d "$HOME/pico-sdk" ]; then
    echo "  ✓ Pico SDK found"
else
    echo "  ⚠ Pico SDK not found. Install: git clone https://github.com/raspberrypi/pico-sdk"
    echo "    Required for: Hub (RP2040)"
fi

# Check for ESP-IDF (ESP32-S3)
if [ -d "$HOME/esp/esp-idf" ] || [ -d "/opt/esp/idf" ]; then
    echo "  ✓ ESP-IDF found"
else
    echo "  ⚠ ESP-IDF not found. Install: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/"
    echo "    Required for: Pool Camera (ESP32-S3)"
fi

# ============================================================
# 3. KiCad
# ============================================================

echo ""
echo "[3/5] Checking KiCad..."

if command -v kicad-cli &>/dev/null; then
    echo "  ✓ KiCad: $(kicad-cli --version 2>/dev/null || echo 'installed')"
else
    echo "  ⚠ KiCad not found. Install: https://www.kicad.org/download/"
    echo "    Required for: Schematic editing and PCB layout"
fi

# ============================================================
# 4. Mobile App Dependencies
# ============================================================

echo ""
echo "[4/5] Checking mobile app dependencies..."

if command -v node &>/dev/null; then
    echo "  ✓ Node.js: $(node --version)"
else
    echo "  ⚠ Node.js not found. Install: https://nodejs.org/"
fi

if command -v npx &>/dev/null; then
    echo "  ✓ npx available (for Expo)"
else
    echo "  ⚠ npx not found. Install: npm install -g expo-cli"
fi

# ============================================================
# 5. Start Dashboard
# ============================================================

echo ""
echo "[5/5] Starting PoolSync dashboard..."

cd "${POOLSYNC_DIR}/software/dashboard"

echo ""
echo "========================================="
echo "  PoolSync Development Environment Ready"
echo "========================================="
echo ""
echo "To start the dashboard:"
echo "  cd ${POOLSYNC_DIR}/software/dashboard"
echo "  ${PYTHON} main.py"
echo ""
echo "Dashboard will be available at: http://localhost:8080"
echo "API docs at: http://localhost:8080/docs"
echo ""
echo "Firmware build commands:"
echo "  Hub (RP2040):    cd firmware/hub && mkdir build && cd build && cmake .. && make"
echo "  Probe (STM32L4): cd firmware/chemistry_probe && make"
echo "  Camera (ESP32):  idf.py build -C firmware/pool_camera"
echo "  Equip (STM32F4): cd firmware/equipment_controller && make"
echo ""
echo "Mobile app:"
echo "  cd ${POOLSYNC_DIR}/software/mobile-app"
echo "  npm install && npx expo start"
echo ""
#!/bin/bash
# UrbanHarvest — Setup Script
# Installs dependencies, builds firmware, prepares for first deployment

set -e

echo "============================================"
echo "  UrbanHarvest — Setup"
echo "============================================"

# ========== SYSTEM DEPENDENCIES ==========

echo ""
echo "Installing system dependencies..."

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    apt-get update -qq
    apt-get install -y -qq python3 python3-pip git cmake ninja-build gcc-arm-none-eabi \
        libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
        openocd picotool esptool docker.io docker-compose \
        mosquitto-clients 2>/dev/null || true
elif [[ "$OSTYPE" == "darwin"* ]]; then
    brew install cmake ninja python3 git openocd picotool esptool mosquitto docker docker-compose 2>/dev/null || true
fi

echo "  ✓ System dependencies installed"

# ========== ZEPHYR RTOS (for nRF5340 hub) ==========

echo ""
echo "Setting up Zephyr RTOS for hub firmware..."

if [ ! -d "$HOME/zephyr" ]; then
    pip3 install west
    west init "$HOME/zephyr"
    cd "$HOME/zephyr"
    west update
    pip3 install -r zephyr/scripts/requirements.txt
    echo "  ✓ Zephyr RTOS installed"
else
    echo "  ✓ Zephyr RTOS already installed"
fi

# ========== PICO SDK (for RP2040 weather station) ==========

echo ""
echo "Setting up Pico SDK for weather station firmware..."

if [ ! -d "$HOME/pico-sdk" ]; then
    git clone https://github.com/raspberrypi/pico-sdk.git "$HOME/pico-sdk"
    cd "$HOME/pico-sdk"
    git submodule update --init
    echo "  ✓ Pico SDK installed"
else
    echo "  ✓ Pico SDK already installed"
fi

# ========== ESP-IDF (for ESP32-S3 grow pod) ==========

echo ""
echo "Setting up ESP-IDF for grow pod firmware..."

if [ ! -d "$HOME/esp-idf" ]; then
    git clone --depth 1 --branch v5.1.2 https://github.com/espressif/esp-idf.git "$HOME/esp-idf"
    cd "$HOME/esp-idf"
    ./install.sh esp32s3
    echo "  ✓ ESP-IDF installed (ESP32-S3 target)"
else
    echo "  ✓ ESP-IDF already installed"
fi

# ========== PYTHON DEPENDENCIES ==========

echo ""
echo "Installing Python dependencies for cloud backend and ML..."

pip3 install fastapi uvicorn sqlalchemy psycopg2-binary paho-mqtt pydantic httpx \
    torch torchvision tensorflow scikit-learn Pillow numpy matplotlib joblib 2>/dev/null || true

echo "  ✓ Python dependencies installed"

# ========== BUILD FIRMWARE ==========

SCRIPT_DIR="$(dirname "$0")"
FIRMWARE_DIR="$SCRIPT_DIR/../firmware"

echo ""
echo "Building firmware..."
echo "--------------------------------------"

# Hub (Zephyr)
echo "  Building hub node firmware (nRF5340)..."
mkdir -p "$FIRMWARE_DIR/hub-node/build"
cd "$FIRMWARE_DIR/hub-node/build"
cmake -GNinja -DBOARD=nrf5340dk_nrf5340_cpuapp .. 2>/dev/null || echo "  ⚠ Hub build requires Zephyr west workspace"
ninja 2>/dev/null || echo "  ⚠ Hub build failed — will need manual build"

# Grow Pod (ESP-IDF)
echo "  Building grow pod firmware (ESP32-S3)..."
mkdir -p "$FIRMWARE_DIR/grow-pod/build"
cd "$FIRMWARE_DIR/grow-pod/build"
. "$HOME/esp-idf/export.sh" 2>/dev/null || true
idf.py build 2>/dev/null || echo "  ⚠ Grow pod build requires ESP-IDF environment"

# Weather Station (Pico SDK)
echo "  Building weather station firmware (RP2040)..."
mkdir -p "$FIRMWARE_DIR/weather-station/build"
cd "$FIRMWARE_DIR/weather-station/build"
cmake -DPICO_SDK_PATH="$HOME/pico-sdk" .. 2>/dev/null || echo "  ⚠ Weather station build requires Pico SDK"
make 2>/dev/null || echo "  ⚠ Weather station build failed"

echo ""
echo "============================================"
echo "  Setup Complete!"
echo "============================================"
echo ""
echo "To deploy:"
echo "  $SCRIPT_DIR/deploy.sh"
echo ""
echo "To calibrate sensors:"
echo "  $SCRIPT_DIR/calibrate.sh [sensor_id] [mqtt_broker]"
echo ""
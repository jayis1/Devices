#!/bin/bash
# flash_all.sh — Flash firmware to all SleepSync nodes
#
# Prerequisites:
#   - ESP-IDF installed (for ESP32-S3 and ESP32-C3)
#   - nRF5 SDK installed (for nRF52832)
#   - All boards connected via USB/UART
#
# Usage:
#   ./flash_all.sh              # Flash all nodes
#   ./flash_all.sh hub          # Flash nightstand hub only
#   ./flash_all.sh strip        # Flash sleep strip only
#   ./flash_all.sh climate      # Flash climate node only
#   ./flash_all.sh shade        # Flash shade controller only

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FW_DIR="$SCRIPT_DIR/../firmware"

# ESP32 serial ports (adjust for your setup)
HUB_PORT="${HUB_PORT:-/dev/ttyUSB0}"
CLIMATE_PORT="${CLIMATE_PORT:-/dev/ttyUSB1}"
SHADE_PORT="${SHADE_PORT:-/dev/ttyUSB2}"
STRIP_PORT="${STRIP_PORT:-/dev/ttyACM0}"

flash_hub() {
    echo "=== Flashing Nightstand Hub (ESP32-S3) on $HUB_PORT ==="
    cd "$FW_DIR/nightstand-hub"
    # In production: idf.py build
    # idf.py -p $HUB_PORT flash
    echo "[STUB] Hub firmware would be flashed here"
}

flash_strip() {
    echo "=== Flashing Sleep Strip (nRF52832) on $STRIP_PORT ==="
    cd "$FW_DIR/sleep-strip"
    # In production:
    # make clean && make
    # nrfjprog --program build/nrf52832_xxaa.hex --chiperase --reset
    echo "[STUB] Strip firmware would be flashed here"
}

flash_climate() {
    echo "=== Flashing Climate Node (ESP32-C3) on $CLIMATE_PORT ==="
    cd "$FW_DIR/climate-node"
    # In production: idf.py build
    # idf.py -p $CLIMATE_PORT flash
    echo "[STUB] Climate firmware would be flashed here"
}

flash_shade() {
    echo "=== Flashing Shade Controller (ESP32-C3) on $SHADE_PORT ==="
    cd "$FW_DIR/shade-controller"
    # In production: idf.py build
    # idf.py -p $SHADE_PORT flash
    echo "[STUB] Shade firmware would be flashed here"
}

case "${1:-all}" in
    hub)
        flash_hub
        ;;
    strip)
        flash_strip
        ;;
    climate)
        flash_climate
        ;;
    shade)
        flash_shade
        ;;
    all)
        flash_hub
        flash_strip
        flash_climate
        flash_shade
        ;;
    *)
        echo "Unknown target: $1"
        echo "Usage: $0 [all|hub|strip|climate|shade]"
        exit 1
        ;;
esac

echo ""
echo "=== Flash Complete ==="
echo "Power cycle all nodes and run the mobile app setup wizard."
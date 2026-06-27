#!/bin/bash
# Flash all CompostSync nodes
# Usage: ./scripts/flash_all.sh [hub|bin|soil|weather|all]

set -e

TARGET=${1:-all}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

flash_hub() {
    echo "=== Flashing Hub (ESP32) ==="
    cd "$ROOT_DIR/firmware/hub"
    if command -v idf.py &> /dev/null; then
        idf.py build
        idf.py -p /dev/ttyUSB0 flash
    else
        echo "ESP-IDF not found. Install from https://docs.espressif.com"
        echo "Firmware is ready at: $ROOT_DIR/firmware/hub/"
    fi
}

flash_bin() {
    echo "=== Flashing Bin Node (ESP32) ==="
    cd "$ROOT_DIR/firmware/bin-node"
    if command -v idf.py &> /dev/null; then
        idf.py build
        idf.py -p /dev/ttyUSB1 flash
    else
        echo "ESP-IDF not found. Build manually."
    fi
}

flash_soil() {
    echo "=== Flashing Soil Probe (RP2040) ==="
    cd "$ROOT_DIR/firmware/soil-probe"
    if [ -d "build" ]; then
        echo "Copy build/soil_probe.uf2 to Pico (hold BOOTSEL, plug USB)"
    else
        echo "Build first: mkdir build && cd build && cmake .. && make"
        echo "Then copy soil_probe.uf2 to Pico RPI-RP2 drive"
    fi
}

flash_weather() {
    echo "=== Flashing Weather Station (nRF52840) ==="
    cd "$ROOT_DIR/firmware/weather-station"
    if command -v make &> /dev/null; then
        make
        echo "Flash with: nrfjprog --program _build/weather_station.hex --sectorerase"
    fi
}

case "$TARGET" in
    hub)     flash_hub ;;
    bin)     flash_bin ;;
    soil)    flash_soil ;;
    weather) flash_weather ;;
    all)
        flash_hub
        flash_bin
        flash_soil
        flash_weather
        ;;
    *)
        echo "Usage: $0 [hub|bin|soil|weather|all]"
        exit 1
        ;;
esac

echo "=== Done ==="
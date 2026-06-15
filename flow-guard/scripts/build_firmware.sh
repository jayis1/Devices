#!/bin/bash
# FlowGuard - Firmware Build and Flash Script
# Builds all firmware and provides flashing instructions
#
# Usage:
#   ./scripts/build_firmware.sh [target]
#   Targets: all, hub, valve, pipe, appliance, flash-hub, flash-valve, flash-pipe, flash-appliance

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FW_DIR="$(dirname "$SCRIPT_DIR")/firmware"
BUILD_DIR="$FW_DIR/build"

echo -e "${GREEN}FlowGuard Firmware Build Script${NC}"
echo "========================================"

# Check for Zephyr / west
if ! command -v west &> /dev/null; then
    echo -e "${RED}Error: west (Zephyr meta-tool) not found${NC}"
    echo "Install Zephyr SDK: https://docs.zephyrproject.org/latest/develop/getting_started/index.html"
    exit 1
fi

# Check for ESP-IDF (for ESP32-C6)
if [ ! -d "$IDF_PATH" ]; then
    echo -e "${YELLOW}Warning: ESP-IDF not found. ESP32-C6 bridge cannot be built.${NC}"
    echo "Set IDF_PATH environment variable if building the WiFi bridge."
fi

build_hub() {
    echo -e "${YELLOW}Building Hub Node (nRF52840)...${NC}"
    mkdir -p "$BUILD_DIR/hub"
    cd "$BUILD_DIR/hub"
    west build -b nrf52840dk_nrf52840 "$FW_DIR/hub-node" -p
    echo -e "${GREEN}Hub build complete: $BUILD_DIR/hub/zephyr.hex${NC}"
    echo "Flash via SWD: nrfjprog --program $BUILD_DIR/hub/zephyr.hex --sectorerase --reset"
}

build_hub_esp() {
    echo -e "${YELLOW}Building Hub WiFi Bridge (ESP32-C6)...${NC}"
    cd "$FW_DIR/hub-node/esp32-bridge"
    idfy set-target esp32c6
    idf.py build
    echo -e "${GREEN}ESP32-C6 build complete${NC}"
    echo "Flash: idf.py -p /dev/ttyUSB0 flash"
}

build_valve() {
    echo -e "${YELLOW}Building Valve Controller (nRF52832)...${NC}"
    mkdir -p "$BUILD_DIR/valve"
    cd "$BUILD_DIR/valve"
    west build -b nrf52dk_nrf52832 "$FW_DIR/valve-controller" -p
    echo -e "${GREEN}Valve controller build complete: $BUILD_DIR/valve/zephyr.hex${NC}"
    echo "Flash via SWD: nrfjprog --program $BUILD_DIR/valve/zephyr.hex --sectorerase --reset"
}

build_pipe() {
    echo -e "${YELLOW}Building Pipe Sensor (nRF52832)...${NC}"
    mkdir -p "$BUILD_DIR/pipe"
    cd "$BUILD_DIR/pipe"
    west build -b nrf52dk_nrf52832 "$FW_DIR/pipe-sensor" -p
    echo -e "${GREEN}Pipe sensor build complete: $BUILD_DIR/pipe/zephyr.hex${NC}"
    echo "Flash via SWD: nrfjprog --program $BUILD_DIR/pipe/zephyr.hex --sectorerase --reset"
}

build_appliance() {
    echo -e "${YELLOW}Building Appliance Monitor (nRF52832)...${NC}"
    mkdir -p "$BUILD_DIR/appliance"
    cd "$BUILD_DIR/appliance"
    west build -b nrf52dk_nrf52832 "$FW_DIR/appliance-monitor" -p
    echo -e "${GREEN}Appliance monitor build complete: $BUILD_DIR/appliance/zephyr.hex${NC}"
    echo "Flash via SWD: nrfjprog --program $BUILD_DIR/appliance/zephyr.hex --sectorerase --reset"
}

case "${1:-all}" in
    all)
        build_hub
        build_valve
        build_pipe
        build_appliance
        ;;
    hub)
        build_hub
        ;;
    valve)
        build_valve
        ;;
    pipe)
        build_pipe
        ;;
    appliance)
        build_appliance
        ;;
    flash-hub)
        build_hub
        echo -e "${YELLOW}Flashing Hub Node...${NC}"
        nrfjprog --program "$BUILD_DIR/hub/zephyr.hex" --sectorerase --reset
        echo -e "${GREEN}Hub Node flashed successfully!${NC}"
        ;;
    flash-valve)
        build_valve
        echo -e "${YELLOW}Flashing Valve Controller...${NC}"
        nrfjprog --program "$BUILD_DIR/valve/zephyr.hex" --sectorerase --reset
        echo -e "${GREEN}Valve Controller flashed successfully!${NC}"
        ;;
    flash-pipe)
        build_pipe
        echo -e "${YELLOW}Flashing Pipe Sensor...${NC}"
        nrfjprog --program "$BUILD_DIR/pipe/zephyr.hex" --sectorerase --reset
        echo -e "${GREEN}Pipe Sensor flashed successfully!${NC}"
        ;;
    flash-appliance)
        build_appliance
        echo -e "${YELLOW}Flashing Appliance Monitor...${NC}"
        nrfjprog --program "$BUILD_DIR/appliance/zephyr.hex" --sectorerase --reset
        echo -e "${GREEN}Appliance Monitor flashed successfully!${NC}"
        ;;
    *)
        echo "Unknown target: $1"
        echo "Valid targets: all, hub, valve, pipe, appliance, flash-hub, flash-valve, flash-pipe, flash-appliance"
        exit 1
        ;;
esac

echo ""
echo -e "${GREEN}Build complete!${NC}"
#!/bin/bash
# PowerPulse Firmware Flash Script
# Flashes firmware to all nodes

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "=== PowerPulse Firmware Flash Utility ==="
echo ""

# ─── Hub Node (ESP32-S3) ─────────────────────────────────────────────

flash_hub() {
    echo -e "${YELLOW}[Hub Node]${NC} Flashing ESP32-S3 firmware..."
    
    FIRMWARE_DIR="$(dirname "${BASH_SOURCE[0]}")/../firmware/hub-node"
    
    if [ ! -d "$FIRMWARE_DIR" ]; then
        echo -e "${RED}Error: Hub firmware directory not found${NC}"
        return 1
    fi
    
    # Check for ESP-IDF
    if command -v idf.py &> /dev/null; then
        cd "$FIRMWARE_DIR"
        idf.py build
        idf.py flash
        echo -e "${GREEN}[Hub Node]${NC} ✓ Flashed successfully"
    else
        echo -e "${RED}Error: ESP-IDF not found. Install from https://docs.espressif.com/projects/esp-idf/${NC}"
        return 1
    fi
}

# ─── Circuit Monitor (STM32G474) ────────────────────────────────────

flash_circuit_monitor() {
    echo -e "${YELLOW}[Circuit Monitor]${NC} Flashing STM32G474 firmware..."
    
    FIRMWARE_DIR="$(dirname "${BASH_SOURCE[0]}")/../firmware/circuit-monitor"
    
    if [ ! -d "$FIRMWARE_DIR" ]; then
        echo -e "${RED}Error: Circuit monitor firmware directory not found${NC}"
        return 1
    fi
    
    # Check for OpenOCD and ST-Link
    if command -v openocd &> /dev/null; then
        cd "$FIRMWARE_DIR"
        make clean
        make
        openocd -f interface/stlink.cfg -f target/stm32g4x.cfg -c "program build/circuit_monitor.elf verify reset exit"
        echo -e "${GREEN}[Circuit Monitor]${NC} ✓ Flashed successfully"
    else
        echo -e "${RED}Error: OpenOCD not found. Install for ST-Link programming${NC}"
        return 1
    fi
}

# ─── Appliance Tag (nRF52840) ────────────────────────────────────────

flash_appliance_tag() {
    echo -e "${YELLOW}[Appliance Tag]${NC} Flashing nRF52840 firmware..."
    
    FIRMWARE_DIR="$(dirname "${BASH_SOURCE[0]}")/../firmware/appliance-tag"
    
    if [ ! -d "$FIRMWARE_DIR" ]; then
        echo -e "${RED}Error: Appliance tag firmware directory not found${NC}"
        return 1
    fi
    
    # Check for nRF Connect SDK / west
    if command -v west &> /dev/null; then
        cd "$FIRMWARE_DIR"
        west build -b nrf52840dongle_nrf52840
        west flash
        echo -e "${GREEN}[Appliance Tag]${NC} ✓ Flashed successfully"
    else
        echo -e "${RED}Error: nRF Connect SDK (west) not found${NC}"
        echo "  Install from https://developer.nordicsemi.com/"
        return 1
    fi
}

# ─── Solar Node (RP2040) ────────────────────────────────────────────

flash_solar_node() {
    echo -e "${YELLOW}[Solar Node]${NC} Flashing RP2040 firmware..."
    
    FIRMWARE_DIR="$(dirname "${BASH_SOURCE[0]}")/../firmware/solar-node"
    
    if [ ! -d "$FIRMWARE_DIR" ]; then
        echo -e "${RED}Error: Solar node firmware directory not found${NC}"
        return 1
    fi
    
    # Check for Pico SDK
    if [ -f "$FIRMWARE_DIR/build/power_pulse_solar.uf2" ]; then
        echo -e "${YELLOW}Copying UF2 to RP2040 mass storage...${NC}"
        echo "  Connect RP2040 via USB while holding BOOTSEL"
        echo "  The UF2 file will be copied to the mounted drive"
        
        # Try to find RP2040 mass storage
        for drive in /media/*/RPI-RP2 /Volumes/RPI-RP2 /run/media/*/RPI-RP2; do
            if [ -d "$drive" ]; then
                cp "$FIRMWARE_DIR/build/power_pulse_solar.uf2" "$drive/"
                sync
                echo -e "${GREEN}[Solar Node]${NC} ✓ Flashed successfully"
                return 0
            fi
        done
        
        echo -e "${YELLOW}RP2040 not found in BOOTSEL mode.${NC}"
        echo "  Connect the RP2040 via USB while holding the BOOTSEL button"
        echo "  Then run this script again."
        echo ""
        echo "  Alternatively, copy this file manually:"
        echo "  $FIRMWARE_DIR/build/power_pulse_solar.uf2"
        return 1
    else
        # Build first
        cd "$FIRMWARE_DIR"
        mkdir -p build && cd build
        cmake .. && make -j$(nproc)
        
        if [ -f "power_pulse_solar.uf2" ]; then
            echo -e "${GREEN}Build complete.${NC} Run this script again to flash."
        else
            echo -e "${RED}Build failed.${NC}"
            return 1
        fi
    fi
}

# ─── Main Menu ────────────────────────────────────────────────────────

echo "Select which node(s) to flash:"
echo "  1) Hub Node (ESP32-S3)"
echo "  2) Circuit Monitor (STM32G474)"
echo "  3) Appliance Tag (nRF52840)"
echo "  4) Solar Node (RP2040)"
echo "  5) All nodes"
echo "  6) Exit"
echo ""
read -p "Enter choice [1-6]: " choice

case $choice in
    1) flash_hub ;;
    2) flash_circuit_monitor ;;
    3) flash_appliance_tag ;;
    4) flash_solar_node ;;
    5) 
        flash_hub
        echo ""
        flash_circuit_monitor
        echo ""
        flash_appliance_tag
        echo ""
        flash_solar_node
        ;;
    6) echo "Exiting"; exit 0 ;;
    *) echo "Invalid choice"; exit 1 ;;
esac

echo ""
echo "=== Flash Complete ==="
#!/bin/bash
# BrewSync — Flash firmware to nodes
# Usage: ./flash.sh <node_type> [device_path]
# node_type: fermenter | cellar | hub | scanner

set -euo pipefail

NODE_TYPE="${1:?Usage: flash.sh <fermenter|cellar|hub|scanner> [device_path]}"
DEVICE="${2:-}"

FIRMWARE_DIR="$(dirname "$0")/../firmware/$NODE_TYPE"

echo "======================================"
echo "BrewSync Firmware Flash"
echo "Node type: $NODE_TYPE"
echo "======================================"

case "$NODE_TYPE" in
    fermenter|cellar)
        # STM32L476 — flash via OpenOCD or ST-Link
        if [ -z "$DEVICE" ]; then
            echo "No device path specified, using ST-Link..."
            DEVICE="stlink"
        fi

        echo "Building firmware..."
        cd "$FIRMWARE_DIR"
        if [ -f "Makefile" ]; then
            make clean && make
        else
            echo "No Makefile found. Building with CMake..."
            mkdir -p build && cd build
            cmake .. && make -j$(nproc)
        fi

        echo "Flashing via OpenOCD..."
        openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
            -c "program build/firmware.elf verify reset exit"
        ;;

    hub)
        # RP2040 — flash via UF2 bootloader
        if [ -z "$DEVICE" ]; then
            echo "Looking for RP2040 in BOOTSEL mode..."
            DEVICE=$(ls /media/*/RPI-RP2 2>/dev/null | head -1 || true)
            if [ -z "$DEVICE" ]; then
                echo "❌ No RP2040 in BOOTSEL mode found!"
                echo "Hold BOOTSEL button on RP2040 and connect USB."
                exit 1
            fi
        fi

        echo "Building RP2040 firmware..."
        cd "$FIRMWARE_DIR"
        mkdir -p build && cd build
        cmake .. && make -j$(nproc)

        echo "Copying UF2 to $DEVICE..."
        cp build/firmware.uf2 "$DEVICE/"
        echo "✅ Firmware flashed! RP2040 will reboot."
        ;;

    scanner)
        # ESP32-S3 — flash via esptool
        if [ -z "$DEVICE" ]; then
            DEVICE="/dev/ttyUSB0"
        fi

        echo "Building ESP32-S3 firmware..."
        cd "$FIRMWARE_DIR"
        idf.py build

        echo "Flashing via esptool..."
        esptool.py -p "$DEVICE" -b 460800 \
            --before default_reset --after hard_reset \
            write_flash 0x0 build/firmware.bin
        echo "✅ Firmware flashed!"
        ;;

    *)
        echo "Unknown node type: $NODE_TYPE"
        echo "Valid types: fermenter, cellar, hub, scanner"
        exit 1
        ;;
esac

echo ""
echo "Flashing complete! Monitor serial output:"
echo "  minicom -D $DEVICE -b 115200"
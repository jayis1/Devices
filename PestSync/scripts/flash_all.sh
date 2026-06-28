#!/bin/bash
# PestSync — Flash All Firmware
# scripts/flash_all.sh
#
# Usage: ./flash_all.sh [hub|sentinel|trap|deterrent|all]
#
# Requires ESP-IDF v5.1+ installed and configured.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$SCRIPT_DIR/../firmware"
PORT="${PORT:-/dev/ttyUSB0}"
BAUD=921600

if [ -z "$IDF_PATH" ]; then
    echo "⚠️  IDF_PATH not set. Please source ESP-IDF export script:"
    echo "   . \$HOME/esp/esp-idf/export.sh"
    exit 1
fi

flash_hub() {
    echo "🔧 Flashing PestSync Hub (ESP32-WROOM-32E)..."
    cd "$FIRMWARE_DIR/hub"
    idf.py set-target esp32
    idf.py build
    idf.py -p "$PORT" -b "$BAUD" flash
    echo "✅ Hub flashed successfully"
}

flash_sentinel() {
    echo "🔧 Flashing Pest Sentinel (ESP32-S3-N8R2)..."
    cd "$FIRMWARE_DIR/pest-sentinel"
    idf.py set-target esp32s3
    idf.py build
    idf.py -p "$PORT" -b "$BAUD" flash
    echo "✅ Pest Sentinel flashed successfully"
}

flash_trap() {
    echo "🔧 Flashing Smart Trap (ESP32-C3)..."
    cd "$FIRMWARE_DIR/smart-trap"
    idf.py set-target esp32c3
    idf.py build
    idf.py -p "$PORT" -b "$BAUD" flash
    echo "✅ Smart Trap flashed successfully"
}

flash_deterrent() {
    echo "🔧 Flashing Deterrent Node (ESP32-C3)..."
    cd "$FIRMWARE_DIR/deterrent-node"
    idf.py set-target esp32c3
    idf.py build
    idf.py -p "$PORT" -b "$BAUD" flash
    echo "✅ Deterrent Node flashed successfully"
}

TARGET="${1:-all}"

case "$TARGET" in
    hub)
        flash_hub
        ;;
    sentinel)
        flash_sentinel
        ;;
    trap)
        flash_trap
        ;;
    deterrent)
        flash_deterrent
        ;;
    all)
        echo "🚀 Flashing all PestSync nodes..."
        echo ""
        echo "⚠️  Connect Hub to $PORT and press Enter..."
        read -r
        flash_hub
        echo ""
        echo "⚠️  Connect Pest Sentinel to $PORT and press Enter..."
        read -r
        flash_sentinel
        echo ""
        echo "⚠️  Connect Smart Trap to $PORT and press Enter..."
        read -r
        flash_trap
        echo ""
        echo "⚠️  Connect Deterrent Node to $PORT and press Enter..."
        read -r
        flash_deterrent
        echo ""
        echo "🎉 All nodes flashed! Connect via mobile app to commission."
        ;;
    *)
        echo "Usage: $0 [hub|sentinel|trap|deterrent|all]"
        exit 1
        ;;
esac
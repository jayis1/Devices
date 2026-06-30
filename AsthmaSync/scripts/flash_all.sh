#!/bin/bash
# AsthmaSync — Flash All Nodes
# Flashes firmware to all 4 hardware nodes.
#
# Prerequisites:
#   - ESP-IDF v5.1+ (for ESP32-S3 nodes)
#   - nRF Connect SDK (for nRF52840 nodes)
#   - USB-C cable connected to each node
#
# License: MIT

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FW_DIR="$PROJECT_DIR/firmware"

echo "╔══════════════════════════════════════════╗"
echo "║   AsthmaSync — Firmware Flash Tool        ║"
echo "╚══════════════════════════════════════════╝"

# ── Detect ESP-IDF ───────────────────────────────────────
if [ -z "${IDF_PATH:-}" ]; then
    echo "⚠️  ESP-IDF not found. Please run: . \$IDF_PATH/export.sh"
    echo "   Or set IDF_PATH environment variable."
    exit 1
fi

# ── Detect nRF Connect SDK ────────────────────────────────
if [ -z "${NCS_TOOLCHAIN_PATH:-}" ]; then
    echo "⚠️  nRF Connect SDK not found."
    echo "   Install: https://www.nordicsemi.com/Products/Development-software/nrf-connect-sdk"
    exit 1
fi

echo ""
echo "Available nodes:"
echo "  1. Hub (ESP32-S3)"
echo "  2. Air Sentinel (ESP32-S3)"
echo "  3. Inhaler Tag (nRF52840)"
echo "  4. Wheeze Band (nRF52840)"
echo "  5. All"
echo ""
read -p "Select node to flash [1-5]: " choice

flash_hub() {
    echo "📡 Flashing Hub (ESP32-S3)..."
    cd "$FW_DIR/hub"
    idf.py set-target esp32s3
    idf.py menuconfig  # let user configure WiFi/MQTT
    idf.py build
    idf.py flash monitor
}

flash_air_sentinel() {
    echo "🌬️  Flashing Air Sentinel (ESP32-S3)..."
    cd "$FW_DIR/air-sentinel"
    idf.py set-target esp32s3
    idf.py build
    idf.py flash monitor
}

flash_inhaler_tag() {
    echo "💊 Flashing Inhaler Tag (nRF52840)..."
    cd "$FW_DIR/inhaler-tag"
    west build -b nrf52840dk_nrf52840
    west flash
}

flash_wheeze_band() {
    echo "⌚ Flashing Wheeze Band (nRF52840)..."
    cd "$FW_DIR/wheeze-band"
    west build -b nrf52840dk_nrf52840
    west flash
}

case $choice in
    1) flash_hub ;;
    2) flash_air_sentinel ;;
    3) flash_inhaler_tag ;;
    4) flash_wheeze_band ;;
    5)
        flash_hub
        flash_air_sentinel
        flash_inhaler_tag
        flash_wheeze_band
        ;;
    *)
        echo "Invalid choice: $choice"
        exit 1
        ;;
esac

echo ""
echo "✅ Firmware flash complete!"
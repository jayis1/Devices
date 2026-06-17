#!/bin/bash
# SoundNest Firmware Flash Script
# Flashes firmware to all SoundNest nodes via SWD/UART

set -e

FIRMWARE_DIR="$(dirname "$0")/../firmware"
OPENOCD="${OPENOCD:-openocd}"
ESPTOOL="${ESPTOOL:-esptool.py}"
NRFUTIL="${NRFUTIL:-nrfutil}"

echo "╔══════════════════════════════════════╗"
echo "║   SoundNest Firmware Flash Tool     ║"
echo "╚══════════════════════════════════════╝"
echo ""

# ── Hub Node (ESP32-S3) ────────────────────────────────────────────────

flash_hub() {
    echo "[Hub Node] Flashing ESP32-S3..."
    if [ -f "$FIRMWARE_DIR/hub-node/build/soundnest-hub.bin" ]; then
        $ESPTOOL.py \
            --chip esp32s3 \
            --port /dev/ttyUSB0 \
            --baud 460800 \
            write_flash \
            -z 0x0 "$FIRMWARE_DIR/hub-node/build/soundnest-hub.bin"
        echo "  ✓ Hub node flashed"
    else
        echo "  ⚠ Hub firmware not found. Build first with:"
        echo "    cd firmware/hub-node && idf.py build"
    fi
}

# ── Room Sensor (nRF52840) ─────────────────────────────────────────────

flash_room_sensor() {
    echo "[Room Sensor] Flashing nRF52840..."
    if [ -f "$FIRMWARE_DIR/room-sensor/build/zephyr.hex" ]; then
        $NRFUTIL dfu usb-serial \
            --port /dev/ttyACM0 \
            --baud 115200 \
            --package "$FIRMWARE_DIR/room-sensor/build/dfu_package.zip"
        echo "  ✓ Room sensor flashed"
    else
        echo "  ⚠ Room sensor firmware not found. Build first with:"
        echo "    cd firmware/room-sensor && west build -b nrf52840dk_nrf52840"
    fi
}

# ── Masking Speaker (ESP32-S3) ─────────────────────────────────────────

flash_masking_speaker() {
    echo "[Masking Speaker] Flashing ESP32-S3..."
    if [ -f "$FIRMWARE_DIR/masking-speaker/build/soundnest-speaker.bin" ]; then
        $ESPTOOL.py \
            --chip esp32s3 \
            --port /dev/ttyUSB1 \
            --baud 460800 \
            write_flash \
            -z 0x0 "$FIRMWARE_DIR/masking-speaker/build/soundnest-speaker.bin"
        echo "  ✓ Masking speaker flashed"
    else
        echo "  ⚠ Masking speaker firmware not found. Build first."
    fi
}

# ── Wearable Tag (nRF52832) ─────────────────────────────────────────────

flash_wearable_tag() {
    echo "[Wearable Tag] Flashing nRF52832..."
    if [ -f "$FIRMWARE_DIR/wearable-tag/build/zephyr.hex" ]; then
        $NRFUTIL dfu usb-serial \
            --port /dev/ttyACM1 \
            --baud 115200 \
            --package "$FIRMWARE_DIR/wearable-tag/build/dfu_package.zip"
        echo "  ✓ Wearable tag flashed"
    else
        echo "  ⚠ Wearable tag firmware not found. Build first with:"
        echo "    cd firmware/wearable-tag && west build -b nrf52dk_nrf52832"
    fi
}

# ── Flash All ───────────────────────────────────────────────────────────

flash_all() {
    flash_hub
    flash_room_sensor
    flash_masking_speaker
    flash_wearable_tag
    echo ""
    echo "✓ All nodes flashed successfully!"
}

# ── Main ────────────────────────────────────────────────────────────────

case "${1:-all}" in
    hub)       flash_hub ;;
    sensor)    flash_room_sensor ;;
    speaker)   flash_masking_speaker ;;
    tag)       flash_wearable_tag ;;
    all)       flash_all ;;
    *)
        echo "Usage: $0 {hub|sensor|speaker|tag|all}"
        echo ""
        echo "  hub      - Flash hub node (ESP32-S3)"
        echo "  sensor   - Flash room sensor (nRF52840)"
        echo "  speaker  - Flash masking speaker (ESP32-S3)"
        echo "  tag      - Flash wearable tag (nRF52832)"
        echo "  all      - Flash all nodes"
        exit 1
        ;;
esac
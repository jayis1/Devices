#!/usr/bin/env bash
# Flash OralSync firmware to all node types.
# Usage: ./flash.sh <node-type> [port]
#   node-type: hub | toothbrush | scanner | saliva
#   port: /dev/ttyACM0 (default) or other
set -euo pipefail

NODE="${1:?usage: $0 <hub|toothbrush|scanner|saliva> [port]}"
PORT="${2:-/dev/ttyACM0}"
FW_DIR="$(cd "$(dirname "$0")/.." && pwd)/firmware"

case "$NODE" in
  hub)
    # RP2040 via picotool (drag-drop UF2 also works)
    echo "[hub] building RP2040 firmware (pico-sdk)..."
    # Requires: pico-sdk + arm-none-eabi-gcc
    if command -v picotool &>/dev/null; then
      picotool load "$FW_DIR/hub/build/oralsync_hub.uf2" -f
      picotool reboot
    else
      echo "[warn] picotool not found — copy oralsync_hub.uf2 to RPI-RP2 mass-storage"
    fi
    # ESP32-C3 via esptool
    if command -v esptool.py &>/dev/null; then
      esptool.py --port "$PORT" --baud 460800 write_flash 0x0 "$FW_DIR/hub/build/esp32c3.bin"
    fi
    ;;
  toothbrush)
    # nRF52840 via openocd (CMSIS-DAP/J-Link) or nrfjprog
    if command -v nrfjprog &>/dev/null; then
      nrfjprog --program "$FW_DIR/toothbrush/build/oralsync_tb.hex" --sectoranduicrerase -f nRF52 --reset
    elif command -v openocd &>/dev/null; then
      openocd -f interface/cmsis-dap.cfg -f target/nrf52.cfg \
        -c "program $FW_DIR/toothbrush/build/oralsync_tb.elf verify reset exit"
    else
      echo "[err] need nrfjprog or openocd"; exit 1
    fi
    ;;
  scanner)
    # ESP32-S3 via esptool
    if command -v esptool.py &>/dev/null; then
      esptool.py --port "$PORT" --baud 460800 --chip esp32s3 \
        write_flash 0x0 "$FW_DIR/scanner/build/oralsync_scanner.bin"
    else
      echo "[err] need esptool.py"; exit 1
    fi
    ;;
  saliva)
    # STM32L432 via openocd (ST-Link)
    if command -v openocd &>/dev/null; then
      openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
        -c "program $FW_DIR/saliva_patch/build/oralsync_saliva.elf verify reset exit"
    else
      echo "[err] need openocd"; exit 1
    fi
    ;;
  *)
    echo "[err] unknown node: $NODE"; exit 1
    ;;
esac
echo "[ok] $NODE flashed via $PORT"
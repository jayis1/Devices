#!/bin/bash
# ErgoFlow — Flash Script
# Flashes compiled firmware to hardware nodes
#
# Usage: ./flash.sh [hub|chair-pad|desk-controller|wearable-tag] [serial_port]
# Copyright (c) 2026 jayis1. MIT License.

set -e

ERGOFLOW_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${ERGOFLOW_DIR}/build"

echo "═══════════════════════════════════════════"
echo "  ErgoFlow Firmware Flash Utility"
echo "═══════════════════════════════════════════"
echo ""

flash_nrf() {
    local target="$1"
    local hex_file="${BUILD_DIR}/${target}/zephyr/zephyr.hex"
    local serial_port="${2:-}"

    if [[ ! -f "${hex_file}" ]]; then
        echo "❌ Firmware not found: ${hex_file}"
        echo "   Run ./build.sh first."
        exit 1
    fi

    echo "Flashing ${target}..."
    echo "  Hex: ${hex_file}"

    if [[ -n "${serial_port}" ]]; then
        echo "  Port: ${serial_port}"
        nrfjprog --program "${hex_file}" --chiperase --serialno "$(nrfjprog --ids | head -1)" -f nrf52
    else
        echo "  Using J-Link"
        nrfjprog --program "${hex_file}" --chiperase -f nrf52
    fi

    echo "Resetting device..."
    nrfjprog --reset -f nrf52

    echo "✅ ${target} flashed successfully!"
}

flash_stm32() {
    local target="$1"
    local hex_file="${BUILD_DIR}/${target}/zephyr/zephyr.hex"
    local serial_port="${2:-/dev/ttyUSB0}"

    if [[ ! -f "${hex_file}" ]]; then
        echo "❌ Firmware not found: ${hex_file}"
        echo "   Run ./build.sh first."
        exit 1
    fi

    echo "Flashing ${target}..."
    echo "  Hex: ${hex_file}"
    echo "  Port: ${serial_port}"

    # Convert hex to bin for STM32 flash
    objcopy -I ihex -O binary "${hex_file}" "${BUILD_DIR}/${target}/zephyr.bin"

    # Flash using STM32CubeProgrammer CLI
    STM32_Programmer_CLI -c port=SWD -w "${BUILD_DIR}/${target}/zephyr.bin" 0x08000000 -v -rst

    echo "✅ ${target} flashed successfully!"
}

# ── Main ────────────────────────────────────────────────────────

TARGET="${1:-}"
PORT="${2:-}"

if [[ -z "${TARGET}" ]]; then
    echo "Usage: $0 <target> [serial_port]"
    echo ""
    echo "Available targets:"
    echo "  hub              — Hub node (nRF5340, nrfjprog)"
    echo "  chair-pad        — Chair pad (nRF52832, nrfjprog)"
    echo "  desk-controller  — Desk controller (STM32G070CB, STM32_Programmer_CLI)"
    echo "  wearable-tag     — Wearable tag (nRF52833, nrfjprog)"
    exit 1
fi

case "${TARGET}" in
    hub)
        flash_nrf "hub-node" "${PORT}"
        ;;
    chair-pad)
        flash_nrf "chair-pad" "${PORT}"
        ;;
    desk-controller)
        flash_stm32 "desk-controller" "${PORT}"
        ;;
    wearable-tag)
        flash_nrf "wearable-tag" "${PORT}"
        ;;
    *)
        echo "Unknown target: ${TARGET}"
        exit 1
        ;;
esac
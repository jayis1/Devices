#!/bin/bash
# ErgoFlow — Build Script
# Compiles Zephyr RTOS firmware for all nodes
#
# Usage: ./build.sh [hub|chair-pad|desk-controller|wearable-tag|all]
# Copyright (c) 2026 jayis1. MIT License.

set -e

ERGOFLOW_DIR="$(cd "$(dirname "$0")/.." && pwd)"
FIRMWARE_DIR="${ERGOFLOW_DIR}/firmware"
BUILD_DIR="${ERGOFLOW_DIR}/build"
ZEPHYR_BASE="${ZEPHYR_BASE:-/opt/zephyrproject}"
WEST="${WEST:-west}"

echo "═══════════════════════════════════════════"
echo "  ErgoFlow Firmware Build Utility"
echo "═══════════════════════════════════════════"
echo ""
echo "Firmware dir:  ${FIRMWARE_DIR}"
echo "Build dir:     ${BUILD_DIR}"
echo "Zephyr base:   ${ZEPHYR_BASE}"
echo ""

build_firmware() {
    local target="$1"
    local board="$2"
    local node_dir="${FIRMWARE_DIR}/${target}"

    echo "━━━ Building ${target} (${board}) ━━━"

    mkdir -p "${BUILD_DIR}/${target}"

    # Create CMakeLists.txt for the node
    cat > "${node_dir}/CMakeLists.txt" << CMAKEEOF
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS \$ENV{ZEPHYR_BASE})
project(ergoflow_${target} VERSION 1.0.0)

target_sources(app PRIVATE
    main.c
    ${FIRMWARE_DIR}/common/ble_mesh/mesh_handler.c
    ${FIRMWARE_DIR}/common/ble_mesh/protocol.c
    ${FIRMWARE_DIR}/common/sensors/i2c_bus.c
    ${FIRMWARE_DIR}/common/util/crc16.c
    ${FIRMWARE_DIR}/common/util/ringbuf.c
)

target_include_directories(app PRIVATE
    ${FIRMWARE_DIR}/common/ble_mesh
    ${FIRMWARE_DIR}/common/sensors
    ${FIRMWARE_DIR}/common/util
)
CMAKEEOF

    # Build with west
    cd "${node_dir}"
    ${WEST} build -b "${board}" -d "${BUILD_DIR}/${target}" --pristine 2>&1 || {
        echo "⚠️  West build failed. Building with Zephyr cmake directly..."
        cd "${BUILD_DIR}/${target}"
        cmake -GNinja -DBOARD="${board}" -DZEPHYR_BASE="${ZEPHYR_BASE}" "${node_dir}"
        ninja
    }

    echo "✅ ${target} built successfully!"
    echo "   Binary: ${BUILD_DIR}/${target}/zephyr/zephyr.hex"
    echo ""
}

# ── Main ────────────────────────────────────────────────────────

case "${1:-all}" in
    hub)
        build_firmware "hub-node" "nrf5340dk_nrf5340_cpuapp"
        ;;
    chair-pad)
        build_firmware "chair-pad" "nrf52dk_nrf52832"
        ;;
    desk-controller)
        build_firmware "desk-controller" "nucleo_g070rb"
        ;;
    wearable-tag)
        build_firmware "wearable-tag" "nrf52840dk_nrf52833"
        ;;
    all)
        build_firmware "hub-node" "nrf5340dk_nrf5340_cpuapp"
        build_firmware "chair-pad" "nrf52dk_nrf52832"
        build_firmware "desk-controller" "nucleo_g070rb"
        build_firmware "wearable-tag" "nrf52840dk_nrf52833"
        echo "═══════════════════════════════════════════"
        echo "  ✅ All firmware built!"
        echo "═══════════════════════════════════════════"
        ;;
    *)
        echo "Usage: $0 [hub|chair-pad|desk-controller|wearable-tag|all]"
        echo ""
        echo "Available targets:"
        echo "  hub              — Hub node (nRF5340)"
        echo "  chair-pad        — Chair pad (nRF52832)"
        echo "  desk-controller  — Desk controller (STM32G070CB)"
        echo "  wearable-tag     — Wearable tag (nRF52833)"
        echo "  all              — Build all nodes"
        exit 1
        ;;
esac
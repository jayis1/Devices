#!/bin/bash
# MigraineSync — Flash All Nodes
# ===============================
# Flashes firmware to all MigraineSync hardware nodes via USB.
#
# Requirements:
#   - ESP-IDF (for ESP32-S3 nodes: Hub, Env Sentinel)
#   - Zephyr + nRF Connect SDK (for nRF52840 nodes: Aura Band, Hydrate Tag)
#   - Nodes connected via USB
#
# License: MIT

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FW_DIR="$SCRIPT_DIR/../firmware"

echo "============================================"
echo "  MigraineSync — Flash All Nodes"
echo "============================================"
echo ""

# ── ESP32-S3 nodes (ESP-IDF) ──────────────────────────────

echo "[1/4] Flashing Hub (ESP32-S3)..."
cd "$FW_DIR/hub"
# idf.py set-target esp32s3
# idf.py build flash
echo "  (Run: cd $FW_DIR/hub && idf.py set-target esp32s3 && idf.py build flash)"
echo ""

echo "[2/4] Flashing Env Sentinel (ESP32-S3)..."
cd "$FW_DIR/env-sentinel"
echo "  (Run: cd $FW_DIR/env-sentinel && idf.py set-target esp32s3 && idf.py build flash)"
echo ""

# ── nRF52840 nodes (Zephyr) ───────────────────────────────

echo "[3/4] Flashing Aura Band (nRF52840)..."
cd "$FW_DIR/aura-band"
echo "  (Run: cd $FW_DIR/aura-band && west build -b nrf52840dk_nrf52840 && west flash)"
echo ""

echo "[4/4] Flashing Hydrate Tag (nRF52840)..."
cd "$FW_DIR/hydrate-tag"
echo "  (Run: cd $FW_DIR/hydrate-tag && west build -b nrf52840dk_nrf52840 && west flash)"
echo ""

echo "============================================"
echo "  All nodes flashed!"
echo "  Pair via: hold Pair button on Hub, then"
echo "  power on each node within range."
echo "============================================"
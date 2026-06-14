#!/bin/bash
# UrbanHarvest — OTA Update Script
# Sends firmware update over mesh to a specific node
# Uses OTA_BLOCK messages (type 0x0B) chunked into 48-byte blocks

set -e

echo "============================================"
echo "  UrbanHarvest — OTA Firmware Update"
echo "============================================"

NODE_ID=${1:?"Usage: $0 <node_id> <firmware.bin>"}
FIRMWARE_BIN=${2:?"Usage: $0 <node_id> <firmware.bin>"}
MQTT_BROKER=${3:-"localhost"}
MQTT_PORT=${4:-1883}

if [ ! -f "$FIRMWARE_BIN" ]; then
    echo "ERROR: Firmware file not found: $FIRMWARE_BIN"
    exit 1
fi

FIRMWARE_SIZE=$(stat -c%s "$FIRMWARE_BIN" 2>/dev/null || stat -f%z "$FIRMWARE_BIN" 2>/dev/null)
CHUNK_SIZE=46  # 48-byte payload minus 2 bytes header (block_index, total_blocks)
TOTAL_BLOCKS=$(( (FIRMWARE_SIZE + CHUNK_SIZE - 1) / CHUNK_SIZE ))

echo "Target:  Node #$NODE_ID"
echo "Firmware: $FIRMWARE_BIN"
echo "Size:     $FIRMWARE_SIZE bytes"
echo "Chunks:   $TOTAL_BLOCKS"
echo ""

# Publish OTA start command
echo "Sending OTA start notification..."
mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
    -t "urbanharvest/ota/node_${NODE_ID}" \
    -m "{\"action\": \"start\", \"node_id\": $NODE_ID, \"total_blocks\": $TOTAL_BLOCKS, \"firmware_size\": $FIRMWARE_SIZE}" 2>/dev/null || true

echo "Sending firmware chunks..."

BLOCK_INDEX=0
OFFSET=0

while [ $OFFSET -lt $FIRMWARE_SIZE ]; do
    # Read chunk of firmware
    CHUNK=$(dd if="$FIRMWARE_BIN" bs=1 skip=$OFFSET count=$CHUNK_SIZE 2>/dev/null | base64)
    REMAINING=$(( FIRMWARE_SIZE - OFFSET ))
    if [ $REMAINING -lt $CHUNK_SIZE ]; then
        ACTUAL_SIZE=$REMAINING
    else
        ACTUAL_SIZE=$CHUNK_SIZE
    fi

    # Publish chunk
    mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
        -t "urbanharvest/ota/node_${NODE_ID}" \
        -m "{\"action\": \"chunk\", \"block_index\": $BLOCK_INDEX, \"total_blocks\": $TOTAL_BLOCKS, \"size\": $ACTUAL_SIZE, \"data\": \"$CHUNK\"}" 2>/dev/null || true

    # Progress
    PROGRESS=$(( (BLOCK_INDEX + 1) * 100 / TOTAL_BLOCKS ))
    printf "\r  Progress: %d/%d blocks (%d%%)" "$((BLOCK_INDEX + 1))" "$TOTAL_BLOCKS" "$PROGRESS"

    BLOCK_INDEX=$((BLOCK_INDEX + 1))
    OFFSET=$((OFFSET + CHUNK_SIZE))

    # Small delay to avoid overwhelming the mesh
    sleep 0.1
done

echo ""
echo ""

# Send OTA complete command
echo "Sending OTA complete notification..."
mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
    -t "urbanharvest/ota/node_${NODE_ID}" \
    -m "{\"action\": \"complete\", \"node_id\": $NODE_ID, \"total_blocks\": $TOTAL_BLOCKS, \"checksum\": \"$(sha256sum "$FIRMWARE_BIN" | cut -d' ' -f1)\"}" 2>/dev/null || true

echo ""
echo "============================================"
echo "  OTA Update Sent!"
echo "============================================"
echo ""
echo "Node #$NODE_ID will verify checksum and apply update."
echo "Monitor the node's health via the mobile app."
echo ""
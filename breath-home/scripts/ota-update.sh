#!/bin/bash
# BreathHome - OTA Firmware Update Script
# Sends firmware updates to sensor nodes over Sub-GHz mesh.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$SCRIPT_DIR/../firmware"
NODE_TYPES=("hub-node" "room-sensor" "hvac-controller" "wearable-tag")

echo "========================================="
echo "  BreathHome OTA Firmware Update"
echo "========================================="
echo ""
echo "Available node types:"
for i in "${!NODE_TYPES[@]}"; do
  echo "  $((i+1))) ${NODE_TYPES[$i]}"
done
echo ""
echo "Select node type to update (1-${#NODE_TYPES[@]}):"
read -r TYPE_IDX

NODE_TYPE="${NODE_TYPES[$((TYPE_IDX-1))]}"
if [ -z "$NODE_TYPE" ]; then
  echo "Invalid selection"
  exit 1
fi

# Find firmware binary
FIRMWARE_BIN=$(find "$FIRMWARE_DIR/$NODE_TYPE" -name "*.bin" -o -name "*.hex" 2>/dev/null | head -1)
if [ -z "$FIRMWARE_BIN" ]; then
  echo "Error: No firmware binary found for $NODE_TYPE"
  echo "Build the firmware first using the appropriate build script."
  exit 1
fi

FIRMWARE_SIZE=$(stat -c%s "$FIRMWARE_BIN")
echo ""
echo "Firmware: $FIRMWARE_BIN"
echo "Size: $FIRMWARE_SIZE bytes ($((FIRMWARE_SIZE/1024))KB)"
echo ""

# MQTT configuration
MQTT_HOST=${MQTT_HOST:-"breathhome.local"}
MQTT_PORT=${MQTT_PORT:-1883}
MQTT_USER=${MQTT_USER:-"breathhome"}
MQTT_PASS=${MQTT_PASS:-"breathhome_secret_2026"}

# Calculate chunks (48 bytes per OTA packet, per mesh protocol spec)
CHUNK_SIZE=48
NUM_CHUNKS=$(( (FIRMWARE_SIZE + CHUNK_SIZE - 1) / CHUNK_SIZE ))

echo "OTA update will send $NUM_CHUNKS chunks over MQTT."
echo "The hub will relay to the target node via Sub-GHz mesh."
echo ""

echo "Enter target Node ID (or 'all' for broadcast):"
read -r TARGET_NODE

echo ""
echo "WARNING: This will update the firmware on $NODE_TYPE node(s)."
echo "Do NOT power off the node during the update."
echo ""
read -p "Continue? (y/N): " CONFIRM

if [ "$CONFIRM" != "y" ] && [ "$CONFIRM" != "Y" ]; then
  echo "Aborted."
  exit 0
fi

# Send OTA start command
echo "Starting OTA update..."
mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASS \
  -t "breathhome/ota/$NODE_TYPE/start" \
  -m "{\"node_type\":\"$NODE_TYPE\",\"target_node\":\"$TARGET_NODE\",\"total_chunks\":$NUM_CHUNKS,\"firmware_size\":$FIRMWARE_SIZE}"

sleep 2

# Send firmware in chunks
CHUNK_NUM=0
while IFS= read -r -n $CHUNK_SIZE CHUNK; do
  # Base64 encode the chunk for MQTT transport
  B64_CHUNK=$(echo -n "$CHUNK" | base64)
  
  mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASS \
    -t "breathhome/ota/$NODE_TYPE/chunk" \
    -m "{\"chunk_num\":$CHUNK_NUM,\"data\":\"$B64_CHUNK\"}"
  
  CHUNK_NUM=$((CHUNK_NUM + 1))
  
  # Progress indicator
  if [ $((CHUNK_NUM % 10)) -eq 0 ]; then
    PCT=$((CHUNK_NUM * 100 / NUM_CHUNKS))
    echo -ne "Progress: $PCT% ($CHUNK_NUM/$NUM_CHUNKS chunks)\r"
  fi
  
  # Rate limit to avoid overwhelming the mesh
  sleep 0.1
done < "$FIRMWARE_BIN"

echo ""
echo "Sending OTA complete command..."
mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASS \
  -t "breathhome/ota/$NODE_TYPE/complete" \
  -m "{\"node_type\":\"$NODE_TYPE\",\"target_node\":\"$TARGET_NODE\",\"total_chunks\":$NUM_CHUNKS}"

echo ""
echo "========================================="
echo "  OTA Update Sent!"
echo "========================================="
echo ""
echo "  The node will verify the firmware CRC, apply the update,"
echo "  and reboot. This may take up to 2 minutes."
echo ""
echo "  Monitor the node status in the BreathHome dashboard"
echo "  or subscribe to: breathhome/ota/$NODE_TYPE/status"
#!/bin/bash
# AllergySync — Window Node Calibration Script
# Triggers the calibration routine on a Window Node via the cloud API
set -euo pipefail

API_BASE="${ALLERGYSYNC_API:-http://localhost:8000/api/v1}"
NODE_ID="${1:-}"

if [ -z "$NODE_ID" ]; then
  echo "Usage: $0 <window-node-id>"
  echo "Example: $0 2"
  exit 1
fi

echo "=== AllergySync Window Node Calibration ==="
echo "Node ID: $NODE_ID"
echo ""

# Send recalibrate command via MQTT (through API → hub → mesh)
echo "Sending RECALIBRATE command to node $NODE_ID..."
# In production, this would POST to the API which forwards via MQTT to hub
# For now, we use the OTA endpoint pattern as a command trigger
curl -s -X POST "$API_BASE/nodes/$NODE_ID/ota?version=calibrate" || true

echo ""
echo "The window node will:"
echo "  1. Drive the stepper motor to fully close the window"
echo "  2. Wait for the reed switch to trigger (closed position)"
echo "  3. Reset step counter to 0"
echo "  4. Report calibrated state via telemetry"
echo ""
echo "Check the mobile app (Settings → Nodes) for calibration status."
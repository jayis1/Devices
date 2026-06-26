#!/bin/bash
# BrewSync — Calibrate Fermenter Node Sensors
# Usage: ./calibrate.sh <node_id> [--sg <reference_sg>] [--ph <reference_ph>] [--temp <reference_temp_c>]

set -euo pipefail

HUB_URL="${BREWSYNC_HUB_URL:-http://brewsync-hub.local:8080}"
NODE_ID="${1:?Usage: calibrate.sh <node_id> [--sg <ref>] [--ph <ref>] [--temp <ref>]}"

REF_SG=""
REF_PH=""
REF_TEMP=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --sg)   REF_SG="$2"; shift 2 ;;
        --ph)   REF_PH="$2"; shift 2 ;;
        --temp) REF_TEMP="$2"; shift 2 ;;
        *) shift ;;
    esac
done

echo "======================================"
echo "BrewSync Fermenter Node Calibration"
echo "Node: $NODE_ID"
echo "Hub:  $HUB_URL"
echo "======================================"

# Check node connectivity
echo "[1/4] Checking node connectivity..."
STATUS=$(curl -sf "$HUB_URL/v1/nodes/$NODE_ID/status" || echo '{"online": false}')
ONLINE=$(echo "$STATUS" | python3 -c "import sys,json; print(json.load(sys.stdin).get('online', False))" 2>/dev/null || echo "False")

if [ "$ONLINE" != "True" ]; then
    echo "❌ Node $NODE_ID is not online!"
    echo "Make sure the node is powered on and paired with the Hub."
    exit 1
fi
echo "✅ Node is online"

# Calibrate temperature sensor
if [ -n "$REF_TEMP" ]; then
    echo ""
    echo "[2/4] Calibrating temperature sensor..."
    echo "Reference temperature: ${REF_TEMP}°C"
    echo "Place the DS18B20 probe in a known-temperature water bath."
    echo "Current reading:"
    READING=$(curl -sf "$HUB_URL/v1/batches/latest/readings?limit=1" | python3 -c "
import sys, json
data = json.load(sys.stdin)
if data:
    print(f'  Temperature: {data[0].get(\"temp_c\", \"N/A\")}°C')
    print(f'  Offset: {float(data[0].get('temp_c', 0)) - $REF_TEMP}°C')
" 2>/dev/null || echo "  (no reading available)")
    echo "$READING"
    echo ""
    echo "Send calibration command to node..."
    curl -sf -X POST "$HUB_URL/v1/nodes/$NODE_ID/command" \
        -H "Content-Type: application/json" \
        -d "{\"cmd\": 5, \"params\": [2, $(python3 -c "print(int(float('$REF_TEMP') * 100))")]}" > /dev/null
    echo "✅ Temperature calibration offset applied"
else
    echo "[2/4] Skipping temperature calibration (no --temp reference provided)"
fi

# Calibrate SG (tilt sensor)
if [ -n "$REF_SG" ]; then
    echo ""
    echo "[3/4] Calibrating SG (tilt) sensor..."
    echo "Reference SG: $REF_SG"
    echo "Place the fermenter node in a reference liquid (distilled water = 1.000)"
    echo "Current SG reading from node..."
    curl -sf "$HUB_URL/v1/nodes/$NODE_ID/calibrate" \
        -H "Content-Type: application/json" \
        -d "{\"sensor\": \"sg\", \"reference_value\": $REF_SG}" > /dev/null
    echo "✅ SG calibration applied"
else
    echo "[3/4] Skipping SG calibration (no --sg reference provided)"
fi

# Calibrate pH sensor
if [ -n "$REF_PH" ]; then
    echo ""
    echo "[4/4] Calibrating pH sensor..."
    echo "Reference pH: $REF_PH"
    echo "Place pH probe in reference buffer solution."
    curl -sf "$HUB_URL/v1/nodes/$NODE_ID/calibrate" \
        -H "Content-Type: application/json" \
        -d "{\"sensor\": \"ph\", \"reference_value\": $REF_PH}" > /dev/null
    echo "✅ pH calibration applied"
else
    echo "[4/4] Skipping pH calibration (no --ph reference provided)"
fi

echo ""
echo "======================================"
echo "Calibration complete!"
echo "======================================"
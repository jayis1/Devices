#!/bin/bash
# BrewSync — Pair a new node with the Hub
# Usage: ./pair-node.sh <node_type> [node_id]

set -euo pipefail

HUB_URL="${BREWSYNC_HUB_URL:-http://brewsync-hub.local:8080}"
NODE_TYPE="${1:?Usage: pair-node.sh <fermenter|cellar|scanner> [node_id]}"
NODE_ID="${2:-auto}"

echo "======================================"
echo "BrewSync Node Pairing"
echo "Node type: $NODE_TYPE"
echo "Hub:       $HUB_URL"
echo "======================================"

# Put Hub in pairing mode
echo "[1/3] Putting Hub in pairing mode..."
curl -sf -X POST "$HUB_URL/v1/nodes/pairing/start" \
    -H "Content-Type: application/json" \
    -d "{\"node_type\": \"$NODE_TYPE\", \"timeout\": 60}" > /dev/null

echo "✅ Hub is now listening for new nodes (60s timeout)"
echo ""
echo "[2/3] Power on your new $NODE_TYPE node..."
echo "  The node will automatically pair with the Hub."
echo "  LED should blink rapidly during pairing, then stay solid when paired."
echo ""
echo "Press Enter when the LED is solid (or after 60s)..."
read -r

# Check pairing result
echo "[3/3] Checking pairing status..."
RESULT=$(curl -sf "$HUB_URL/v1/nodes/pairing/status" 2>/dev/null || echo '{}')

NODE_ID=$(echo "$RESULT" | python3 -c "
import sys, json
data = json.load(sys.stdin)
print(data.get('node_id', 'unknown'))
" 2>/dev/null || echo "unknown")

if [ "$NODE_ID" != "unknown" ]; then
    echo "✅ Node paired successfully!"
    echo "   Node ID: $NODE_ID"
    echo "   Type:    $NODE_TYPE"
    echo ""
    echo "You can now assign this node to a batch via the mobile app."
else
    echo "❌ Pairing failed or timed out."
    echo "   Make sure the node is powered on and within radio range."
    exit 1
fi
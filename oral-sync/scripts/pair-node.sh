#!/usr/bin/env bash
# Pair an OralSync node with the Hub over USB-CDC console.
# Generates a per-node AES-128 session key and provisions it.
# Usage: ./pair-node.sh <node-type> <port> [node-id]
set -euo pipefail

NODE="${1:?usage: $0 <toothbrush|scanner|saliva> <port> [node-id]}"
PORT="${2:?port required, e.g. /dev/ttyACM0}"
NODE_ID="${3:-$(hostname)-$NODE-$(date +%s | tail -c4)}"

if ! command -v python3 &>/dev/null; then echo "[err] python3 required"; exit 1; fi

KEY=$(python3 -c "import secrets; print(secrets.token_hex(16))")
echo "[pair] node_id=$NODE_ID  key=$KEY"

# Send PAIR command over serial (OSMP PAIR_REQ with factory key)
python3 - "$PORT" "$NODE_ID" "$KEY" <<'PY'
import serial, sys, time, struct
port, node_id, key_hex = sys.argv[1], sys.argv[2], sys.argv[3]
key = bytes.fromhex(key_hex)
ser = serial.Serial(port, 115200, timeout=2)
time.sleep(0.5)
ser.write(b"PAIR " + node_id.encode() + b" " + key_hex.encode() + b"\n")
resp = ser.read(256)
print("[pair] response:", resp.decode(errors="replace").strip())
ser.close()
PY

echo "[ok] node paired. Store this key securely — it is the AES-128 session key."
echo "  key: $KEY"
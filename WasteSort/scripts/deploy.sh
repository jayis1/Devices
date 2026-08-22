#!/bin/bash
set -euo pipefail

echo "=== WasteSort deployment ==="
cd "$(dirname "$0")/../software/dashboard"

echo "[1/3] Building image"
docker build -t wastesort/dashboard:latest .

echo "[2/3] Launching container"
docker rm -f wastesort-api >/dev/null 2>&1 || true
docker run -d   --name wastesort-api   -p 8000:8000   -e MQTT_BROKER=broker.wastesort.local   -e DATABASE_URL=postgresql://wastesort:wastesort@db/wastesort   --restart unless-stopped   wastesort/dashboard:latest

echo "[3/3] Health check"
sleep 2
curl -sf http://localhost:8000/api/v1/health || {
  echo "backend not responding" >&2
  exit 1
}

echo "Deployment complete"

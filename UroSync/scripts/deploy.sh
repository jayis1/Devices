#!/bin/bash
set -euo pipefail

echo "=== UroSync deployment ==="
cd "$(dirname "$0")/../software/dashboard"

echo "[1/3] Building image"
docker build -t urosync/dashboard:latest .

echo "[2/3] Launching container"
docker rm -f urosync-api >/dev/null 2>&1 || true
docker run -d   --name urosync-api   -p 8000:8000   -e MQTT_BROKER=broker.urosync.local   -e DATABASE_URL=postgresql://urosync:***@db/urosync   --restart unless-stopped   urosync/dashboard:latest

echo "[3/3] Health check"
sleep 2
curl -sf http://localhost:8000/api/v1/health || {
  echo "backend not responding" >&2
  exit 1
}

echo "Deployment complete"

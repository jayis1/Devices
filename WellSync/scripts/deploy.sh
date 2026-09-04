#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export WELLSYNC_ENV=${WELLSYNC_ENV:-demo}
export PYTHONPATH="$ROOT_DIR/software/dashboard"
exec "$ROOT_DIR/software/dashboard/.venv/bin/uvicorn" main:app --host 0.0.0.0 --port 8094

#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/software/dashboard"

echo "[DrainSync] create virtualenv if missing"
python3 -m venv .venv || true
source .venv/bin/activate
pip install -r requirements.txt

echo "[DrainSync] starting API"
uvicorn main:app --host 0.0.0.0 --port 8060

#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"
python3 -m venv .venv
. .venv/bin/activate
pip install --upgrade pip
pip install -r software/dashboard/requirements.txt
cd software/dashboard
uvicorn main:app --host 0.0.0.0 --port 8046

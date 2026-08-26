#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../software/dashboard"
python3 -m uvicorn main:app --host 0.0.0.0 --port 8099

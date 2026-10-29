#!/usr/bin/env bash
set -euo pipefail
python3 -m uvicorn app:app --app-dir "$(dirname "$0")/../software/dashboard" --host 127.0.0.1

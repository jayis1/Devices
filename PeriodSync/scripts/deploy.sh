#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source .venv-periodsync/bin/activate
uvicorn software.dashboard.main:app --host 0.0.0.0 --port 8010

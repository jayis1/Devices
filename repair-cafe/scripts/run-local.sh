#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT/software/dashboard"
exec python -m uvicorn main:app --host 127.0.0.1 --port 8000

#!/bin/sh
set -eu

APP_DIR="$(CDPATH= cd -- "$(dirname "$0")/../software/dashboard" && pwd)"
cd "$APP_DIR"
uvicorn main:app --host 0.0.0.0 --port 8095

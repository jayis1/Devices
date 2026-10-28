#!/bin/sh
set -eu
cd "$(dirname "$0")/../software/dashboard"
python3 -m pip install -r requirements.txt
exec python3 -m uvicorn main:app --host 127.0.0.1 --port 8000

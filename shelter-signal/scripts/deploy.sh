#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 -m pip install -r software/dashboard/requirements.txt
printf '%s\n' 'Demo dependencies installed. Run uvicorn on 127.0.0.1 only; not a production deployment.'

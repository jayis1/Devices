#!/usr/bin/env bash
set -euo pipefail
python3 -m venv .venv
source .venv/bin/activate
pip install -r software/dashboard/requirements.txt
python -m uvicorn app.main:app --app-dir software/dashboard --host 0.0.0.0 --port 8055

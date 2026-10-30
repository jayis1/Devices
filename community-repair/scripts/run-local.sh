#!/bin/sh
set -eu
python -m venv .venv
. .venv/bin/activate
pip install -r software/dashboard/requirements.txt
exec uvicorn app:app --app-dir software/dashboard --host 127.0.0.1 --port 8000

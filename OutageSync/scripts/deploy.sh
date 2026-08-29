#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 -m venv .venv
. .venv/bin/activate
pip install -r software/dashboard/requirements.txt -r software/ml-pipeline/requirements.txt
echo "Run: uvicorn software.dashboard.main:app --reload"
echo "Models: python software/ml-pipeline/train_outage_duration.py"

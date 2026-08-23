#!/usr/bin/env bash
set -euo pipefail
python3 -m venv .venv-periodsync
source .venv-periodsync/bin/activate
python -m pip install --upgrade pip
python -m pip install -r software/dashboard/requirements.txt -r software/ml-pipeline/requirements.txt

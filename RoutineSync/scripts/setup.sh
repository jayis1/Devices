#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r software/dashboard/requirements.txt
pip install -r software/ml-pipeline/requirements.txt
printf 'RoutineSync environment ready.
'

#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
python3 -m py_compile software/dashboard/main.py software/dashboard/ml_inference.py software/dashboard/models.py
for script in software/ml-pipeline/*.py; do
  python3 "$script"
done
printf 'MoldSync deployment checks complete.
'

#!/bin/bash
set -euo pipefail

python3 -m venv .venv-urosync
source .venv-urosync/bin/activate
pip install --upgrade pip
pip install -r software/dashboard/requirements.txt -r software/ml-pipeline/requirements.txt

echo "UroSync development environment ready"

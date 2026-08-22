#!/bin/bash
set -euo pipefail

echo "Setting up WasteSort development environment"
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r software/dashboard/requirements.txt
pip install -r software/ml-pipeline/requirements.txt

echo "Done. Activate with: source .venv/bin/activate"

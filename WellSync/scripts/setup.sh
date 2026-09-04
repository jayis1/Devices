#!/usr/bin/env bash
set -euo pipefail
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r software/dashboard/requirements.txt
python software/ml-pipeline/train_contamination_risk.py
python software/ml-pipeline/train_pump_failure.py
python software/ml-pipeline/train_dry_well_forecast.py
python software/ml-pipeline/train_treatment_integrity.py
python software/ml-pipeline/train_service_priority.py

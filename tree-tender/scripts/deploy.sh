#!/bin/sh
set -eu
python3 -m venv .venv
. .venv/bin/activate
pip install -r software/dashboard/requirements.txt
printf '%s\n' 'Set MQTT credentials in the environment before deployment; do not commit secrets.'

#!/bin/sh
set -eu
printf '%s\n' 'ReadTogether development setup only. This script does not deploy services or flash hardware.'
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r software/dashboard/requirements.txt

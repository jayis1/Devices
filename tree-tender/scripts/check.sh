#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 -m unittest discover -s software/dashboard/tests
python3 -m py_compile software/dashboard/main.py software/dashboard/tests/test_api.py software/ml-pipeline/*.py scripts/*.py

#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 firmware/common/test_protocol.py
python3 -m unittest discover -s software/dashboard -v
python3 -m py_compile software/dashboard/main.py software/ml-pipeline/train.py

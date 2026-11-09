#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/tests/test_protocol.c -o /tmp/mendmate-protocol-test
/tmp/mendmate-protocol-test
python3 -m unittest discover -s software/dashboard/tests -v
python3 -m py_compile software/dashboard/main.py software/ml-pipeline/train.py
python3 software/ml-pipeline/train.py

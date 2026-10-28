#!/bin/sh
set -eu
cd "$(dirname "$0")/../firmware/common"
cc -std=c11 -Wall -Wextra -Werror -I. protocol.c test_protocol.c -o /tmp/marketchill-test
/tmp/marketchill-test
python3 -m py_compile ../../software/dashboard/main.py ../../software/ml-pipeline/train.py ../../scripts/calibration_offset.py

#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/cookshare-test
/tmp/cookshare-test
python -m py_compile software/dashboard/app.py software/dashboard/tests/test_api.py software/ml-pipeline/train.py

#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cc -std=c11 -Wall -Wextra -Werror "$ROOT/firmware/common/protocol.c" "$ROOT/firmware/tests/protocol_test.c" -o /tmp/chargeplan-test
/tmp/chargeplan-test
python3 -m unittest discover -s "$ROOT/software/dashboard" -v
python3 "$ROOT/software/ml-pipeline/train.py" --smoke

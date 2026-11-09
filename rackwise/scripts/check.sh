#!/bin/sh
set -eu
# Non-destructive local prototype checks; no hardware flashing or network changes.
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 -m unittest discover -s "$ROOT/software/dashboard/tests" -v
python3 -m py_compile "$ROOT/software/dashboard/main.py" "$ROOT/software/ml-pipeline/train.py"
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware/common" "$ROOT/firmware/common/protocol.c" "$ROOT/firmware/tests/test_protocol.c" -o /tmp/rackwise-protocol-test
/tmp/rackwise-protocol-test

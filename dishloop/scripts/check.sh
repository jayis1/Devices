#!/bin/sh
set -eu
python3 -m unittest -v software/dashboard/test_main.py
cc -std=c11 -Wall -Wextra -Werror firmware/common/protocol_test.c -lm -o /tmp/dishloop-test
/tmp/dishloop-test
python3 -m py_compile software/dashboard/main.py software/ml-pipeline/train.py

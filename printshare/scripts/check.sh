#!/bin/sh
set -eu
python3 -m pytest software/dashboard/test_app.py
python3 -m py_compile software/dashboard/app.py software/dashboard/test_app.py software/ml-pipeline/train.py
cc -std=c99 -Wall -Wextra -Werror -fsyntax-only firmware/common/protocol.c firmware/gateway/main.c firmware/printer-observer/main.c firmware/filament-sentinel/main.c
python3 -m json.tool software/mobile-app/package.json >/dev/null
python3 -m json.tool software/mobile-app/app.json >/dev/null

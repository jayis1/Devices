#!/usr/bin/env bash
# Usage: ./scripts/check.sh (from repository root)
set -euo pipefail
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -pedantic -Ifirmware/common -fsyntax-only firmware/common/protocol.c firmware/cabinet-sentinel/main.c firmware/inspection-beacon/main.c
cc -std=c11 -Wall -Wextra -Werror -pedantic -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/aedready-protocol-test
/tmp/aedready-protocol-test
python3 -m py_compile software/dashboard/main.py software/dashboard/test_main.py software/ml-pipeline/train.py
python3 software/ml-pipeline/train.py --epochs 1
python3 -m json.tool software/mobile-app/package.json >/dev/null

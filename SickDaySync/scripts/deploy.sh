#!/usr/bin/env bash
set -euo pipefail
python3 -m unittest discover -s software/dashboard/tests
for script in software/ml-pipeline/train_*.py; do
  python3 "$script"
done
gcc -std=c11 -Wall -Wextra -I firmware/common -fsyntax-only firmware/common/protocol.c firmware/hub/main.c firmware/recovery-band/main.c firmware/room-sentinel/main.c firmware/med-station/main.c firmware/vent-controller/main.c
printf 'SickDaySync validation complete.
'

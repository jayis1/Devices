#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=${TMPDIR:-/tmp}/pbl-protocol-test-$$
TMPDIR_WORK=${TMP}.d
mkdir "$TMPDIR_WORK"
trap 'rm -rf "$TMP" "$TMPDIR_WORK"' EXIT
cc -std=c11 -Wall -Wextra -Werror "$ROOT/firmware/common/protocol.c" "$ROOT/firmware/common/test_protocol.c" -o "$TMP"
"$TMP"
for role in gateway loan-dock pack-tag; do
  source="$ROOT/firmware/$role/main.c"
  cc -std=c11 -Wall -Wextra -Werror -c "$source" -o "$TMPDIR_WORK/$role.o"
done
python3 - "$ROOT" <<'PY'
import ast, pathlib, sys
root=pathlib.Path(sys.argv[1])
for rel in ('software/dashboard/app.py','software/dashboard/test_app.py','software/dashboard/mqtt_bridge.py','software/ml-pipeline/generate.py','software/ml-pipeline/train.py'):
    ast.parse((root/rel).read_text(), filename=rel)
PY
(cd "$ROOT/software/dashboard" && PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -p no:cacheprovider)
python3 "$ROOT/software/ml-pipeline/generate.py" --help >/dev/null
python3 - "$ROOT" <<'PY'
import csv, pathlib, sys
root=pathlib.Path(sys.argv[1])
for p in (root/'hardware/bom').glob('*.csv'):
    with p.open(newline='') as f:
        rows=list(csv.DictReader(f)); assert rows and {'Item','Manufacturer','Part number','Qty','Notes'}.issubset(rows[0]), p
    print(f'{p.name}: {len(rows)} BOM lines')
PY
(cd "$ROOT/software/ml-pipeline" && python3 generate.py "$TMPDIR_WORK/sample.csv" >/dev/null && python3 train.py "$TMPDIR_WORK/sample.csv")
printf '%s\n' 'Validation passed (host-only; no embedded or hardware validation).'

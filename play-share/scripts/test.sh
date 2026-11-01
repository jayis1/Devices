#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
python3 -m py_compile "$ROOT/software/dashboard/app.py" "$ROOT/software/dashboard/test_app.py" "$ROOT/software/ml-pipeline/train.py"
python3 -m json.tool "$ROOT/software/mobile-app/package.json" >/dev/null
python3 - "$ROOT" <<'PY'
import csv, pathlib, sys
root=pathlib.Path(sys.argv[1])
for p in root.glob('hardware/bom/*/BOM.csv'):
    rows=list(csv.reader(p.open(newline='')))
    assert rows and len({len(r) for r in rows}) == 1, p
    assert len(rows[0]) == 9, p
print('BOM CSV shape: OK')
PY
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware/common" "$ROOT/firmware/common/protocol.c" "$ROOT/firmware/common/test_protocol.c" -lm -o "$ROOT/.protocol-test"
"$ROOT/.protocol-test"
rm -f "$ROOT/.protocol-test"
PYTHONPATH="$ROOT/software/dashboard" python3 -m unittest discover -s "$ROOT/software/dashboard" -p 'test_*.py'
python3 "$ROOT/software/ml-pipeline/train.py" --synthetic

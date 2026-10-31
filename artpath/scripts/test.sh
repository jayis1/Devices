#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${TMPDIR:-$HOME/.hermes/cache/scratch}/artpath-protocol-test
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware/common" "$ROOT/firmware/common/protocol.c" "$ROOT/firmware/common/test_protocol.c" -o "$OUT"
"$OUT"
python3 -c 'import ast,pathlib,sys; [ast.parse(pathlib.Path(p).read_text(), filename=p) for p in sys.argv[1:]]' "$ROOT/software/dashboard/app.py" "$ROOT/software/dashboard/tests/test_api.py" "$ROOT/software/ml-pipeline/generate_fixture.py"
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$ROOT/software/dashboard${PYTHONPATH:+:$PYTHONPATH}" python3 -m unittest discover -s "$ROOT/software/dashboard/tests"
python3 "$ROOT/software/ml-pipeline/generate_fixture.py"

#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT="${TMPDIR:-/tmp}/tuneshare-protocol-test"
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware/common" "$ROOT/firmware/common/protocol.c" "$ROOT/firmware/common/test_protocol.c" -o "$OUT"
"$OUT"
for node in gateway climate rack checkin; do
  src="$ROOT/firmware/$node/main.c"
  cc -std=c11 -Wall -Wextra -Werror -fsyntax-only -I"$ROOT/firmware/common" "$src"
done
printf '%s\n' 'C host protocol and node syntax checks passed'

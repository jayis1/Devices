#!/usr/bin/env sh
set -eu
# Run only on loopback until authentication/TLS are implemented.
cd "$(dirname "$0")/../software/dashboard"
export CLAIMLOOP_DB="${CLAIMLOOP_DB:-$PWD/claimloop.sqlite3}"
exec uvicorn main:app --host 127.0.0.1 --port "${PORT:-8000}"

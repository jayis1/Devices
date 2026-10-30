#!/bin/sh
set -eu
# Placeholder: pass the board-specific flashing command explicitly after SDK setup.
if [ "$#" -lt 1 ]; then echo 'Usage: flash-node <vendor-flasher> [args...]' >&2; exit 2; fi
exec "$@"

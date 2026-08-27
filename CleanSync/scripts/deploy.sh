#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
docker build -t cleansync-api ./software/dashboard
printf 'Built cleansync-api image successfully.
'

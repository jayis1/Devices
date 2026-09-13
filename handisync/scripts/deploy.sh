#!/bin/sh
set -eu
: "${HANDISYNC_API_TOKEN:?set HANDISYNC_API_TOKEN}"
cd "$(dirname "$0")/../software/dashboard"
docker compose up --build -d

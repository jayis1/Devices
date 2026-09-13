#!/usr/bin/env bash
set -euo pipefail
: "${MAINTAINSYNC_API_TOKEN:?set MAINTAINSYNC_API_TOKEN}"
cd "$(dirname "$0")/../software/dashboard"
docker compose up --build -d

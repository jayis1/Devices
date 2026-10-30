#!/bin/sh
set -eu
: "${API_URL:=http://127.0.0.1:8000}"
printf 'SeedShare local API: %s\n' "$API_URL"
exec uvicorn app:app --app-dir "$(dirname "$0")/../software/dashboard" --host 127.0.0.1 --port 8000

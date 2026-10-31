# Prototype run script; no privileged operations or public binding.
set -eu
cd "$(dirname "$0")/../software/dashboard"
exec uvicorn app:app --host 127.0.0.1 --port "${PORT:-8000}"

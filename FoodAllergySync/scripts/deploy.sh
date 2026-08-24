#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR/software/dashboard"

echo "[deploy] FoodAllergySync dashboard container build stub"
echo "[deploy] run: docker build -t foodallergysync-dashboard ."
echo "[deploy] run: docker run -p 8080:8080 foodallergysync-dashboard"

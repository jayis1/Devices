#!/usr/bin/env bash
# OralSync cloud stack deployment (docker-compose)
# Brings up: FastAPI dashboard, PostgreSQL+TimescaleDB, MQTT broker, ML inference service
set -euo pipefail

COMPOSE_DIR="${1:-.}"
cd "$COMPOSE_DIR"

if ! command -v docker &>/dev/null; then
  echo "[err] docker not found"; exit 1
fi

cat > docker-compose.yml <<'YML'
version: "3.8"
services:
  db:
    image: timescale/timescaledb:latest-pg15
    environment:
      POSTGRES_USER: oralsync
      POSTGRES_PASSWORD: oralsync_dev
      POSTGRES_DB: oralsync
    ports: ["5432:5432"]
    volumes: ["dbdata:/var/lib/postgresql/data"]
  mqtt:
    image: eclipse-mosquitto:2
    ports: ["1883:1883"]
    volumes: ["./mosquitto.conf:/mosquitto/config/mosquitto.conf"]
  dashboard:
    build: ./software/dashboard
    ports: ["8000:8000"]
    depends_on: [db, mqtt]
    env_file: .env
  ml:
    image: oralsync/ml-service:latest
    ports: ["8501:8501"]
    volumes: ["./software/ml-pipeline/artifacts:/models:ro"]
volumes:
  dbdata:
YML

cat > mosquitto.conf <<'CONF'
listener 1883
allow_anonymous true
CONF

cat > .env <<'ENV'
DATABASE_URL=postgresql://oralsync:oralsync_dev@db:5432/oralsync
MQTT_HOST=mqtt
MQTT_PORT=1883
JWT_SECRET=change-me-in-production
ML_SERVICE_URL=http://ml:8501
ENV

docker compose up -d --build
echo "[ok] OralSync cloud stack up — dashboard at http://localhost:8000/v1/health"
#!/bin/bash
# AsthmaSync — Deployment Script
# Deploys the cloud backend (FastAPI + PostgreSQL + TimescaleDB + MQTT broker)
#
# Prerequisites:
#   - Docker + Docker Compose installed
#   - Domain name pointing to server (for TLS)
#   - MQTT broker credentials configured
#
# License: MIT

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "╔══════════════════════════════════════════╗"
echo "║   AsthmaSync — Cloud Backend Deployment  ║"
echo "╚══════════════════════════════════════════╝"

# Check Docker
if ! command -v docker &> /dev/null; then
    echo "❌ Docker not installed. Install: https://docs.docker.com/get-docker/"
    exit 1
fi

if ! command -v docker compose &> /dev/null; then
    echo "❌ Docker Compose not installed."
    exit 1
fi

# Create data directories
mkdir -p "$PROJECT_DIR/data/postgres"
mkdir -p "$PROJECT_DIR/data/mqtt"

# Check for .env file
if [ ! -f "$PROJECT_DIR/.env" ]; then
    echo "⚠️  No .env file found. Creating from template..."
    cat > "$PROJECT_DIR/.env" <<EOF
# Database
POSTGRES_USER=asthmasync
POSTGRES_PASSWORD=change_me_in_production
POSTGRES_DB=asthmasync
DATABASE_URL=postgresql://asthmasync:change_me_in_production@db:5432/asthmasync

# MQTT Broker
MQTT_BROKER=mqtt
MQTT_PORT=1883

# API
API_HOST=0.0.0.0
API_PORT=8000

# Security
SECRET_KEY=$(openssl rand -hex 32)
JWT_ALGORITHM=HS256
EOF
    echo "✅ Created .env file (update passwords before production!)"
fi

# Build and start
echo "🔨 Building Docker images..."
docker compose -f "$PROJECT_DIR/docker-compose.yml" build

echo "🚀 Starting services..."
docker compose -f "$PROJECT_DIR/docker-compose.yml" up -d

echo ""
echo "✅ Deployment complete!"
echo ""
echo "Services:"
echo "  - API:     http://localhost:8000"
echo "  - API docs: http://localhost:8000/docs"
echo "  - DB:      localhost:5432"
echo "  - MQTT:    localhost:1883"
echo ""
echo "Next steps:"
echo "  1. Update .env with strong passwords"
echo "  2. Set up TLS (use nginx + Let's Encrypt)"
echo "  3. Run ML pipeline: cd software/ml-pipeline && python train_*.py"
echo "  4. Flash firmware to nodes (see scripts/flash_all.sh)"
#!/bin/bash
# AllergySync — Deploy Backend
# Sets up PostgreSQL, Redis, MQTT broker, and FastAPI backend
set -euo pipefail

echo "=== AllergySync Backend Deployment ==="

# Check dependencies
command -v docker >/dev/null 2>&1 || { echo "Error: docker not installed"; exit 1; }
command -v docker-compose >/dev/null 2>&1 || command -v docker compose >/dev/null 2>&1 || { echo "Error: docker-compose not installed"; exit 1; }

# Create docker-compose.yml if not exists
if [ ! -f docker-compose.yml ]; then
cat > docker-compose.yml << 'COMPOSE'
version: '3.8'
services:
  postgres:
    image: postgres:16
    environment:
      POSTGRES_USER: allergysync
      POSTGRES_PASSWORD: allergysync_dev
      POSTGRES_DB: allergysync
    ports: ["5432:5432"]
    volumes: ["pgdata:/var/lib/postgresql/data"]

  redis:
    image: redis:7-alpine
    ports: ["6379:6379"]

  mosquitto:
    image: eclipse-mosquitto:2
    ports: ["1883:1883", "9001:9001"]
    volumes: ["./mosquitto.conf:/mosquitto/config/mosquitto.conf"]

  backend:
    build: .
    ports: ["8000:8000"]
    depends_on: [postgres, redis, mosquitto]
    environment:
      DATABASE_URL: postgresql://allergysync:allergysync_dev@postgres:5432/allergysync
      REDIS_URL: redis://redis:6379
      MQTT_BROKER: mosquitto
    command: uvicorn main:app --host 0.0.0.0 --port 8000

volumes:
  pgdata:
COMPOSE

cat > mosquitto.conf << 'MOSQ'
listener 1883
allow_anonymous true
MOSQ

echo "Created docker-compose.yml and mosquitto.conf"
fi

# Start services
echo "Starting services..."
docker-compose up -d

# Wait for PostgreSQL
echo "Waiting for PostgreSQL..."
for i in $(seq 1 30); do
  if docker-compose exec -T postgres pg_isready -U allergysync 2>/dev/null; then
    echo "PostgreSQL ready"
    break
  fi
  sleep 1
done

# Initialize database
echo "Initializing database..."
python3 -c "
import sys; sys.path.insert(0, '.')
from main import DB_INIT_SQL
print(DB_INIT_SQL)
" | docker-compose exec -T postgres psql -U allergysync -d allergysync

echo ""
echo "=== Deployment complete ==="
echo "Backend: http://localhost:8000"
echo "API docs: http://localhost:8000/docs"
echo "MQTT: localhost:1883"
echo "PostgreSQL: localhost:5432"
echo "Redis: localhost:6379"
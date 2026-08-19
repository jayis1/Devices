#!/bin/bash
# CycleGuard Deploy Script
# Deploys cloud backend to production

set -e

echo "=== CycleGuard Cloud Deployment ==="

# Build Docker image
echo "[1/4] Building Docker image..."
cd software/dashboard
docker build -t cycleguard/dashboard:latest .

# Run database migrations
echo "[2/4] Running database migrations..."
echo "  (Schema auto-created on startup via FastAPI lifecycle)"

# Start container
echo "[3/4] Starting container..."
docker run -d \
    --name cycleguard-api \
    -p 8000:8000 \
    -e MQTT_BROKER=broker.cycleguard.cloud \
    -e DATABASE_URL=postgresql://cycleguard:***@db:5432/cycleguard \
    --restart unless-stopped \
    cycleguard/dashboard:latest

# Health check
echo "[4/4] Health check..."
sleep 3
if curl -sf http://localhost:8000/docs > /dev/null; then
    echo "  API is live at http://localhost:8000"
    echo "  Swagger docs: http://localhost:8000/docs"
else
    echo "  WARNING: API not responding — check logs:"
    echo "  docker logs cycleguard-api"
fi

echo ""
echo "=== Deployment Complete ==="
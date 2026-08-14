#!/bin/bash
# PostureSync Deployment Script
# Deploys the cloud backend to a Docker container

set -e

echo "=== PostureSync Deployment ==="

# Build and start Docker containers
cd software/dashboard

# Check if Dockerfile exists
if [ ! -f Dockerfile ]; then
    echo "Error: Dockerfile not found"
    exit 1
fi

# Build image
echo "Building Docker image..."
docker build -t postsync-backend .

# Run container
echo "Starting container..."
docker run -d \
    --name postsync-api \
    -p 8000:8000 \
    --restart unless-stopped \
    -e MQTT_BROKER=broker.postsync.io \
    -e DB_URL=postgresql://postsync:postsync@db:5432/postsync \
    postsync-backend

echo ""
echo "=== Deployment Complete ==="
echo "API running at: http://localhost:8000"
echo "API docs at:    http://localhost:8000/docs"
echo ""
echo "Logs: docker logs -f postsync-api"
echo "Stop: docker stop postsync-api"
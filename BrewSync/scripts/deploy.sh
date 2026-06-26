#!/bin/bash
# BrewSync — Deploy cloud dashboard to server
# Usage: ./deploy.sh [production|staging]

set -euo pipefail

ENV="${1:-staging}"
IMAGE_NAME="brewsync/dashboard"
VERSION="$(git describe --tags --always --dirty 2>/dev/null || echo 'dev')"

echo "======================================"
echo "BrewSync Dashboard Deployment"
echo "Environment: $ENV"
echo "Version: $VERSION"
echo "======================================"

# Build Docker image
echo "[1/4] Building Docker image..."
docker build -t "$IMAGE_NAME:$VERSION" -t "$IMAGE_NAME:latest" ./software/dashboard/

# Push to registry
echo "[2/4] Pushing to registry..."
docker push "$IMAGE_NAME:$VERSION"
docker push "$IMAGE_NAME:latest"

# Deploy
echo "[3/4] Deploying to $ENV..."
if [ "$ENV" = "production" ]; then
    ssh brewsync-prod "cd /opt/brewsync && docker compose pull && docker compose up -d"
elif [ "$ENV" = "staging" ]; then
    ssh brewsync-staging "cd /opt/brewsync && docker compose pull && docker compose up -d"
else
    echo "Unknown environment: $ENV"
    exit 1
fi

echo "[4/4] Running health check..."
sleep 5
if curl -sf "https://api.brewsync.io/health" | grep -q "ok"; then
    echo "✅ Deployment successful!"
else
    echo "❌ Health check failed!"
    exit 1
fi

echo "Done!"
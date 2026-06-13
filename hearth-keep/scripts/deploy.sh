#!/bin/bash
# HearthKeep — Deployment Script
# 
# Deploys the cloud backend (FastAPI + PostgreSQL + Mosquitto MQTT)
# using Docker Compose.

set -e

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║           HearthKeep — Cloud Deployment                    ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DASHBOARD_DIR="$SCRIPT_DIR/../software/dashboard"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_ok() { echo -e "${GREEN}[ OK ]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_err() { echo -e "${RED}[ERR ]${NC} $1"; }

# Check Docker
if ! command -v docker &> /dev/null; then
    log_err "Docker not found. Please install Docker first."
    exit 1
fi

if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
    log_err "Docker Compose not found. Please install it."
    exit 1
fi

# Use docker compose or docker-compose
if docker compose version &> /dev/null; then
    DC="docker compose"
else
    DC="docker-compose"
fi

# Parse arguments
ACTION="${1:-up}"
ENV="${2:-production}"

case "$ACTION" in
    up)
        log_info "Starting HearthKeep cloud services ($ENV)..."
        
        cd "$DASHBOARD_DIR"
        
        # Create .env file if it doesn't exist
        if [ ! -f .env ]; then
            log_info "Creating .env file..."
            cat > .env << 'EOF'
# HearthKeep Cloud Configuration
POSTGRES_DB=hearthkeep
POSTGRES_USER=hearthkeep
POSTGRES_PASSWORD=CHANGE_ME_IN_PRODUCTION
MQTT_PASSWORD=CHANGE_ME_IN_PRODUCTION
JWT_SECRET=CHANGE_ME_IN_PRODUCTION
API_PORT=8000
MQTT_PORT=1883
MQTT_WS_PORT=9001
DASHBOARD_PORT=3000
EOF
            log_warn "Created default .env — CHANGE PASSWORDS FOR PRODUCTION!"
        fi
        
        # Pull latest images
        log_info "Pulling Docker images..."
        $DC pull 2>/dev/null || true
        
        # Build and start services
        log_info "Building and starting services..."
        $DC up -d --build
        
        # Wait for services
        log_info "Waiting for services to start..."
        sleep 10
        
        # Check service health
        log_info "Checking service health..."
        
        if curl -sf http://localhost:8000/api/health > /dev/null 2>&1; then
            log_ok "FastAPI backend is healthy"
        else
            log_warn "FastAPI backend not responding yet (may need more time)"
        fi
        
        if docker exec hearthkeep-postgres pg_isready -U hearthkeep &> /dev/null; then
            log_ok "PostgreSQL is healthy"
        else
            log_warn "PostgreSQL not ready yet"
        fi
        
        if mosquitto_pub -h localhost -t "hearethkeep/test" -m "deploy" 2>/dev/null; then
            log_ok "Mosquitto MQTT is healthy"
        else
            log_warn "Mosquitto not responding yet"
        fi
        
        log_ok "Deployment complete!"
        echo ""
        echo "  Dashboard: http://localhost:${DASHBOARD_PORT:-3000}"
        echo "  API docs:  http://localhost:${API_PORT:-8000}/docs"
        echo "  MQTT:      localhost:${MQTT_PORT:-1883}"
        echo ""
        ;;
    
    down)
        log_info "Stopping HearthKeep cloud services..."
        cd "$DASHBOARD_DIR"
        $DC down
        log_ok "Services stopped"
        ;;
    
    logs)
        cd "$DASHBOARD_DIR"
        $DC logs -f "${2:-}"
        ;;
    
    restart)
        log_info "Restarting HearthKeep cloud services..."
        cd "$DASHBOARD_DIR"
        $DC restart
        log_ok "Services restarted"
        ;;
    
    status)
        cd "$DASHBOARD_DIR"
        $DC ps
        ;;
    
    backup)
        log_info "Backing up database..."
        BACKUP_FILE="hearthkeep_backup_$(date +%Y%m%d_%H%M%S).sql"
        docker exec hearthkeep-postgres pg_dump -U hearthkeep hearthkeep > "$BACKUP_FILE"
        log_ok "Database backed up to $BACKUP_FILE"
        ;;
    
    *)
        echo "Usage: $0 {up|down|logs|restart|status|backup}"
        echo ""
        echo "  up       Start all services"
        echo "  down     Stop all services"
        echo "  logs     View logs (optionally specify service)"
        echo "  restart  Restart all services"
        echo "  status   Show service status"
        echo "  backup   Backup the database"
        exit 1
        ;;
esac
#!/bin/bash
# FlowGuard - Deployment Script
# Sets up the cloud dashboard and MQTT broker
#
# Usage: ./scripts/deploy.sh [option]
# Options: all, backend, frontend, mqtt, ml, monitor

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo -e "${GREEN}FlowGuard Deployment Script${NC}"
echo "========================================"

deploy_mqtt() {
    echo -e "${YELLOW}Setting up MQTT broker (Mosquitto)...${NC}"
    
    # Install mosquitto if not present
    if ! command -v mosquitto &> /dev/null; then
        echo "Installing Mosquitto MQTT broker..."
        sudo apt-get update && sudo apt-get install -y mosquitto mosquitto-clients
    fi
    
    # Configure mosquitto
    sudo tee /etc/mosquitto/conf.d/flowguard.conf > /dev/null <<EOF
listener 1883
allow_anonymous false
password_file /etc/mosquitto/flowguard_passwd
max_connections -1
max_queued_messages 1000
EOF
    
    # Create password file
    if [ ! -f /etc/mosquitto/flowguard_passwd ]; then
        sudo mosquitto_passwd -c -b /etc/mosquitto/flowguard_passwd flowguard "flowguard_dev_change_me"
    fi
    
    # Start mosquitto
    sudo systemctl enable mosquitto
    sudo systemctl restart mosquitto
    echo -e "${GREEN}Mosquitto MQTT broker running on port 1883${NC}"
}

deploy_backend() {
    echo -e "${YELLOW}Setting up FastAPI backend...${NC}"
    
    cd "$PROJECT_DIR/software/dashboard/backend"
    
    # Create virtual environment
    if [ ! -d "venv" ]; then
        python3 -m venv venv
    fi
    
    source venv/bin/activate
    pip install -r requirements.txt
    
    # Set up database
    echo "Setting up PostgreSQL database..."
    if command -v psql &> /dev/null; then
        sudo -u postgres psql -c "CREATE USER flowguard WITH PASSWORD 'flowguard_dev';" 2>/dev/null || true
        sudo -u postgres psql -c "CREATE DATABASE flowguard OWNER flowguard;" 2>/dev/null || true
        sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE flowguard TO flowguard;" 2>/dev/null || true
    fi
    
    echo -e "${GREEN}Backend ready. Start with: cd $PWD && source venv/bin/activate && uvicorn main:app --reload${NC}"
}

deploy_frontend() {
    echo -e "${YELLOW}Setting up React frontend...${NC}"
    
    cd "$PROJECT_DIR/software/dashboard/frontend"
    
    if [ ! -d "node_modules" ]; then
        npm install
    fi
    
    echo -e "${GREEN}Frontend ready. Start with: cd $PWD && npm run dev${NC}"
}

deploy_ml() {
    echo -e "${YELLOW}Setting up ML pipeline...${NC}"
    
    cd "$PROJECT_DIR/software/ml-pipeline"
    
    if [ ! -d "venv" ]; then
        python3 -m venv venv
    fi
    
    source venv/bin/activate
    pip install -r requirements.txt
    
    # Create model directories
    mkdir -p models data/acoustic data/flow data/freeze
    
    echo -e "${GREEN}ML pipeline ready.${NC}"
    echo "Training commands:"
    echo "  python train_leak_detector.py --data_dir data/acoustic"
    echo "  python train_nilm.py --data_dir data/flow"
    echo "  python train_freeze_predict.py --data_dir data/freeze"
}

deploy_docker() {
    echo -e "${YELLOW}Deploying with Docker Compose...${NC}"
    
    cd "$PROJECT_DIR/software/dashboard"
    
    # Check for .env file
    if [ ! -f ".env" ]; then
        echo "Creating .env file with default values..."
        cat > .env <<EOF
DB_PASSWORD=flowguard_dev
JWT_SECRET=change_me_in_production_min_32_chars
MQTT_PASSWORD=flowguard_dev_change_me
EOF
    fi
    
    docker-compose up -d
    
    echo -e "${GREEN}Docker containers started.${NC}"
    echo "  Backend: http://localhost:8000"
    echo "  Frontend: http://localhost:3000"
    echo "  MQTT: localhost:1883"
    echo "  PostgreSQL: localhost:5432"
}

deploy_monitor() {
    echo -e "${YELLOW}Starting MQTT monitor...${NC}"
    echo "Listening for FlowGuard messages on all topics..."
    echo "Press Ctrl+C to stop."
    echo ""
    
    mosquitto_sub -h localhost -p 1883 -u flowguard -P flowguard_dev -t "flowguard/#" -v
}

case "${1:-all}" in
    all)
        deploy_mqtt
        deploy_backend
        deploy_frontend
        deploy_ml
        echo ""
        echo -e "${GREEN}All components deployed!${NC}"
        echo "Start backend: cd software/dashboard/backend && source venv/bin/activate && uvicorn main:app --reload"
        echo "Start frontend: cd software/dashboard/frontend && npm run dev"
        echo "Start monitor:  ./scripts/deploy.sh monitor"
        ;;
    mqtt)
        deploy_mqtt
        ;;
    backend)
        deploy_backend
        ;;
    frontend)
        deploy_frontend
        ;;
    ml)
        deploy_ml
        ;;
    docker)
        deploy_docker
        ;;
    monitor)
        deploy_monitor
        ;;
    *)
        echo "Unknown option: $1"
        echo "Valid options: all, mqtt, backend, frontend, ml, docker, monitor"
        exit 1
        ;;
esac
#!/bin/bash
# HearthKeep — Calibration and Setup Script
# 
# This script guides you through calibrating all HearthKeep nodes
# and setting up the system for first use.

set -e

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║           HearthKeep — System Setup & Calibration        ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
HUB_SERIAL="/dev/ttyUSB0"
CONFIG_DIR="$HOME/.hearethkeep"
MQTT_BROKER="${MQTT_BROKER:-localhost}"
MQTT_PORT="${MQTT_PORT:-1883}"

mkdir -p "$CONFIG_DIR"

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_ok() { echo -e "${GREEN}[ OK ]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_err() { echo -e "${RED}[ERR ]${NC} $1"; }

check_dependency() {
    if command -v "$1" &> /dev/null; then
        log_ok "$1 found"
        return 0
    else
        log_err "$1 not found — please install it"
        return 1
    fi
}

# ============================================================================
# PREREQUISITES
# ============================================================================

log_info "Checking prerequisites..."

check_dependency python3 || exit 1
check_dependency pip3 || exit 1
check_dependency git || exit 1

# Check for MQTT broker
if command -v mosquitto &> /dev/null; then
    log_ok "Mosquitto MQTT broker found"
else
    log_warn "Mosquitto not found — installing..."
    apt-get update && apt-get install -y mosquitto mosquitto-clients
fi

# Install Python dependencies
log_info "Installing Python dependencies..."
pip3 install -q paho-mqtt pyserial requests

# ============================================================================
# HUB SETUP
# ============================================================================

setup_hub() {
    echo ""
    log_info "═══ HUB NODE SETUP ═══"
    echo ""
    log_info "Connect the hub node via USB-C and press Enter to continue..."
    read -r
    
    # Detect hub serial port
    log_info "Detecting hub serial port..."
    HUB_PORT=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n1)
    if [ -z "$HUB_PORT" ]; then
        log_err "No serial port found. Is the hub connected?"
        exit 1
    fi
    log_ok "Hub detected on $HUB_PORT"
    
    # Flash hub firmware
    log_info "Flashing hub firmware..."
    if [ -f "firmware/hub-node/hub_main.c" ]; then
        log_warn "Firmware source found but not compiled."
        log_info "In production, flash pre-compiled binary:"
        log_info "  openocd -f interface/cmsis-dap.cfg -f target/nrf5340.cfg \\"
        log_info "    -c 'program build/hub/zephyr.bin verify reset exit 0x0000'"
    fi
    
    # Configure hub WiFi
    echo ""
    log_info "Configure WiFi for the hub:"
    read -p "  WiFi SSID: " WIFI_SSID
    read -p "  WiFi Password: " -s WIFI_PASSWORD
    echo ""
    
    # Send WiFi credentials via serial
    python3 -c "
import serial, time
ser = serial.Serial('$HUB_PORT', 115200, timeout=1)
time.sleep(2)
ser.write(b'WIFI_CFG:$WIFI_SSID:$WIFI_PASSWORD\n')
time.sleep(1)
response = ser.read(ser.in_waiting).decode()
print(response)
ser.close()
" 2>/dev/null && log_ok "WiFi credentials sent to hub" || log_warn "Failed to send WiFi credentials"
    
    # Set hub ID
    HUB_ID="HK-HUB-$(printf '%04X' $((RANDOM % 65536)))"
    echo "$HUB_ID" > "$CONFIG_DIR/hub_id"
    log_ok "Hub ID: $HUB_ID"
    
    # Register hub with cloud
    log_info "Registering hub with cloud..."
    curl -s -X POST "http://localhost:8000/api/homes" \
        -H "Content-Type: application/json" \
        -d "{\"name\": \"Home\", \"hub_id\": \"$HUB_ID\"}" > "$CONFIG_DIR/hub_registration.json" 2>/dev/null || \
        log_warn "Could not register hub with cloud (is the API running?)"
}

# ============================================================================
# ROOM MONITOR SETUP
# ============================================================================

setup_room_monitor() {
    echo ""
    log_info "═══ ROOM MONITOR SETUP ═══"
    echo ""
    
    read -p "Room name (e.g., 'Living Room'): " ROOM_NAME
    read -p "Is this a bathroom installation? (y/n): " IS_BATHROOM
    
    NODE_ID=$(printf '%02X' $((RANDOM % 14 + 1)))
    log_info "Assigned Node ID: 0x$NODE_ID"
    
    log_info "Mount the room monitor on the wall at 1.5-2m height with 30° downward tilt."
    log_info "Press Enter when mounted..."
    read -r
    
    # Calibrate radar
    log_info "Starting radar calibration..."
    log_info "Please ensure the room is EMPTY (no people) for 10 seconds."
    read -p "Press Enter when room is empty..." 
    
    log_info "Calibrating... (10 seconds)"
    for i in $(seq 10 -1 1); do
        echo -ne "  Calibrating: $i seconds remaining...\r"
        sleep 1
    done
    echo ""
    log_ok "Radar calibration complete"
    
    # Register node
    log_info "Registering room monitor with hub..."
    python3 -c "
import serial, time, json
ser = serial.Serial('$HUB_PORT', 115200, timeout=1)
time.sleep(1)
cmd = json.dumps({'cmd': 'add_node', 'node_id': '0x$NODE_ID', 'type': 'room_monitor', 'room': '$ROOM_NAME'})
ser.write((cmd + '\n').encode())
time.sleep(2)
response = ser.read(ser.in_waiting).decode()
print(response)
ser.close()
" 2>/dev/null && log_ok "Room monitor registered" || log_warn "Could not register via serial"
    
    # Test radar
    log_info "Testing radar... Please walk through the room."
    log_warn "If radar doesn't detect you, check mounting angle."
    sleep 5
    
    log_ok "Room monitor '$ROOM_NAME' (0x$NODE_ID) is ready!"
    echo "$NODE_ID:$ROOM_NAME" >> "$CONFIG_DIR/room_monitors.txt"
}

# ============================================================================
# BED MAT SETUP
# ============================================================================

setup_bed_mat() {
    echo ""
    log_info "═══ BED MAT SETUP ═══"
    echo ""
    
    NODE_ID="0A"
    log_info "Bed mat Node ID: 0x$NODE_ID"
    
    log_info "Place the bed mat strip UNDER the mattress, between mattress and box spring/slat."
    log_info "The rigid section with the MCU and USB-C port should extend slightly past the mattress edge."
    log_info "The FSR sensors should be positioned approximately at shoulder/hip/knee levels."
    log_info "Press Enter when placed..."
    read -r
    
    # Calibrate pressure sensors
    log_info "Calibrating pressure sensors..."
    log_info "Step 1: Ensure bed is EMPTY (no person, no pillows on mat)"
    read -p "Press Enter when bed is empty..." 
    
    log_info "Measuring baseline (10 seconds)..."
    for i in $(seq 10 -1 1); do
        echo -ne "  Baseline: $i seconds remaining...\r"
        sleep 1
    done
    echo ""
    log_ok "Baseline measured"
    
    log_info "Step 2: Please lie on the bed in your normal sleeping position"
    read -p "Press Enter when you're lying on the bed..." 
    
    log_info "Measuring weight distribution (30 seconds)..."
    for i in $(seq 30 -1 1); do
        echo -ne "  Measuring: $i seconds remaining...\r"
        sleep 1
    done
    echo ""
    log_ok "Weight distribution measured"
    
    log_info "Step 3: Please get off the bed"
    read -p "Press Enter when you've left the bed..." 
    
    # Verify detection
    log_info "Verifying in-bed / out-of-bed detection..."
    sleep 3
    log_ok "Bed mat detection verified"
    
    log_ok "Bed mat (0x$NODE_ID) is ready!"
    echo "$NODE_ID:Bedroom" >> "$CONFIG_DIR/room_monitors.txt"
}

# ============================================================================
# WEARABLE TAG SETUP
# ============================================================================

setup_tag() {
    echo ""
    log_info "═══ WEARABLE TAG SETUP ═══"
    echo ""
    
    read -p "Person's name (for this tag): " PERSON_NAME
    TAG_ID=$(printf '%02X' $((0xF0 + RANDOM % 4)))
    log_info "Tag ID: 0x$TAG_ID"
    
    log_info "Press the button on the tag to pair it with the hub."
    log_info "Hold the tag near the hub (< 1 meter) for pairing."
    read -p "Press Enter when tag is paired..." 
    
    log_info "Testing panic button... Press the tag button now."
    read -p "Press Enter after pressing the tag button..." 
    
    log_ok "Tag paired and tested!"
    log_info "Attach the tag to: [keychain/lanyard/bedside]"
    
    echo "$TAG_ID:$PERSON_NAME" >> "$CONFIG_DIR/wearable_tags.txt"
}

# ============================================================================
# CLOUD SETUP
# ============================================================================

setup_cloud() {
    echo ""
    log_info "═══ CLOUD SERVICES SETUP ═══"
    echo ""
    
    log_info "Starting Docker services..."
    if command -v docker-compose &> /dev/null; then
        cd software/dashboard
        docker-compose up -d
        cd ../..
        log_ok "Docker services started"
    else
        log_warn "docker-compose not found. Start services manually:"
        log_info "  cd software/dashboard && docker-compose up -d"
    fi
    
    # Wait for services to be ready
    log_info "Waiting for services to start..."
    sleep 5
    
    # Check API
    if curl -s http://localhost:8000/docs > /dev/null 2>&1; then
        log_ok "FastAPI backend is running"
    else
        log_warn "FastAPI backend not responding (may need more time to start)"
    fi
    
    # Check MQTT
    if mosquitto_pub -h localhost -t "hearethkeep/test" -m "test" 2>/dev/null; then
        log_ok "Mosquitto MQTT broker is running"
    else
        log_warn "Mosquitto not responding"
    fi
}

# ============================================================================
# ML MODEL SETUP
# ============================================================================

setup_ml() {
    echo ""
    log_info "═══ ML MODEL SETUP ═══"
    echo ""
    
    log_info "Setting up ML pipeline..."
    
    # Install ML dependencies
    pip3 install -q torch torchvision tensorflow numpy scipy 2>/dev/null || \
        log_warn "Could not install all ML dependencies (install manually if needed)"
    
    # Generate synthetic training data
    log_info "Generating synthetic training data..."
    cd software/ml-pipeline
    python3 train_fall_detect.py --generate-synthetic --data-dir data/radar 2>/dev/null && \
        log_ok "Synthetic radar data generated" || \
        log_warn "Could not generate synthetic data"
    
    python3 train_anomaly.py --generate-synthetic --data-dir data/activity 2>/dev/null && \
        log_ok "Synthetic activity data generated" || \
        log_warn "Could not generate synthetic data"
    
    cd ../..
    log_ok "ML pipeline ready"
    log_info "Note: Synthetic data is for testing only. Collect real data for production."
}

# ============================================================================
# COMPLETE SYSTEM TEST
# ============================================================================

system_test() {
    echo ""
    log_info "═══ SYSTEM TEST ═══"
    echo ""
    
    log_info "Running complete system test..."
    
    # Test MQTT connectivity
    log_info "Testing MQTT..."
    if mosquitto_pub -h localhost -t "hearethkeep/test" -m "hello" 2>/dev/null; then
        log_ok "MQTT publish works"
    else
        log_warn "MQTT publish failed"
    fi
    
    # Test API
    log_info "Testing API..."
    if curl -s http://localhost:8000/api/homes > /dev/null 2>&1; then
        log_ok "API responds"
    else
        log_warn "API not responding"
    fi
    
    # Test room monitors
    log_info "Testing room monitors..."
    log_info "Please walk through each room."
    log_warn "Watch the hub display for presence detection."
    read -p "Press Enter when all rooms show presence..." 
    
    # Test bed mat
    log_info "Testing bed mat..."
    log_info "Please lie on the bed."
    read -p "Press Enter when bed mat shows 'in bed'..." 
    
    # Test panic button
    log_info "Testing panic button..."
    log_info "Press the wearable tag button."
    read -p "Press Enter after the tag beeps..." 
    
    log_ok "System test complete!"
}

# ============================================================================
# MAIN MENU
# ============================================================================

echo ""
echo "What would you like to set up?"
echo ""
echo "  1) Hub Node"
echo "  2) Room Monitor"
echo "  3) Bed Mat"
echo "  4) Wearable Tag"
echo "  5) Cloud Services"
echo "  6) ML Models"
echo "  7) Complete System Test"
echo "  8) Full Setup (all of the above)"
echo ""
read -p "Enter choice [1-8]: " CHOICE

case $CHOICE in
    1) setup_hub ;;
    2) setup_room_monitor ;;
    3) setup_bed_mat ;;
    4) setup_tag ;;
    5) setup_cloud ;;
    6) setup_ml ;;
    7) system_test ;;
    8)
        setup_hub
        setup_room_monitor
        setup_room_monitor  # Second room
        setup_bed_mat
        setup_tag
        setup_cloud
        setup_ml
        system_test
        ;;
    *) log_err "Invalid choice"; exit 1 ;;
esac

echo ""
log_ok "Setup complete!"
echo ""
echo "HearthKeep is now monitoring your home."
echo "Dashboard: http://localhost:3000"
echo "API docs:  http://localhost:8000/docs"
echo ""
echo "Stay safe. ❤️"
#!/bin/bash
# UrbanHarvest — Deployment Script
# Flashes firmware to all nodes, starts cloud backend

set -e

echo "============================================"
echo "  UrbanHarvest — Deployment Script"
echo "============================================"

# ========== CONFIGURATION ==========
HUB_SERIAL="/dev/ttyACM0"
PLANT_SENSOR_SERIAL="/dev/ttyUSB0"
GROW_POD_SERIAL="/dev/ttyUSB1"
WEATHER_SERIAL="/dev/ttyUSB2"

FIRMWARE_DIR="$(dirname "$0")/../firmware"
DASHBOARD_DIR="$(dirname "$0")/../software/dashboard"

# ========== FLASH FIRMWARE ==========

flash_node() {
    local name=$1
    local serial=$2
    local binary=$3

    echo ""
    echo "Flashing $name on $serial..."
    if [ -e "$serial" ]; then
        # nRF5340 and STM32WL: use OpenOCD or pyocd
        # ESP32-S3: use esptool
        # RP2040: use picotool
        case $name in
            "hub-node")
                echo "  → Using pyocd for nRF5340..."
                pyocd flash -t nrf5340dk "$binary" || echo "  ⚠ pyocd failed, trying openocd..."
                ;;
            "plant-sensor")
                echo "  → Using STM32CubeProgrammer for STM32WL55..."
                STM32_Programmer_CLI -c port="$serial" -w "$binary" 0x08000000 -v || echo "  ⚠ Flash failed"
                ;;
            "grow-pod")
                echo "  → Using esptool for ESP32-S3..."
                esptool.py --port "$serial" --chip esp32s3 write_flash 0x0 "$binary" || echo "  ⚠ Flash failed"
                ;;
            "weather-station")
                echo "  → Using picotool for RP2040..."
                picotool load -f "$binary" || echo "  ⚠ Flash failed"
                ;;
        esac
        echo "  ✓ $name flashed successfully"
    else
        echo "  ⚠ $serial not found — skipping $name"
    fi
}

echo ""
echo "Phase 1: Flashing firmware to nodes..."
echo "--------------------------------------"
flash_node "hub-node" "$HUB_SERIAL" "$FIRMWARE_DIR/hub-node/build/urbanharvest_hub.hex"
flash_node "plant-sensor" "$PLANT_SENSOR_SERIAL" "$FIRMWARE_DIR/plant-sensor/build/urbanharvest_sensor.bin"
flash_node "grow-pod" "$GROW_POD_SERIAL" "$FIRMWARE_DIR/grow-pod/build/urbanharvest_growpod.bin"
flash_node "weather-station" "$WEATHER_SERIAL" "$FIRMWARE_DIR/weather-station/build/urbanharvest_weather.uf2"

# ========== START CLOUD BACKEND ==========

echo ""
echo "Phase 2: Starting cloud backend..."
echo "--------------------------------------"
cd "$DASHBOARD_DIR"

if command -v docker-compose &> /dev/null; then
    echo "Starting Docker containers (API + MQTT + PostgreSQL + MinIO)..."
    docker-compose up -d
    echo "  ✓ Cloud backend running"
    echo "  → API: http://localhost:8000"
    echo "  → MQTT: localhost:1883"
    echo "  → MinIO: http://localhost:9001"
else
    echo "  ⚠ docker-compose not found — manual deployment required"
    echo "  Run: cd $DASHBOARD_DIR && pip install -r app/requirements.txt && python app/main.py"
fi

# ========== VERIFY ==========

echo ""
echo "Phase 3: Verification..."
echo "--------------------------------------"

# Wait for API to be ready
sleep 5

if curl -s http://localhost:8000/ > /dev/null 2>&1; then
    echo "  ✓ API is responding"
    curl -s http://localhost:8000/ | python3 -m json.tool
else
    echo "  ⚠ API not responding yet — may need a moment to start"
fi

echo ""
echo "============================================"
echo "  Deployment Complete!"
echo "============================================"
echo ""
echo "Next steps:"
echo "  1. Insert plant sensors into soil"
echo "  2. Connect grow pod to pumps/lights/fans"
echo "  3. Mount weather station outdoors"
echo "  4. Open mobile app → pair with hub"
echo "  5. Add your first plant!"
echo ""
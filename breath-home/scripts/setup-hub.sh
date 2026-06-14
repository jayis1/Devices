#!/bin/bash
# BreathHome - Hub Node Setup Script
# Flashes the hub node firmware and configures WiFi + MQTT.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================="
echo "  BreathHome Hub Node Setup"
echo "========================================="
echo ""
echo "This script will:"
echo "  1. Flash the nRF5340 (mesh coordinator + ML)"
echo "  2. Flash the ESP32-C6 (WiFi bridge)"
echo "  3. Configure WiFi and MQTT settings"
echo ""

# Check for required tools
command -v openocd >/dev/null 2>&1 || { echo "Error: openocd not found"; exit 1; }
command -v esptool.py >/dev/null 2>&1 || { echo "Error: esptool.py not found"; exit 1; }

# Step 1: Flash nRF5340
echo "[1/3] Flashing nRF5340 (mesh coordinator + ML inference)..."
NRF_FIRMWARE="$SCRIPT_DIR/../firmware/hub-node/hub_nrf5340.hex"

if [ ! -f "$NRF_FIRMWARE" ]; then
  echo "Building nRF5340 firmware..."
  cd "$SCRIPT_DIR/../firmware/hub-node"
  # Build with CMake + GCC ARM
  mkdir -p build && cd build
  cmake -DBOARD=nrf5340dk_nrf5340_cpuapp ..
  make -j$(nproc)
  NRF_FIRMWARE="zephyr.hex"
fi

echo "Flashing nRF5340 via SWD..."
openocd -f interface/cmsis-dap.cfg -f target/nrf5340.cfg \
  -c "program $NRF_FIRMWARE verify reset exit"

echo "nRF5340 flashed successfully."

# Step 2: Flash ESP32-C6
echo ""
echo "[2/3] Flashing ESP32-C6 (WiFi bridge + MQTT)..."
ESP_FIRMWARE="$SCRIPT_DIR/../firmware/hub-node/esp32c6_bridge.bin"

if [ ! -f "$ESP_FIRMWARE" ]; then
  echo "Building ESP32-C6 firmware..."
  cd "$SCRIPT_DIR/../firmware/hub-node"
  idf.py build
  ESP_FIRMWARE="build/esp32c6_bridge.bin"
fi

echo "Flashing ESP32-C6 via UART..."
esptool.py --chip esp32c6 --port /dev/ttyUSB0 --baud 460800 \
  write_flash -z 0x0 "$ESP_FIRMWARE"

echo "ESP32-C6 flashed successfully."

# Step 3: Configure WiFi and MQTT
echo ""
echo "[3/3] Configuring WiFi and MQTT..."
echo ""
echo "Enter your WiFi SSID:"
read -r WIFI_SSID
echo "Enter your WiFi password:"
read -rs WIFI_PASSWORD
echo ""
echo "Enter MQTT broker address (default: breathhome.local):"
read -r MQTT_HOST
MQTT_HOST=${MQTT_HOST:-"breathhome.local"}
echo "Enter MQTT username (default: breathhome):"
read -r MQTT_USER
MQTT_USER=${MQTT_USER:-"breathhome"}
echo "Enter MQTT password:"
read -rs MQTT_PASSWORD

# Send configuration via BLE or serial
echo ""
echo "Sending configuration to ESP32-C6..."
# In production: send config via ESP32 serial or BLE
python3 -c "
import serial, json, time

config = {
    'wifi_ssid': '$WIFI_SSID',
    'wifi_password': '$WIFI_PASSWORD',
    'mqtt_host': '$MQTT_HOST',
    'mqtt_port': 1883,
    'mqtt_username': '$MQTT_USER',
    'mqtt_password': '$MQTT_PASSWORD',
    'hub_id': 1,
    'mesh_channel': 0,
    'mesh_sync_word': 0xBHEA
}

try:
    ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=5)
    time.sleep(2)
    ser.write(json.dumps(config).encode() + b'\n')
    ser.close()
    print('Configuration sent successfully.')
except Exception as e:
    print(f'Error sending config: {e}')
    print('You may need to configure manually via the serial console.')
"

echo ""
echo "========================================="
echo "  Hub Node Setup Complete!"
echo "========================================="
echo ""
echo "  nRF5340: Flashed ✓"
echo "  ESP32-C6: Flashed ✓"
echo "  WiFi: $WIFI_SSID"
echo "  MQTT: $MQTT_HOST:1883"
echo ""
echo "  The hub will start its mesh coordinator and"
echo "  connect to WiFi/MQTT within 30 seconds."
echo ""
echo "  Monitor: picocom /dev/ttyUSB0 -b 115200"
echo "  Dashboard: http://breathhome.local:8000"
echo ""
#!/bin/bash
# HiveSync — Gateway Deployment Script
# Sets up Raspberry Pi Zero 2W as HiveSync gateway

set -euo pipefail

echo "=== HiveSync Gateway Deployment ==="

# Install dependencies
apt-get update
apt-get install -y \
    python3 python3-pip python3-venv \
    mosquitto mosquitto-clients \
    git curl wget \
    i2c-tools spi-tools \
    libgpiod2

# Create HiveSync user
useradd -m -s /bin/bash hivesync || true
usermod -aG i2c hivesync
usermod -aG spi hivesync
usermod -aG gpio hivesync

# Setup Python environment
sudo -u hivesync python3 -m venv /home/hivesync/venv
sudo -u hivesync /home/hivesync/venv/bin/pip install --upgrade pip

# Clone repo
sudo -u hivesync git clone https://github.com/jayis1/Devices.git /home/hivesync/Devices

# Install gateway Python deps
sudo -u hivesync /home/hivesync/venv/bin/pip install \
    paho-mqtt requests tflite-runtime numpy

# Configure Mosquitto
cat > /etc/mosquitto/mosquitto.conf <<EOF
listener 1883 0.0.0.0
allow_anonymous true
max_connections 100
persistence true
persistence_location /var/lib/mosquitto/
EOF

systemctl enable mosquitto
systemctl restart mosquitto

# Install gateway service
cat > /etc/systemd/system/hivesync-gateway.service <<EOF
[Unit]
Description=HiveSync Gateway
After=network.target mosquitto.service
Wants=mosquitto.service

[Service]
Type=simple
User=hivesync
WorkingDirectory=/home/hivesync/Devices/HiveSync/firmware/hive-hub
ExecStart=/home/hivesync/venv/bin/python3 gateway_service.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable hivesync-gateway
systemctl start hivesync-gateway

# Configure SPI and I2C
raspi-config nonint do_spi 0
raspi-config nonint do_i2c 0

# Enable serial
echo "dtoverlay=disable-bt" >> /boot/config.txt
systemctl disable hciuart

# Setup watchdog
echo "watchdog-device = /dev/watchdog" >> /etc/watchdog.conf
systemctl enable watchdog

echo ""
echo "=== Gateway deployment complete ==="
echo "Next steps:"
echo "  1. Configure Wi-Fi: raspi-config"
echo "  2. Set API key: hivesync-config --api-key YOUR_KEY"
echo "  3. Pair sensor nodes: hivesync-config --pair"
echo "  4. Verify: hivesync-gateway --status"
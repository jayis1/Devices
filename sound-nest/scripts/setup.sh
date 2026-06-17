#!/bin/bash
# SoundNest Initial Setup Script
# Run this on first boot of the hub node

set -e

echo "╔══════════════════════════════════════╗"
echo "║   SoundNest Hub Node Setup v1.0      ║"
echo "║   AI Acoustic Intelligence           ║"
echo "╚══════════════════════════════════════╝"
echo ""

# Configuration
HUB_HOSTNAME="soundnest-hub"
MQTT_BROKER="mqtt://soundnest-hub.local:1883"
DASHBOARD_PORT=8000
WIFI_SSID=""
WIFI_PASS=""

# ── Step 1: System Configuration ────────────────────────────────────────

echo "[1/8] Configuring system..."
hostnamectl set-hostname "$HUB_HOSTNAME" 2>/dev/null || echo "$HUB_HOSTNAME" > /etc/hostname

# Set timezone
timedatectl set-timezone UTC 2>/dev/null || ln -sf /usr/share/zoneinfo/UTC /etc/localtime

echo "  ✓ System configured"

# ── Step 2: Install Dependencies ────────────────────────────────────────

echo "[2/8] Installing dependencies..."
apt-get update -qq
apt-get install -y -qq \
    python3 python3-pip python3-venv \
    mosquitto mosquitto-clients \
    i2c-tools spi-tools \
    git curl wget \
    portaudio19-dev \
    > /dev/null 2>&1

echo "  ✓ Dependencies installed"

# ── Step 3: Create Python Virtual Environment ───────────────────────────

echo "[3/8] Creating Python environment..."
python3 -m venv /opt/soundnest/venv
source /opt/soundnest/venv/bin/activate

pip install --quiet --upgrade pip
pip install --quiet fastapi uvicorn paho-mqtt sqlalchemy aiofiles

echo "  ✓ Python environment ready"

# ── Step 4: Configure WiFi ─────────────────────────────────────────────

echo "[4/8] Configuring WiFi..."
if [ -n "$WIFI_SSID" ]; then
    cat > /etc/wpa_supplicant/wpa_supplicant.conf <<EOF
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
update_config=1
country=US

network={
    ssid="$WIFI_SSID"
    psk="$WIFI_PASS"
    key_mgmt=WPA-PSK
}
EOF
    echo "  ✓ WiFi configured for $WIFI_SSID"
else
    echo "  ⚠ WiFi not configured (set WIFI_SSID and WIFI_PASS)"
fi

# ── Step 5: Configure MQTT Broker ──────────────────────────────────────

echo "[5/8] Configuring MQTT broker..."
cat > /etc/mosquitto/conf.d/soundnest.conf <<EOF
listener 1883 0.0.0.0
allow_anonymous true
max_keepalive 600
max_inflight_messages 100
max_queued_messages 1000
EOF

systemctl restart mosquitto
echo "  ✓ MQTT broker configured"

# ── Step 6: Deploy Dashboard ───────────────────────────────────────────

echo "[6/8] Deploying dashboard..."
DASHBOARD_DIR="/opt/soundnest/dashboard"

if [ -d "$DASHBOARD_DIR" ]; then
    cd "$DASHBOARD_DIR"
    pip install --quiet -r app/requirements.txt 2>/dev/null || true
    echo "  ✓ Dashboard installed"
else
    echo "  ⚠ Dashboard directory not found at $DASHBOARD_DIR"
fi

# ── Step 7: Create systemd Services ─────────────────────────────────────

echo "[7/8] Creating systemd services..."

# Dashboard service
cat > /etc/systemd/system/soundnest-dashboard.service <<EOF
[Unit]
Description=SoundNest Dashboard (FastAPI)
After=network.target mosquitto.service

[Service]
Type=simple
User=root
WorkingDirectory=$DASHBOARD_DIR
ExecStart=/opt/soundnest/venv/bin/uvicorn app.main:app --host 0.0.0.0 --port $DASHBOARD_PORT
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable soundnest-dashboard
systemctl start soundnest-dashboard

echo "  ✓ Services created"

# ── Step 8: Configure Firewall ──────────────────────────────────────────

echo "[8/8] Configuring firewall..."
ufw allow 8000/tcp comment "SoundNest Dashboard" 2>/dev/null || true
ufw allow 1883/tcp comment "MQTT" 2>/dev/null || true
ufw allow 80/tcp comment "HTTP" 2>/dev/null || true

echo "  ✓ Firewall configured"

echo ""
echo "╔══════════════════════════════════════╗"
echo "║   SoundNest Setup Complete! ✓        ║"
echo "╚══════════════════════════════════════╝"
echo ""
echo "  Dashboard: http://$HUB_HOSTNAME.local:$DASHBOARD_PORT"
echo "  MQTT:       $MQTT_BROKER"
echo ""
echo "  Next steps:"
echo "  1. Pair sensor nodes using the mobile app"
echo "  2. Place sensors in rooms"
echo "  3. Calibrate SPL meters"
echo "  4. Configure alert rules"
echo ""
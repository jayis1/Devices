#!/bin/bash
# SoundNest Dashboard Deployment Script

set -e

DASHBOARD_DIR="$(dirname "$0")/../software/dashboard"
VENV_DIR="/opt/soundnest/venv"

echo "╔══════════════════════════════════════╗"
echo "║   SoundNest Dashboard Deployment     ║"
echo "╚══════════════════════════════════════╝"
echo ""

# ── Install Dependencies ────────────────────────────────────────────────

echo "[1/4] Installing Python dependencies..."
if [ ! -d "$VENV_DIR" ]; then
    python3 -m venv "$VENV_DIR"
fi
source "$VENV_DIR/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet fastapi uvicorn sqlalchemy aiofiles paho-mqtt python-multipart

echo "  ✓ Dependencies installed"

# ── Database Migration ──────────────────────────────────────────────────

echo "[2/4] Running database migrations..."
cd "$DASHBOARD_DIR"
if [ -d "alembic" ]; then
    alembic upgrade head 2>/dev/null || echo "  ⚠ No migrations to run"
else
    echo "  ⚠ No alembic directory found, skipping migrations"
fi

echo "  ✓ Database ready"

# ── Configure Systemd ───────────────────────────────────────────────────

echo "[3/4] Configuring systemd service..."
cat > /etc/systemd/system/soundnest-dashboard.service <<EOF
[Unit]
Description=SoundNest Dashboard (FastAPI)
After=network.target mosquitto.service
Wants=mosquitto.service

[Service]
Type=simple
User=root
WorkingDirectory=$DASHBOARD_DIR
ExecStart=$VENV_DIR/bin/uvicorn app.main:app --host 0.0.0.0 --port 8000 --workers 2
Restart=always
RestartSec=5
Environment=PYTHONUNBUFFERED=1

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable soundnest-dashboard

echo "  ✓ Service configured"

# ── Start Dashboard ──────────────────────────────────────────────────────

echo "[4/4] Starting dashboard..."
systemctl restart soundnest-dashboard
sleep 2
systemctl status soundnest-dashboard --no-pager || true

echo ""
echo "╔══════════════════════════════════════╗"
echo "║   Dashboard Deployed! ✓               ║"
echo "╚══════════════════════════════════════╝"
echo ""
echo "  URL: http://$(hostname -I | awk '{print $1}'):8000"
echo "  API: http://$(hostname -I | awk '{print $1}'):8000/docs"
echo ""
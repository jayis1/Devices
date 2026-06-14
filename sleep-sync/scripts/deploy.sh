# SleepSync — Deployment Guide

## Cloud Deployment

### 1. Server Requirements
- Linux server (Ubuntu 22.04+ recommended)
- Docker + Docker Compose installed
- 1 GB RAM minimum
- 10 GB storage (for sleep data)
- Internet connectivity for MQTT

### 2. Deploy Backend

```bash
cd software/dashboard
docker-compose up -d
```

This starts:
- FastAPI backend on port 8000
- Mosquitto MQTT broker on port 1883

### 3. Verify

```bash
curl http://localhost:8000/api/sleep/latest
# Should return: {}

curl http://localhost:8000/api/env/recommendations
# Should return population-level recommendations
```

### 4. SSL (Production)

Add a reverse proxy (nginx/Caddy) with TLS:

```nginx
server {
    listen 443 ssl;
    server_name sleepsync.yourdomain.com;
    
    ssl_certificate /etc/letsencrypt/live/sleepsync.yourdomain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/sleepsync.yourdomain.com/privkey.pem;
    
    location / {
        proxy_pass http://127.0.0.1:8000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
    
    location /ws/live {
        proxy_pass http://127.0.0.1:8000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

## ML Pipeline Deployment

### Train Models

```bash
cd scripts
chmod +x train_all.sh
./train_all.sh
```

Outputs:
- `software/ml-pipeline/sleep_staging.tflite` (~180KB)
- `software/ml-pipeline/apnea_detector.tflite` (~60KB)

### Deploy to Hub

Copy TFLite models to hub's SD card:

```bash
# Mount SD card (or use OTA)
cp software/ml-pipeline/sleep_staging.tflite /sdcard/models/
cp software/ml-pipeline/apnea_detector.tflite /sdcard/models/
```

### Cloud-side Optimizer

The environment optimizer runs as a nightly cron job:

```bash
# Add to crontab on cloud server
0 7 * * * cd /opt/sleepsync && python3 -m ml_pipeline.env_optimizer --daily
```

## Mobile App

### Build

```bash
cd software/mobile-app
npm install
npx react-native run-android  # or run-ios
```

### Configuration

Update `API_BASE` in `App.tsx` to point to your hub:
```typescript
const API_BASE = 'http://YOUR_HUB_IP:8000/api';
```

## Hub Setup

1. Flash firmware (see `scripts/flash_all.sh`)
2. Insert microSD card (FAT32 formatted)
3. Copy TFLite models to `/models/` on SD
4. Copy soundscape audio files to `/audio/` on SD
5. Connect USB-C power
6. Hub creates BLE advertising network
7. Use mobile app to provision WiFi credentials

## Node Setup

1. Flash each node's firmware
2. Power on — nodes auto-join hub's BLE mesh
3. Climate node: point IR blaster at AC unit, run IR learning
4. Shade controller: power on, run auto-calibration (limit switches)
5. Sleep strip: place under pillow, Qi-charge first
# SleepSync — Architecture & Protocol Docs

## BLE 5 Mesh Protocol

### Physical Layer
- **Radio:** ESP32-S3/C3 + nRF52832, 2.4GHz BLE 5.0
- **Modulation:** GFSK, 1 Mbps PHY (long range: 125kbps coded PHY)
- **TX Power:** 0 dBm (sleep strip), +4 dBm (hub/climate/shade)
- **Range:** ~10m indoor (single bedroom)
- **Topology:** Managed flooding mesh (ESP-BLE-MESH)

### Mesh Network
- **Provisioner:** Nightstand Hub (ESP32-S3)
- **Max nodes:** 6 per network (1 hub + 1 strip + 1 climate + 3 shades)
- **Security:** AES-CCM 128-bit encryption, Network Key + Application Key
- **Join:** New node advertises as unprovisioned device → hub provisions via app or auto

### Message Flow
```
Every 5 seconds:
  Sleep Strip ──MSG_SLEEP_DATA──► Nightstand Hub
                                ├── Run sleep staging (TFLite Micro)
                                ├── Update environment setpoints
                                ├── Check smart alarm
                                └── Forward to WiFi/MQTT

Every 30 seconds:
  Climate Node ──MSG_ENV_DATA──► Nightstand Hub

Every 60 seconds:
  Shade Controller ──MSG_SHADE_STATUS──► Nightstand Hub

On event:
  Nightstand Hub ──MSG_HUB_COMMAND──► Climate/Shade (setpoint change, open/close)
  Nightstand Hub ──MSG_ALARM_TRIGGER──► All nodes (morning alarm)
```

## WiFi / MQTT Bridge (ESP32-S3 on Hub)

### MQTT Topics
- `sleepsync/{device_id}/sleep_data` — Sleep staging + vitals (JSON, every 5s)
- `sleepsync/{device_id}/env_data` — Environment readings (JSON, every 30s)
- `sleepsync/{device_id}/shade_data` — Shade status (JSON, every 60s)
- `sleepsync/{device_id}/alarm` — Alarm triggered event
- `sleepsync/{device_id}/daily_report` — Morning sleep report
- `sleepsync/{device_id}/env_setpoints` — ML-optimized setpoints (cloud → hub)
- `sleepsync/{device_id}/ota` — OTA firmware blocks
- `sleepsync/{device_id}/commands` — Remote commands from mobile app

### BLE GATT Service (Mobile App Direct Connection)
- Service UUID: `0xFFC0` (SleepSync)
- Char `0xFFC0`: Sleep score + stage (read/notify)
- Char `0xFFC1`: Environment data (read)
- Char `0xFFC2`: Sound control (write)
- Char `0xFFC3`: Climate control (write)
- Char `0xFFC4`: Shade control (write)
- Char `0xFFC5`: Alarm config (write)
- Char `0xFFC6`: System status (read/notify)
- Char `0xFFC7`: WiFi config (write, provisioning only)

## Sleep Staging Model

### Architecture
```
Input: [60, 11] — 60 timesteps × 11 features
  │
  ├── Conv1D(64, k=5, same) + BatchNorm
  ├── Conv1D(32, k=3, same) + BatchNorm
  ├── MaxPool1D(2)
  ├── BiLSTM(64, return_sequences)
  ├── Self-Attention (Dense(1, tanh) → Softmax → Multiply)
  ├── ReduceMean (temporal collapse)
  ├── Dense(64, ReLU) + Dropout(0.3)
  ├── Dense(32, ReLU)
  └── Dense(4, Softmax) → [wake, light, deep, REM]
```

### Features (11 per timestep)
1. `heart_rate` — BPM × 10 (normalized to [0,1])
2. `hrv` — Heart rate variability 0-255
3. `resp_rate` — Breaths/min × 10
4. `rrv` — Respiration variability 0-255
5. `movement` — Movement intensity 0-255
6. `snoring` — Snoring intensity 0-255
7. `sleep_stage` — Previous stage estimate 0-3
8. `stage_conf` — Stage confidence 0-255
9. `battery_pct` — Strip battery 0-100
10. `hr_rr_ratio` — Heart-to-respiration ratio (derived)
11. `movement_ma` — 5-sample moving average of movement (derived)

### Deployment
- **Platform:** TFLite Micro on ESP32-S3 (240MHz dual-core, 8MB PSRAM)
- **Quantization:** INT8 (full integer)
- **Model size:** ~180KB
- **Inference time:** ~15ms per window
- **Accuracy:** ~82% vs PSG benchmark

## Apnea Detection Model

### Architecture
```
Input: [6000, 1] — 30s BCG window at 200Hz
  │
  ├── Conv1D(64, k=7) + BatchNorm + MaxPool(4)
  ├── Conv1D(32, k=5) + BatchNorm + MaxPool(4)
  ├── LSTM(64)
  ├── Dense(32, ReLU) + Dropout(0.3)
  └── Dense(4, Softmax) → [normal, snoring, apnea, hypopnea]
```

### Deployment
- **Model size:** ~60KB
- **Runs on:** Hub every 30s during sleep
- **Action:** If AHI > 5 over 7-day window, recommend clinical evaluation

## Environment Optimizer

### Algorithm
- **Bayesian Optimization** with Gaussian Process surrogate
- **Input:** 30+ nights of [temperature, humidity] → sleep quality composite
- **Acquisition:** Expected Improvement (EI) with exploration parameter ξ=0.01
- **Output:** Personalized optimal [temp, humidity] per sleep stage
- **Cold start:** Population priors (18.3°C, 45% RH) for first 7 nights
- **Update:** Nightly re-fit after new data available

## Smart Alarm Algorithm

### Operation
1. User sets alarm window (e.g., 6:30 – 7:00 AM)
2. Dawn simulation begins 30 min before window start
3. At window start, hub evaluates current sleep stage:
   - **Light sleep** → trigger alarm immediately (optimal)
   - **REM sleep** → trigger within 5 min (acceptable)
   - **Deep sleep** → delay, use HMM to predict transition
   - **Hard alarm** at window end regardless of stage
4. Alarm: soundscape fade-in (60s) + shade open + display briefing

### Dawn Simulation
- 30-minute LED program on shade controller:
  - 0-30%: Amber only (deep sunrise glow)
  - 30-60%: Amber + warm white (golden hour)
  - 60-100%: Warm white + cool white (full daylight)
- Shade opens from 0% to 100% over last 5 minutes

## Alert Priority Levels

| Level | Notification | Local Action |
|-------|-------------|-------------|
| INFO | Dashboard only | LED blue pulse |
| WARNING | Push notification | LED amber + brief tone |
| CRITICAL | Push + vibration | LED red + alarm sound |
| HEALTH | Weekly report | Apnea risk update |

## OTA Update Protocol

1. Dashboard uploads new firmware binary to cloud
2. Cloud sends OTA trigger via MQTT → hub (with size + CRC)
3. Hub downloads firmware blocks, caches on SD
4. Hub distributes OTA_BLOCK messages to target node via BLE mesh
5. Target node writes to flash, verifies CRC, sends ACK
6. On all blocks received + verified, target reboots into new firmware
7. Hub monitors heartbeat from updated node
8. If no heartbeat in 60s, rollback to previous firmware

## Data Privacy

- **No camera** in bedroom — only BCG + actigraphy
- **No raw BCG stored or transmitted** — only extracted features (HR, RR, movement)
- **Local-first** — 7-day buffer on SD card, cloud upload optional
- **Encrypted BLE mesh** — AES-CCM 128-bit
- **Encrypted MQTT** — TLS 1.3 for cloud communication
- **User data ownership** — full export/delete available in app
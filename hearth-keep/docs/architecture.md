# HearthKeep Architecture

## System Overview

HearthKeep is a 4-node ambient elder safety monitoring system. It uses mmWave radar (no cameras) to detect falls, an under-mattress pressure mat to monitor heart rate and breathing, and an optional wearable panic tag. All nodes communicate over a Sub-GHz LoRa mesh network coordinated by a central hub.

```
┌──────────────────────────────────────────────────────────────────────┐
│                         CLOUD LAYER                                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │
│  │  FastAPI      │  │  PostgreSQL  │  │  Mosquitto   │               │
│  │  REST API     │  │  Time-series │  │  MQTT Broker  │               │
│  │  + WebSocket  │  │  + Alerts     │  │  + Bridge    │               │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘               │
│         │                  │                  │                        │
│  ┌──────┴───────┐  ┌──────┴───────┐  ┌──────┴───────┐               │
│  │  ML Pipeline  │  │  React       │  │  Push        │               │
│  │  Anomaly Det  │  │  Dashboard   │  │  Alerts      │               │
│  │  Health Trend │  │  + History    │  │  (APNs/FCM)  │               │
│  └──────────────┘  └──────────────┘  └──────────────┘               │
└──────────────────────────────────────────────────────────────────────┘
                              ▲ MQTT/TLS
                              │
┌──────────────────────────────────────────────────────────────────────┐
│                         HOME NETWORK                                  │
│                                                                       │
│  ┌───────────────┐   Sub-GHz    ┌──────────────┐                    │
│  │  Room Monitor │◄────────────►│              │                    │
│  │  (Living Room)│   868MHz     │              │                    │
│  └───────────────┘   LoRa mesh  │              │──── WiFi6 ────►Cloud
│                                │   HUB NODE    │                    │
│  ┌───────────────┐             │  nRF5340 +    │──── BLE ─────►Tag  │
│  │  Room Monitor │◄───────────►│  ESP32-C6     │                    │
│  │  (Bathroom)  │   Sub-GHz    │              │──── I2S ─────►Speaker
│  └───────────────┘   mesh      │              │◄─── I2S ─────►Mic   │
│                                │              │──── TFT ─────►Display│
│  ┌───────────────┐             │              │                    │
│  │  Room Monitor │◄───────────►│              │                    │
│  │  (Kitchen)   │   Sub-GHz    └──────┬───────┘                    │
│  └───────────────┘   mesh            │                             │
│                                      │ Sub-GHz mesh                │
│  ┌───────────────┐                   │                             │
│  │  Bed Mat      │◄──────────────────┘                             │
│  │  (Under       │   Sub-GHz mesh                                  │
│  │   mattress)   │                                                 │
│  └───────────────┘                                                  │
└──────────────────────────────────────────────────────────────────────┘
```

## Node Architecture

### Hub Node (nRF5340 + ESP32-C6)

The hub is the central coordinator. It:
- Runs the TDMA mesh scheduler (assigns time slots to all nodes)
- Receives and aggregates data from all room monitors and bed mat
- Runs local TFLite Micro fall detection verification
- Runs activity recognition model
- Bridges Sub-GHz mesh to WiFi (MQTT) and BLE (mobile app + wearable tags)
- Drives the TFT display showing room status and alerts
- Provides two-way voice via I2S speaker and microphone
- Has an emergency button for manual panic activation
- Operates for 8+ hours on battery during power outages (mesh-only mode: 40+ hours)

### Room Monitor (nRF52833 + BGT60TR13C)

The room monitor is the core fall detection device. It:
- Runs the BGT60TR13C 60GHz FMCW radar continuously (100ms cycle in presence mode)
- Switches to high-resolution point cloud mode (10Hz) when person detected
- Runs a TFLite Micro fall detection classifier locally on the nRF52833
- Detects falls with >97% sensitivity, >99% specificity
- Measures environment (temp, humidity, IAQ, light) every 30 seconds
- Reports to hub via Sub-GHz mesh in its TDMA time slot
- Operates on USB power with AA battery backup (48 hours)
- IP65 version available for bathrooms

### Bed Mat (STM32L476RG)

The under-mattress vital signs monitor. It:
- Uses 8 FSR-402 pressure sensors at 250Hz for ballistocardiography
- Extracts heart rate and breathing rate through mattress
- Estimates sleep phase (light/deep/REM/awake)
- Detects in/out of bed events
- Operates for 5.5 days on a single charge (or continuously with USB-C)
- Drops to 50µA when bed is empty (theoretical 4+ year battery life)
- Auto-calibrates on first use (learns mattress firmness)

### Wearable Tag (nRF52810)

The optional panic pendant. It:
- BLE 5.0 connectionless advertising every 2 seconds
- Panic button with immediate BLE alert to hub
- Accelerometer-based fall detection as backup to radar
- 6+ month battery life on CR2032
- "I'm OK" long press (3s) to cancel false alarms

## Communication Architecture

### Sub-GHz Mesh (SX1262/61, 868MHz LoRa)

- TDMA protocol with hub as coordinator
- 18 time slots × 50ms = 900ms frame
- Supports up to 16 nodes per hub
- Fall alerts bypass TDMA (CSMA override on slot 17)
- Range: 30m indoor (SF7), 150m (SF9)

### BLE (nRF52833/nRF52810)

- BLE 5.0 for wearable tags and mobile app
- Custom GATT service (HearthKeep Tag)
- Connectionless advertising for tags (2s interval)
- Connected mode for mobile app (dashboard, configuration)

### WiFi (ESP32-C6)

- WiFi 6 (802.11ax) for cloud connectivity
- MQTT QoS 1 with TLS for all uplink data
- WebSocket for real-time dashboard updates
- OTA firmware update distribution

## ML Pipeline Architecture

### On-Device (Room Monitor)
- **Fall Detection**: MobileNetV2-0.1 + 1D-CNN temporal head, INT8 quantized, 75KB
- Input: 16×32 range-Doppler map from BGT60TR13C
- Output: Position class (standing/sitting/lying/falling/fallen/absent) + fall probability
- 3-frame confirmation (300ms) to reduce false positives
- Inference time: ~15ms on nRF52833

### On-Device (Hub)
- **Activity Recognition**: 1D-CNN + LSTM, INT8 quantized, 120KB
- Input: Aggregated motion patterns from all room monitors (5-min windows)
- Output: Activity class (walking/sitting/lying/cooking/bathroom/etc.)

### Cloud (PyTorch)
- **Routine Anomaly Detection**: Transformer autoencoder
- Input: 24-hour activity timeline features
- Output: Anomaly Z-score per day
- Personalized per-person (14-day baseline)

- **Health Trend Predictor**: LSTM with attention
- Input: Longitudinal heart rate, breathing, sleep, movement
- Output: 7-day trend predictions

## Data Flow

```
Room Monitor (radar data, 10Hz during active)
    │
    ├──► Local fall detection (TFLite Micro)
    │    │
    │    ├── fall_prob > 0.85 ──► IMMEDIATE mesh alert (bypasses TDMA)
    │    │                         │
    │    │                         ▼
    │    │                     Hub receives FALL_ALERT
    │    │                         │
    │    │                         ├──► Voice prompt: "Are you okay?"
    │    │                         ├──► 30s wait for "I'm OK" cancel
    │    │                         ├──► No cancel → Push notification to caregiver
    │    │                         └──► 2min no response → Call caregiver → 911
    │    │
    │    └── fall_prob < 0.85 ──► Normal TDMA mesh report (1s cycle)
    │                              │
    │                              ▼
    │                         Hub aggregates all room data
    │                              │
    │                              ├──► Activity recognition (local ML)
    │                              ├──► MQTT publish (WiFi → Cloud)
    │                              ├──► TFT display update
    │                              └──► BLE notify mobile app
    │
Bed Mat (vitals data, 30s while in bed)
    │
    └──► Mesh report (TDMA slot 9)
         │
         ▼
    Hub receives BED_VITALS
         │
         ├──► Local vital sign monitoring
         ├──► Heart rate/breathing alerts
         └──► MQTT publish → Cloud health trend analysis

Wearable Tag (BLE advertising, 2s cycle)
    │
    ├──► Panic button press ──► BLE alert to Hub
    │                              │
    │                              └──► PANIC_ALERT processing
    │
    └──► Accelerometer fall detect ──► BLE alert to Hub
                                         │
                                         └──► Cross-reference with radar data
```

## Privacy Architecture

HearthKeep is designed with privacy as a core principle:

1. **No cameras** — mmWave radar provides position/movement data without images
2. **No audio recording** — microphone only active during explicit voice sessions
3. **On-device ML** — fall detection and activity recognition run locally
4. **Minimal uplink** — only processed results sent to cloud (never raw radar data)
5. **Encrypted transport** — TLS for MQTT, BLE encryption for tags
6. **Encrypted storage** — AES-256 at rest in PostgreSQL
7. **User data ownership** — full export and deletion API endpoints
8. **Privacy mode** — setting that stops all cloud upload (local-only operation)
9. **Open firmware** — full source available for security audit

## Deployment Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Docker Compose (Cloud Server or Home Server)            │
│                                                          │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐        │
│  │  FastAPI    │  │  PostgreSQL│  │  Mosquitto  │        │
│  │  :8000      │  │  :5432      │  │  :1883      │        │
│  └────┬───────┘  └─────┬──────┘  └─────┬──────┘        │
│       │                 │                 │                │
│  ┌────┴───────┐  ┌─────┴──────┐  ┌─────┴──────┐        │
│  │  React      │  │  ML Pipeline│  │  Push      │        │
│  │  :3000      │  │  (training)  │  │  Service   │        │
│  └────────────┘  └────────────┘  └────────────┘        │
└─────────────────────────────────────────────────────────┘
```

Self-hosted option: Run the entire stack on a Raspberry Pi 4 or NAS alongside the hub.
Cloud option: Deploy to any cloud provider (AWS, GCP, Azure) with Docker Compose.
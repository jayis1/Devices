# HiveSync — Architecture Document

## System Overview

HiveSync is a distributed IoT system for real-time beehive health monitoring and management. It combines multi-sensor hardware nodes, Sub-GHz mesh networking, edge ML inference, and cloud analytics to help beekeepers detect problems early and take action.

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        CLOUD LAYER                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │  FastAPI      │  │  TimescaleDB │  │  Push Notif      │  │
│  │  Backend      │  │  (sensor TS) │  │  (FCM/APNs)      │  │
│  └──────┬───────┘  └──────────────┘  └──────────────────┘  │
│         │           ┌──────────────┐                        │
│         │           │  ML Models   │                        │
│         │           │  (PyTorch)   │                        │
│         │           └──────────────┘                        │
└─────────┼───────────────────────────────────────────────────┘
          │ MQTT over TLS / HTTPS
┌─────────┼───────────────────────────────────────────────────┐
│         │            APIARY LAYER                           │
│  ┌──────┴───────┐                                          │
│  │  Hive Gateway│  Raspberry Pi Zero 2W                    │
│  │  (Hub)       │  ┌──────────────────────┐                │
│  │              │  │ ESP32-S3 + CC1101     │                │
│  │  ┌─────────┐ │  │ Wi-Fi/LTE bridge      │                │
│  │  │ TFLite  │ │  │ Sub-GHz coordinator   │                │
│  │  │ Edge ML │ │  │ Local MQTT broker      │                │
│  │  └─────────┘ │  │ 7-day data buffer      │                │
│  └──────────────┘  └──────────────────────┘                │
│          │  Sub-GHz 868 MHz mesh                           │
│    ┌─────┼────────┼────────┐                              │
│    │     │        │        │                              │
│  ┌─┴──┐ ┌┴────┐ ┌┴──────┐ ┌┴──────────┐                │
│  │Sensor│ │Sensor│ │Entrance│ │Smart      │                │
│  │Node │ │Node  │ │Monitor │ │Feeder     │                │
│  │×N   │ │×N   │ │×1-N    │ │×1-N       │                │
│  └─────┘ └─────┘ └────────┘ └───────────┘                │
│    Per hive           Per apiary     Per hive              │
└─────────────────────────────────────────────────────────────┘
```

## Data Flow

### Sensor Data Pipeline

```
Sensor Node → [5 min interval]
  ├─ SHT45 ×3 (temp/humidity)
  ├─ HX711 + load cells (weight)
  ├─ LIS3DH (vibration)
  ├─ ICS-43434 (audio → FFT features)
  └─ Battery ADC
      │
      ▼ CC1101 Sub-GHz (868 MHz, TDMA)
Gateway → [receive + buffer]
  ├─ Local MQTT publish
  ├─ TFLite inference (swarm, queen)
  └─ HTTPS/MQTT to Cloud
      │
      ▼
Cloud → [ingest + store + infer]
  ├─ TimescaleDB storage
  ├─ Full ML pipeline (PyTorch models)
  ├─ Alert generation
  └─ Push notifications
      │
      ▼
Mobile App → [display + action]
  ├─ Dashboard, charts, alerts
  └─ Feeder commands → Gateway → Feeder
```

### Alert Pipeline

```
Sensor Data → Gateway Edge ML
  ├─ SwarmPredictor LSTM: probability > 0.7? → CRITICAL swarm alert
  ├─ QueenHealth CNN+GRU: queen missing? → HIGH queen alert
  ├─ Accelerometer spike? → disturbance / theft alert
  └─ Anomaly in weight/traffic? → MEDIUM attention alert

Entrance Monitor → Gateway
  ├─ VarroaDetector: mites/bbee > 0.03? → HIGH mite alert
  ├─ ForagerCounter: traffic anomaly? → MEDIUM traffic alert
  └─ Forager mortality spike? → HIGH pesticide alert
```

## Sub-GHz Mesh Network

### Topology

- **Star + relay**: Gateway is central hub; nodes can relay for distant nodes
- **TDMA**: 60-second frames, 2-second slots per node
- **Frequency**: 868.0 MHz (EU) / 915 MHz (US) — configurable
- **Range**: 500m+ line-of-sight (apiary-scale)
- **Data rate**: 250 kbps (sufficient for sensor data + compressed thumbnails)

### TDMA Schedule

```
Slot 0:      Gateway beacon (network sync, slot assignments)
Slots 1-30:  Node data uploads (1 slot per active node)
Slots 31-45: Mesh relay slots (for out-of-range nodes)
Slots 46-59: ALOHA contention (alerts, commands, OTA)
```

### Power Management

- **Sensor Node**: Deep sleep between samples (98.3% duty cycle). Avg current: ~180 µA. Battery life: ~2.8 years (CR3032).
- **Entrance Monitor**: Continuous camera at 10 fps. Avg current: ~80 mA. Requires solar + 18650.
- **Smart Feeder**: Deep sleep with hourly weight check. Avg current: ~200 µA. Battery: 6+ months (18650 + solar).
- **Gateway**: Always-on. Avg current: ~300 mA. Requires 5W solar + 2×18650.

## ML Models

| Model | Input | Architecture | Edge/Cloud | Size | Latency |
|-------|-------|---------------|------------|------|---------|
| SwarmPredictor | 14-day × 4 features | BiLSTM + Attention | Gateway (TFLite) | 450 KB | 15 ms |
| VarroaDetector | 224×224 RGB | EfficientNet-B0 | Entrance (TFLite Micro) | 1.2 MB | 80 ms |
| QueenHealth | Acoustic + Temp | 1D-CNN + GRU | Gateway (TFLite) | 280 KB | 8 ms |
| ForagerCounter | 160×160 RGB | YOLOv8-nano | Entrance (TFLite Micro) | 800 KB | 50 ms |
| ColonyStrength | Tabular features | XGBoost | Cloud only | 120 KB | 2 ms |
| PesticideAlert | Traffic stats | Isolation Forest | Cloud only | 50 KB | 1 ms |

## Security

- **Transport**: TLS 1.3 for all cloud connections (HTTPS/MQTT)
- **Data at rest**: AES-256 encrypted storage on gateway
- **Authentication**: JWT tokens for API, X.509 certificates for device auth
- **Firmware**: Signed OTA updates (Ed25519 signatures)
- **Privacy**: No hive location data shared without beekeeper consent
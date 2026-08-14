# PostureSync Architecture

## System Overview

PostureSync is a distributed multi-node system for real-time posture monitoring, correction, and long-term spinal health risk assessment.

```
┌─────────────────────────────────────────────────────────────────────┐
│                        PostureSync Cloud                           │
│  FastAPI + MQTT + PostgreSQL + ML Pipeline (6 models)              │
│  • SpinalRisk LSTM (90-day spinal health risk forecast)            │
│  • PostureCNN (12-class posture classification)                    │
│  • MuscleImbalance XGBoost (bilateral EMG asymmetry)               │
│  • ErgonomicCoach DQN (personalized correction timing)             │
│  • ScoliosisScreen Bayesian (early scoliosis detection)            │
│  • SpinalAge Regressor (biological spinal age estimation)          │
└──────────────┬──────────────────────────────────────────────────────┘
               │ MQTT over TLS  (Wi-Fi / 4G LTE backup)
               │
    ┌──────────┴──────────┐
    │   PostureSync Hub   │
    │   (ESP32-S3 + Wi-Fi │
    │    + Sub-GHz 868    │
    │    MHz TDMA mesh    │
    │    coordinator)     │
    └──┬──────┬──────┬────┘
       │      │      │     Sub-GHz 868 MHz TDMA mesh
       │      │      │     + BLE 5.0 (for wearable)
  ┌────┴──┐ ┌─┴────────┐ ┌─┴──────────┐ ┌──────────────┐
  │ Spine │ │ Posture  │ │  Smart     │ │  Desk       │
  │ Band  │ │ Garment  │ │  Chair Pad │ │  Sentinel   │
  └───────┘ └──────────┘ └────────────┘ └──────────────┘
```

## Data Flow

1. **Sensor nodes** collect data (IMU, EMG, pressure, ToF, light)
2. **Wearable nodes** (Spine Band, Posture Garment) send data via BLE 5.0 to Hub
3. **Fixed nodes** (Chair Pad, Desk Sentinel) send data via Sub-GHz 868 MHz TDMA mesh to Hub
4. **Hub** aggregates, runs on-device PostureCNN, displays posture score on e-ink, triggers haptic corrections
5. **Hub** forwards aggregated data to cloud via MQTT over TLS
6. **Cloud** runs ML pipeline (5 additional models), stores in PostgreSQL, serves API
7. **Mobile app** receives real-time data via WebSocket, displays dashboard, fetches forecasts

## Communication Layers

### Layer 1: BLE 5.0 (Wearable → Hub)
- Spine Band: 10 Hz posture data, 1 Hz PPG
- Posture Garment: 10 Hz EMG + IMU data
- GATT service with custom 128-bit UUID
- Notifications for real-time data streaming

### Layer 2: Sub-GHz 868 MHz TDMA Mesh (Fixed nodes → Hub)
- Hub = TDMA coordinator (sends beacons, assigns slots)
- 1-second superframe: 20ms beacon + 18× 50ms slots
- CRC-16/CCITT frame verification
- Join/leave protocol for mesh membership
- AES-128 encryption (in production)

### Layer 3: Wi-Fi/MQTT (Hub → Cloud)
- MQTT 3.1.1 over TLS 1.3
- Topics: postsync/sensor/{node}, postsync/alerts, postsync/commands/{node}
- QoS 1 (at-least-once delivery)
- 4G LTE cellular backup (in production)

### Layer 4: WebSocket (Cloud → Mobile App)
- Real-time posture score, EMG, alerts
- Automatic reconnection with backoff

## ML Architecture

### On-Device (Hub, ESP32-S3)
- **PostureCNN**: 1D-CNN, 48KB quantized int8, 12ms inference
  - Input: 6-axis IMU, 200Hz, 2s window
  - Output: 12-class posture classification

### Cloud (FastAPI + ONNX Runtime)
- **SpinalRisk LSTM**: 30-day → 90-day risk forecast
- **MuscleImbalance XGBoost**: 8-channel EMG → 6-class imbalance
- **ErgonomicCoach DQN**: Optimal correction timing
- **ScoliosisScreen Bayesian**: Change-point + GP regression
- **SpinalAge Regressor**: LightGBM, 24 features → spinal age

## Power Architecture

| Node | Power Source | Battery Life | Power Consumption |
|------|-------------|-------------|-------------------|
| Hub | USB-C 5V + 18650 LiFePO4 backup | 8+ hours backup | ~200mA avg |
| Spine Band | 402030 LiPo 400mAh | 5 days | ~3.3mA avg |
| Posture Garment | 502035 LiPo 500mAh | 3 days | ~6.9mA avg |
| Chair Pad | 2× AAA (LR03) | 6 months | ~0.3mA avg (deep sleep) |
| Desk Sentinel | USB-C 5V + 2× AAA backup | 12+ hours backup | ~15mA avg |

## Security

- TLS 1.3 for all cloud communication
- AES-128 for Sub-GHz mesh (in production)
- BLE pairing with bonding
- HIPAA-compliant clinical reports (de-identified)
- No raw sensor data leaves Hub unless cloud sync enabled
- No cameras — privacy-first (IMU + pressure + EMG only)
# AsthmaSync — Architecture Document

## Overview

AsthmaSync is a 4-node hardware+software system for continuous asthma management. It combines wearable wheeze detection, inhaler adherence tracking, environmental trigger sensing, and AI-powered risk forecasting into a single integrated platform.

## Design Principles

1. **Privacy-first** — audio is processed on-device; only mel-spectrogram features (not raw audio) are sent to the cloud
2. **Offline-capable** — the Hub caches 14 days of data in PSRAM and syncs when WiFi is available
3. **Clinical alignment** — all thresholds and action plans follow GINA 2023 guidelines
4. **Low-power** — the Inhaler Tag runs 6 months on a CR2032; the Wheeze Band runs 1.5 days on a 200 mAh LiPo
5. **Buildable** — all components are off-the-shelf; no custom silicon

## Data Flow

```
[Sensors] → [Firmware] → [Protocol Pack] → [Radio/BLE] → [Hub] → [MQTT/TLS] → [Cloud]
                                                                              ↓
                                                                    [TimescaleDB]
                                                                              ↓
                                                                    [ML Pipeline]
                                                                    (LSTM + XGBoost
                                                                     + CNN + RF)
                                                                              ↓
                                                                    [FastAPI REST]
                                                                              ↓
                                                                    [Mobile App]
```

## Node Roles

### Hub (ESP32-S3)
- **Role**: Central coordinator + edge ML inference
- **Radios**: WiFi (cloud), Sub-GHz SX1262 (mesh coordinator), BLE 5.0 (central)
- **ML**: tflite-micro wheeze CNN (22-class) + actuation classifier (4-class)
- **Storage**: 14-day rolling cache in PSRAM (8 MB)
- **Display**: 2.4" TFT status display
- **Alerts**: RGB LED + buzzer + mobile push notification

### Air Sentinel (ESP32-S3)
- **Role**: Environmental trigger monitoring
- **Sensors**: PMSA003I (PM), BME688 (VOC/temp/humidity), SGP40 (HCHO), SCD41 (CO₂)
- **Radio**: Sub-GHz SX1262 mesh node
- **Power**: USB-C mains (always-on) with 18650 backup

### Inhaler Tag (nRF52840)
- **Role**: Rescue inhaler actuation detection
- **Sensor**: LSM6DSO 6-axis IMU
- **Detection**: Accelerometer shake-signature classification (4-class)
- **Radio**: BLE 5.0 GATT notify
- **Power**: CR2032 (6-month life)
- **Form**: 18mm disc that clips onto MDI canister

### Wheeze Band (nRF52840)
- **Role**: Continuous wheeze detection + vitals monitoring
- **Sensors**: SPH0645 I²S microphone, MAX30101 PPG (HR/HRV/SpO₂), TMP117 temp, LSM6DSO IMU
- **Audio**: 16 kHz capture → 40-band mel-spectrogram → on-device pre-classifier → Hub CNN
- **Radio**: BLE 5.0 GATT notify
- **Power**: 200 mAh LiPo (1.5-day, USB-C charging)

## Communication Architecture

### Sub-GHz Mesh (Hub ↔ Air Sentinel)
- **Protocol**: LoRa SX1262, 868 MHz (EU) / 915 MHz (US)
- **Modulation**: LoRa SF7, BW 125 kHz, CR 4/5
- **MAC**: TDMA (Hub is coordinator, 8 slots × 200ms = 2s superframe)
- **Range**: 300m LoS, 60m indoor
- **Payload**: AsthmaSync protocol (10-byte header + up to 128-byte payload)

### BLE 5.0 (Hub ↔ Inhaler Tag / Wheeze Band)
- **Profile**: Custom GATT service (0xA501)
- **Characteristics**:
  - 0x2A01: Telemetry (notify) — sensor data
  - 0x2A03: Event (notify) — discrete events
  - 0x2A02: Command (write) — config updates
- **Connection**: 30ms interval, 4s supervision timeout
- **MTU**: 247 bytes (for audio feature transmission)

### WiFi (Hub ↔ Cloud)
- **Protocol**: WPA2-PSK → MQTT over TLS 1.3
- **Topics**: `asthmasync/telemetry`, `asthmasync/events`, `asthmasync/commands`
- **Offline**: PSRAM ring buffer (512 packets) flushed on reconnection

## ML Architecture

### Edge (Hub — tflite-micro)
| Model | Input | Output | Size | Latency |
|-------|-------|--------|------|---------|
| Wheeze CNN | 40×32 mel-spectrogram | 22-class softmax | 180 KB | 35 ms |
| Actuation RF | 12 accel features | 4-class | 24 KB | 8 ms |

### Cloud (FastAPI + Celery)
| Model | Framework | Purpose | Retrain |
|-------|-----------|---------|---------|
| Exacerbation LSTM | PyTorch | 7-day risk forecast | Nightly |
| Trigger XGBoost | XGBoost + SHAP | Per-trigger attribution | Weekly |
| Wheeze CNN | PyTorch | 22-class sound classification | Monthly |
| Actuation RF | scikit-learn | Inhaler event classification | Monthly |
| Lung-function trend | Bayesian change-point | FEV₁-proxy decline detection | Daily |

## Power Budget

| Node | Average Current | Battery | Life |
|------|----------------|---------|------|
| Hub | 120 mA @ 5V | USB-C mains | Continuous |
| Air Sentinel | 80 mA @ 5V | USB-C mains | Continuous |
| Inhaler Tag | 15 µA | CR2032 (220 mAh) | 6 months |
| Wheeze Band | 5.5 mA | 200 mAh LiPo | 36 hours |

## Security

- **Transport**: TLS 1.3 (MQTT), WPA2-PSK (WiFi), BLE Secure Connections (LE SC)
- **Storage**: AES-256 at rest (PHI in TimescaleDB)
- **Auth**: OAuth 2.0 (mobile app → API), mTLS (hub → cloud MQTT)
- **Privacy**: Raw audio never leaves the Wheeze Band; only mel-spectrogram features are transmitted
- **Compliance**: HIPAA (US), GDPR (EU), PIPEDA (CA)
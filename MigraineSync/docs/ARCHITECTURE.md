# MigraineSync — Architecture Document

## Overview

MigraineSync is a 4-node hardware+software system for migraine trigger detection and attack prediction. The architecture follows a **hub-and-spoke** model with edge ML inference and cloud-based model training.

## System Topology

```
                     CLOUD LAYER
    ┌────────────────────────────────────────────┐
    │  FastAPI Backend  │  TimescaleDB  │  MQTT   │
    │  6-Model ML Pipeline (cloud inference)     │
    │  React Native API (REST over HTTPS)        │
    └────────────────▲───────────────────────────┘
                     │ MQTT/TLS (WiFi)
    ┌────────────────┴───────────────────────────┐
    │            HUB (ESP32-S3)                  │
    │  Edge ML (tflite-micro)                    │
    │  Sub-GHz 868 MHz TDMA mesh coordinator     │
    │  BLE 5.0 central                           │
    │  14-day local cache (PSRAM + microSD)      │
    └──┬─────────────┬──────────────┬───────────┘
       │ Sub-GHz     │ BLE 5.0      │ BLE 5.0
  ┌────┴──────┐ ┌────┴──────┐ ┌────┴──────┐
  │Env Sentinel│ │ Aura Band │ │Hydrate Tag│
  │ ESP32-S3   │ │ nRF52840  │ │ nRF52840  │
  └────────────┘ └───────────┘ └───────────┘
```

## Data Flow

### Real-time path (sub-second)
1. Aura Band samples PPG at 100 Hz, processes HRV locally every 5 min.
2. Hydrate Tag wakes on tilt, measures load cell, classifies sip, sleeps.
3. Env Sentinel polls all sensors at 1 Hz, computes 3-hour pressure delta.
4. All nodes push to Hub via BLE or Sub-GHz.
5. Hub runs edge ML (prodrome detector + onset predictor) every 15 min.
6. If risk > threshold → immediate local alert (buzzer + TFT + BLE push to band vibrator).

### Cloud path (minutes)
1. Hub batches telemetry every 5 min → MQTT publish.
2. Cloud backend ingests → TimescaleDB insert.
3. Cloud ML pipeline runs hourly: onset predictor, trigger identifier, sleep quality, hydration pattern, barometric change-point.
4. Results pushed to mobile app via REST polling (every 60s) or push notification (on alert).

### Training path (daily)
1. Cloud collects 24h of labeled data (manual migraine logs + sensor data).
2. Nightly training job retrains all 6 models on rolling 90-day window.
3. Updated edge models (quantized) pushed to hub via OTA.
4. Trigger attribution model retrained weekly (needs 7-day windows).

## Edge vs Cloud Split

| Model | Edge (Hub) | Cloud | Rationale |
|-------|-----------|-------|-----------|
| Prodrome detector | ✅ (quantized 1D-CNN, 180 KB) | ✅ (full) | Edge for immediate alerts; cloud for retraining |
| Onset predictor | ✅ (quantized LSTM, 420 KB) | ✅ (full) | Edge for low-latency; cloud for accuracy |
| Trigger identifier | ❌ | ✅ (XGBoost + SHAP) | Needs 7-day window; SHAP too heavy for edge |
| Hydration classifier | ❌ | ✅ (RF) | Low urgency; cloud sufficient |
| Sleep quality | ❌ | ✅ (GBR) | Computed once daily |
| Barometric change-point | ✅ (lightweight) | ✅ (full) | Edge for immediate weather trigger detection |

**Edge ML budget**: ESP32-S3 with 8 MB PSRAM can hold ~1 MB of tflite-micro models. Current edge models total ~600 KB (prodrome 180 KB + onset 420 KB).

## Time Synchronization

- Hub: NTP over WiFi (every 6 hours). RTC backup (DS3231) for offline operation.
- Env Sentinel: time-synced via Sub-GHz mesh (hub broadcasts timestamp beacon every 5 min).
- Aura Band / Hydrate Tag: time-synced via BLE (hub sends current time on connection).

## Reliability

- Hub offline: stores 14 days of data in PSRAM + microSD. Backfills to cloud when WiFi restored.
- Env Sentinel offline: stores 7 days in ESP32-S3 flash (partitioned). Backfills via Sub-GHz when hub restored.
- Aura Band offline: stores 2 days (limited by flash). BLE reconnect triggers backfill.
- Hydrate Tag offline: stores 7 days (low data rate). BLE reconnect triggers backfill.

## Security

- Sub-GHz: AES-128-CCM encryption (SX1262 hardware-assisted). Network key provisioned at pairing.
- BLE: LE Secure Connections (Elliptic Curve Diffie-Hellman). Per-device key.
- WiFi: WPA2-PSK (home network). Hub also supports WPA3-SAE.
- Cloud: MQTT over TLS 1.3. Per-hub client certificate. API: JWT auth with refresh tokens.
- Data at rest: TimescaleDB with column-level encryption for PHI fields.
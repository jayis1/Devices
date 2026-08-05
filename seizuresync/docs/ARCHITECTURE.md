# SeizureSync — Architecture

## Overview

SeizureSync is a 4-node hardware+software system for epilepsy seizure
detection, SUDEP prevention, and epilepsy management.

```
┌──────────────────────────────────────────────────────────┐
│                    CLOUD / EDGE                           │
│  ┌────────────┐  ┌──────────────┐  ┌─────────────────┐    │
│  │ FastAPI    │  │ ML Pipeline  │  │ Twilio Dispatch │    │
│  │ + MQTT     │  │ (8 models)   │  │ (911 + caregiver)│    │
│  │ + Timescale│  │ ONNX runtime │  │                 │    │
│  └─────┬──────┘  └──────┬───────┘  └─────────────────┘    │
│        │ Wi-Fi/MQTT      │                                 │
└────────┼─────────────────┼─────────────────────────────────┘
         │                 │
    ┌────▼─────────────────▼────┐
    │      SEIZURE HUB          │
    │  ESP32-S3 + SX1262        │
    │  BCG bed-mat + SpO2       │
    │  MLX90640 prone detect    │
    │  Edge inference           │
    │  4G LTE backup            │
    │  Bed-shaker relay         │
    └────┬──────────┬───────────┘
         │ Sub-GHz  │ 868 MHz TDMA mesh
    ┌────▼────┐ ┌───▼──────┐ ┌──────────────┐
    │ BAND    │ │ PATCH    │ │ CAREGIVER    │
    │ ESP32-S3│ │ nRF52840 │ │ BEACON       │
    │ accel + │ │ temp +   │ │ ESP32-C3     │
    │ PPG +   │ │ EDA +    │ │ haptic +     │
    │ EDA     │ │ microPPG │ │ audio +      │
    │ CNN     │ │ 14-day   │ │ e-ink + LED  │
    │ on-dev  │ │ coin cell│ │ 7-day battery│
    └─────────┘ └──────────┘ └──────────────┘
```

## Data flow

### Real-time seizure detection (primary path)
1. **Seizure Band** samples accel (2000 Hz) + PPG (100 Hz) + EDA (4 Hz)
2. **SeizureNet 1D CNN** runs on ESP32-S3 every 500 ms (2 s windows)
3. If p(seizure) > 0.85 → band sends `SZ_PKT_SEIZURE_ALERT` via Sub-GHz mesh to hub
4. **Hub cross-validates** with BCG motion energy + SpO2 (reduces false positives)
5. If confirmed → hub relays to **Caregiver Beacon** via Sub-GHz mesh
6. Beacon emits haptic + audio + visual alert
7. Hub publishes event to **cloud MQTT** (Wi-Fi) for ML classification + diary
8. If no caregiver ACK in 90s → cloud triggers Twilio 911 dispatch

### Pre-ictal prediction (early warning)
1. **Aura Patch** samples skin temp (TMP117) + EDA (AD8232) + micro-PPG (MAX30101) at 1 Hz
2. Every 30s, extracts 10-min autonomic history → AuraNet pre-ictal probability
3. If p(pre-ictal) > 0.65 → sends `SZ_PKT_AURA_ALERT` via BLE to hub
4. Hub relays to beacon (yellow alert: "seizure may occur in 5-8 min")
5. Cloud re-runs AuraNet for confirmation + RiskNet update

### SUDEP nocturnal monitoring (critical safety)
1. **Hub** continuously monitors bed-mat BCG (breathing + HR) + MAX30102 SpO2
2. **MLX90640** detects prone position every 2s
3. If SpO2 < 88% OR breathing < 6 breaths/min for > 20s → `SZ_PKT_SUDEP_ALERT`
4. Immediate escalating alarm: bed-shaker relay ON + audio alarm ON
5. Beacon receives SUDEP alert (flashing red + max haptic + max audio)
6. If no ACK in 60s → Twilio 911 auto-dispatch (SUDEP is seconds-critical)
7. 4G LTE backup ensures alert delivery even during internet outage

### Cloud ML pipeline (async)
1. Hub uploads raw signal chunks + event metadata via MQTT
2. **SemiologyNet** classifies ILAE seizure type (5-class)
3. **TriggerNet** (XGBoost + SHAP) attributes triggers to the event
4. **RecoveryNet** estimates post-ictal recovery state + duration
5. **RiskNet** updates 24-hour seizure risk forecast
6. **SUDEP Risk Score** updates annual SUDEP risk
7. Results stored in TimescaleDB; mobile app updated via WebSocket

## Power budget

| Node | Avg current | Battery | Life |
|---|---|---|---|
| Hub | 220 mA @ 5V | USB-C + 12V SLA UPS | Unlimited (plugged) |
| Band | 10 mA @ 3.7V | 500 mAh LiPo | 48 h |
| Patch | 3 mA @ 3V (burst) | CR2477 (1 Ah) | 14 days |
| Beacon | 12 mA @ 3.7V | 2000 mAh LiPo | 7 days |

## Reliability

- **Mesh relaying**: Beacon can relay Patch→Hub if patch out of direct range
- **Fail-open**: If band-hub link lost, band alerts beacon directly
- **Fail-closed**: Bed-shaker relay watchdog forces ON if MCU hang
- **Cellular backup**: 4G LTE (SIM7600G) for alerts during internet outage
- **Battery backup**: 12V SLA on hub for power outage continuity
- **Local storage**: microSD (32 GB) buffers events during cloud outage
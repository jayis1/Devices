# PawSync System Architecture

## Overview

PawSync is a 4-node pet health, behavior & anxiety management system:

1. **Collar Tag** (nRF52840) — wearable: HR/HRV, activity, gait, scratching detection
2. **Behavior Camera** (ESP32-S3) — ambient: behavior CV + vocalization classification
3. **Smart Feeder** (ESP32-C6) — kitchen: weight-verified dispensing + RFID pet ID
4. **Hub Node** (RP2040 + ESP32-C6 + nRF52840) — coordinator: wellness ML + display + enrichment + cloud bridge

## Data Flow

```
Collar Tag ──BLE mesh──► Hub Node ──WiFi6──► Cloud (MQTT → FastAPI → TimescaleDB)
   │  (HR, HRV, activity,      │                   │
   │   gait, scratching,       │  (aggregated      │
   │   temp, battery)           │   vitals +        │
   │                            │   wellness score)  │
Behavior Camera ──WiFi──► Hub + Cloud
   │  (behavior class, vocalization,
   │   anxiety episode flag, clip ref)
   │
Smart Feeder ──BLE mesh──► Hub Node
   │  (feeding event, intake, water level,
   │   RFID pet ID, low-food alert)
   │
Hub ──BLE──► Mobile App (instant alerts)
Hub ──WiFi──► Cloud (MQTT → FastAPI → TimescaleDB)
Cloud ──► Vet Portal (structured reports)
```

## Communication Protocol

- **Body mesh:** BLE 5.3 mesh (nRF52840), 2.4GHz, ~30m range
- **Cloud bridge:** WiFi6 (ESP32-C6), MQTT over TLS
- **Camera link:** WiFi6 (ESP32-S3), events only (no video stream)
- **Mobile:** BLE 5.3 + WiFi
- **Outdoor:** Collar stores 4hr offline, syncs when back in range

## Message Types (paw_protocol.h)

| Type | Direction | Payload | Notes |
|------|-----------|---------|-------|
| PAW_MSG_VITALS | Collar → Hub | HR, HRV, temp, gait, battery | Every 60s |
| PAW_MSG_ACTIVITY | Collar → Hub | Activity class, confidence, duration | Every 60s |
| PAW_MSG_BEHAVIOR | Camera → Hub/Cloud | Behavior class, vocalization, clip ref | Every 30s or on episode |
| PAW_MSG_FEEDING | Feeder → Hub | Dispensed/consumed g, water, hopper % | On feeding event |
| PAW_MSG_ALERT | Any → Hub | Alert flags + value | Immediate (mesh flood) |
| PAW_MSG_WELLNESS | Hub → Mesh | Wellness score, illness risk, anxiety | Every 15 min |
| PAW_MSG_ENRICHMENT | Hub → Feeder/Camera | Treat/audio/voice trigger | On anxiety episode |
| PAW_MSG_HEARTBEAT | Any → Hub | Battery level | Every 5 min |

## Wellness Score Pipeline

1. Collar samples PPG (20s/min) → HR + HRV (RMSSD)
2. Collar samples IMU (50Hz continuous) → activity CNN (5s windows) + gait analysis
3. Collar detects scratching (8-12Hz accel) + head-shaking (3-6Hz yaw)
4. Camera runs behavior CV (5fps) + vocalization classification (10Hz)
5. Feeder tracks food intake + water level
6. Hub aggregates all data every 5 min into ring buffer (24h × 288 slots)
7. Hub runs TFLite Micro wellness model every 15 min → wellness + illness_risk + anxiety
8. Hub triggers enrichment (audio/treat) on anxiety episodes
9. Hub syncs to cloud every 60s via MQTT
10. Cloud runs alert engine + baseline tracking + vet report generation

## Baseline Learning

- 14-day personalization period (4032 samples at 5-min intervals)
- Baseline: resting HR, HRV-RMSSD, skin temp (averaged during rest/sleep)
- After establishment: deviations detected (>20% HRV decline, >15% HR elevation, >0.5°C temp change)
- Sliding window refinement: most recent 7 days for ongoing baseline

## Enrichment System

When separation anxiety is detected (pacing + vocalizing + destruction >5 min):
1. **Mild (anxiety <40):** Play calming audio track (low volume)
2. **Moderate (40-70):** Play learned-best track (medium volume)
3. **Severe (>70):** Play owner voice recording + dispense treat

The system tracks which interventions reduce anxiety over time and auto-selects the most effective one.
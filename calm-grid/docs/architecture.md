# CalmGrid System Architecture

## Overview

CalmGrid is a 4-node personal stress & mental wellness system:

1. **Wrist Band** (nRF52840) — wearable: HR/HRV, EDA (skin conductance), activity, skin temp
2. **Room Sentinel** (ESP32-S3) — ambient: voice prosody stress (no transcription), environment monitoring
3. **Light Node** (ESP32-C6) — actuator: tunable-white LED lighting with circadian + de-stress scenes
4. **Hub Node** (RP2040 + ESP32-C6 + nRF52840) — coordinator: stress ML + display + breathing audio + cloud bridge

## Data Flow

```
Wrist Band ──BLE mesh──► Hub Node ──WiFi6──► Cloud (MQTT → FastAPI → TimescaleDB)
   │  (HR, HRV, EDA, temp,      │                   │
   │   activity, stress flag,    │  (aggregated      │
   │   battery)                   │   vitals +        │
│                                │   stress score)   │
Room Sentinel ──WiFi──► Hub + Cloud
   │  (prosody stress, ambient light,
   │   temp, humidity, noise level,
   │   env-stress flag — NO AUDIO)
   │
Light Node ──BLE mesh──► Hub Node
   │  (scene ack, brightness, ambient lux,
   │   override status)
   │
Hub ──BLE──► Mobile App (instant alerts + breathing cue)
Hub ──WiFi──► Cloud (MQTT → FastAPI → TimescaleDB)
Cloud ──► Therapist Portal (structured reports)
```

## Communication Protocol

- **Body mesh:** BLE 5.3 mesh (nRF52840), 2.4GHz, ~30m range
- **Cloud bridge:** WiFi6 (ESP32-C6), MQTT over TLS
- **Sentinel link:** WiFi6 (ESP32-S3), features only (no audio stream)
- **Mobile:** BLE 5.3 + WiFi
- **Outdoor:** Wrist band stores 6hr offline, syncs when back in range

## Stress Detection Pipeline

```
Wrist Band Sensors          Hub Edge ML              Cloud ML
┌─────────────┐            ┌──────────────┐         ┌──────────────┐
│ PPG → HR/HRV│──BLE──►    │ Stress CNN-  │──WiFi──►│ Burnout      │
│ EDA → SCL/SCR│           │  LSTM (15min)│         │ Predictor    │
│ IMU → Activity│          │ → Stress 0-100│        │ (30-day)     │
│ TMP → Temp   │            │ → Burnout 0-100│       │ → MBI-validated│
└─────────────┘            └──────────────┘         └──────────────┘
                                                          │
Room Sentinel                                         Therapist
┌─────────────┐            ┌──────────────┐         ┌──────────────┐
│ 6-mic → VAD │──WiFi──►   │ Prosody stress│        │ Structured   │
│ → Prosody   │            │  (0-3)        │        │ report with  │
│ features    │            │ → F0 deviation│        │ 30-day trends│
│ (NO text!)  │            └──────────────┘         └──────────────┘
│ VEML → Lux  │
│ SHT40 → T/H │
└─────────────┘
```

## Intervention Loop

```
Stress detected → Hub triggers:
  1. Lighting → Light node shifts to de-stress scene (warm, dim)
  2. Breathing → Hub display shows animated breathing guide + audio
  3. Soundscape → Hub plays nature sounds via speaker
  4. Notification → Phone alert "Take a break"

Measure outcome:
  - HRV before vs after (Δ = improvement)
  - EDA SCR before vs after (Δ = reduction)
  - Efficacy score = HRV improvement / baseline

Learn over time:
  - Which interventions work for this person
  - Which times/contexts trigger stress
  - Personalize intervention selection
```

## Privacy Architecture

- **Voice:** All prosody analysis on-device (ESP32-S3). No speech transcribed, stored, or transmitted — only acoustic feature vectors and a stress classification. Physical mic mute switch.
- **Vitals:** Encrypted in transit (TLS) and at rest.
- **Data ownership:** Owner controls all sharing. Therapist access explicitly granted and revocable.
- **No third-party data sales.**
# EchoSync — Architecture

## System Overview

EchoSync is a 4-node-type IoT system for sound awareness, alert delivery,
and accessibility for the deaf and hard-of-hearing. It combines distributed
room-based acoustic monitoring (4-mic array + on-device CNN), wearable haptic
alerts, door/phone source-level detection, and a visual display hub with
bed-shaker relay for sleeping alerts.

## Node Topology

```
                    ┌─────────────┐
                    │  Mobile App │
                    └──────┬──────┘
                           │ HTTPS
                    ┌──────▼──────┐
                    │   Cloud     │ (FastAPI + MQTT + InfluxDB + PostgreSQL)
                    └──────┬──────┘
                           │ MQTT / HTTPS
                    ┌──────▼──────┐
                    │  Echo Hub   │ (ESP32-S3, Wi-Fi)
                    └──────┬──────┘
                           │ Sub-GHz 868 MHz + BLE 5.0
          ┌────────────────┼────────────────┬──────────────┐
          │                │                │              │
   ┌──────▼─────┐  ┌──────▼─────┐  ┌───────▼─────┐  ┌─────▼──────┐
   │ Room       │  │ Wrist      │  │ Door Tag    │  │ (More      │
   │ Sentinel×N│  │ Band       │  │ ×N          │  │  Sentinels)│
   │ ESP32-S3   │  │ nRF52840   │  │ nRF52840    │  │            │
   │ +SX1262    │  │ +BLE 5.0   │  │ +BLE 5.0    │  │            │
   │ 4-mic I²S  │  │ Haptic     │  │ Piezo+Mic   │  │            │
   │ SoundNet   │  │ OLED       │  │ CR2032      │  │            │
   │ DOA        │  │ LiPo       │  │ 12-month    │  │            │
   └────────────┘  └────────────┘  └────────────┘  └────────────┘
```

## Data Flow

1. **Room Sentinel** (×N, distributed in rooms) continuously samples ambient
   audio via 4-mic I²S array → on-device SoundNet CNN classifies 20+ sound
   types in <200 ms → 4-mic beamforming estimates direction-of-arrival (±15°)
   → SHT40 monitors temperature/humidity → reports sound events to Hub
   immediately via Sub-GHz 868 MHz (event-driven)

2. **Wrist Band** (wearable, wrist) receives sound alerts from Hub via BLE 5.0
   → delivers distinct haptic vibration patterns based on sound priority →
   OLED display shows sound type icon + direction → IMU detects if user is
   sleeping (suppresses non-emergency during sleep) → 3-day battery life

3. **Door Tag** (×N, mounted on doors/phones) uses piezo contact sensor to
   detect physical vibration (knock, doorbell mechanism) + MEMS microphone
   for ring-tone detection → 12-month CR2032 battery → reports to Hub via
   BLE 5.0 (proximity to hub) or through Room Sentinel relay

4. **Echo Hub** aggregates all sound events, runs local edge priority
   classification, drives RGB LED matrix display, triggers bed-shaker relay
   for sleeping alerts, forwards to cloud via MQTT, manages OTA firmware

5. **Cloud** runs full 6-model ML pipeline:
   - SoundNet: sound classification (retraining)
   - AlertPriority: priority + false-positive reduction (XGBoost)
   - SoundLocalize: direction-of-arrival refinement (SRP-PHAT + CNN)
   - SoundAnomaly: unknown sound detection (Isolation Forest)
   - PersonalSound: custom sound enrollment (Prototypical Networks)
   - DailySoundLog: event pattern analytics (LSTM + Clustering)

6. **Mobile App** receives push notifications and displays real-time sound
   event feed, daily/weekly sound history, custom sound enrollment,
   accessibility sharing with family/caregivers

## Communication

- **Sub-GHz:** 868 MHz (EU) / 915 MHz (US), LoRa SX1262, TDMA mesh
- **BLE 5.0:** Wrist Band and Door Tag (wearable/proximity nodes)
- **Hub→Cloud:** Wi-Fi/MQTT
- **Encryption:** AES-128-CCM on all radio messages
- **Range:** 300 m LOS (Sub-GHz), 15 m (BLE)
- **Max nodes:** 6 room sentinels + 8 door tags + 2 wrist bands = 16

## Power Architecture

| Node | Power | Battery | Autonomy |
|------|-------|---------|----------|
| Echo Hub | USB-C 5V | — | Continuous |
| Room Sentinel | USB-C 5V | — | Continuous |
| Wrist Band | USB-C charge | 300 mAh LiPo | 3 days |
| Door Tag | CR2032 | CR2032 | 12 months |

## Safety & Privacy

- **Privacy-first sound processing:** SoundNet CNN runs entirely on-device
  (ESP32-S3). No raw audio is transmitted — only classification results.
- **No speech transcription:** The CNN classifies environmental sounds, not
  speech content. Speech is classified only as "person entering."
- **AES-128-CCM encryption:** All radio communication encrypted.
- **Emergency redundancy:** Emergency sounds trigger all alert channels
  simultaneously — no single point of failure.
- **Sleep mode:** Wrist band IMU detects sleep; non-emergency suppressed.
- **Watchdog:** TPL5010 on battery-powered nodes for automatic recovery.
- **OTA with rollback:** Firmware updates with automatic rollback on failure.
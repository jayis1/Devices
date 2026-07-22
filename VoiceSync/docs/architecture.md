# VoiceSync — Architecture

## System Overview

VoiceSync is a 5-node-type IoT system for voice health monitoring, vocal
disorder risk prediction, and vocal wellness. It combines wearable vocal fold
vibration analysis (contact microphone), ambient voice quality classification
(on-device CNN), smart hydration tracking, environmental humidity control,
and a 6-model ML pipeline for voice disorder risk forecasting.

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
                    │  Voice Hub  │ (ESP32-S3, Wi-Fi)
                    └──────┬──────┘
                           │ Sub-GHz 868 MHz + BLE 5.0
          ┌────────────────┼────────────────┬──────────────┐
          │                │                │              │
   ┌──────▼─────┐  ┌──────▼─────┐  ┌───────▼─────┐  ┌─────▼──────┐
   │ Vocal Band │  │ Room       │  │ Hydration   │  │ Humidity   │
   │ (Wearable) │  │ Sentinel×N│  │ Tag         │  │ Node       │
   │ nRF52840   │  │ ESP32-S3   │  │ nRF52840    │  │ ESP32      │
   │ BLE 5.0    │  │ +SX1262    │  │ BLE 5.0     │  │ +SX1262    │
   │ Contact mic│  │ 4-mic I²S │  │ Load cell   │  │ SHT40      │
   │ IMU+PPG    │  │ VoiceNet  │  │ HX711       │  │ Humidifier │
   └────────────┘  └────────────┘  └────────────┘  └────────────┘
```

## Data Flow

1. **Vocal Band** (wearable, throat) continuously samples vocal fold
   vibrations through a contact microphone → extracts acoustic features
   (F0, jitter, shimmer, HNR) on-device → IMU tracks neck angle → TMP117
   detects inflammation → PPG monitors stress (HRV) → transmits to Hub
   every 30 seconds via BLE 5.0

2. **Room Sentinel** (desk/room) captures ambient voice via 4-mic I²S
   array → on-device VoiceNet CNN classifies voice quality (8 classes)
   in <300 ms → SGP40 monitors VOCs → SHT40 tracks temperature/humidity
   → reports to Hub every 2 minutes via Sub-GHz 868 MHz

3. **Hydration Tag** (water bottle) uses HX711 load cell to measure water
   mass + IMU to detect sip events → reports cumulative intake to Hub
   every 15 minutes via BLE 5.0 → 6-month CR2032 battery life

4. **Humidity Node** monitors room humidity via SHT40 → controls smart
   humidifier relay to maintain 40–60% RH → reports every 5 minutes
   via Sub-GHz 868 MHz

5. **Voice Hub** aggregates all data, runs local edge vocal health
   heuristic (Vocal Health Score, vocal dose, hydration scoring),
   forwards to cloud via MQTT, broadcasts voice status alerts,
   controls humidifier based on humidity readings

6. **Cloud** runs full 6-model ML pipeline:
   - VoiceNet: voice quality classification (retraining)
   - VocalLoad: cumulative vocal dose XGBoost
   - VoiceRisk: 7-day disorder risk LSTM
   - RefluxDetect: LPR 1D-CNN
   - HydrationModel: hydration status XGBoost
   - VocalAnomaly: Isolation Forest

7. **Mobile App** receives push notifications and displays real-time
   Vocal Health Score, 7-day risk forecast, vocal load, hydration,
   and clinical reports

## Communication

- **Sub-GHz:** 868 MHz (EU) / 915 MHz (US), LoRa SX1262, TDMA mesh
- **BLE 5.0:** Vocal Band and Hydration Tag (wearable nodes)
- **Hub→Cloud:** Wi-Fi/MQTT
- **Encryption:** AES-128-CCM on all radio messages
- **Range:** 300 m LOS (Sub-GHz), 10 m (BLE)
- **Max nodes:** 4 room sentinels + 1 humidity + 4 vocal bands + 4 hydration = 13

## Power Architecture

| Node | Power | Battery | Autonomy |
|------|-------|---------|----------|
| Voice Hub | USB-C 5V | — | Continuous |
| Vocal Band | USB-C charge | 250 mAh LiPo | 48 hours |
| Room Sentinel | USB-C 5V | — | Continuous |
| Hydration Tag | CR2032 | CR2032 | 6 months |
| Humidity Node | USB-C 5V | — | Continuous |

## Safety & Privacy

- **Privacy-first:** VoiceNet runs entirely on-device. No raw audio leaves
  the room sentinel — only extracted features and classification results.
- **Contact mic isolation:** Vocal Band contact microphone picks up vocal
  fold vibrations through tissue conduction, not airborne speech.
- **AES-128-CCM encryption:** All radio communication encrypted.
- **Clinical accuracy:** Acoustic features computed using Praat-compatible
  algorithms (jitter, shimmer, HNR).
- **OTA with rollback:** Firmware updates with automatic rollback.
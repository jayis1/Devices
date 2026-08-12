# WanderSync — Architecture

## System Overview

WanderSync is a 5-node-type dementia care IoT system that enables people
with Alzheimer's and other dementias to safely age in place. It combines a
GPS wearable band (Wander Band), motorized door/window sentinels with
auto-lock for wandering prevention, privacy-first mmWave radar room
sentinels for activity-of-daily-living recognition, a voice node for
personalized family-voice reminders, and a coordinating gateway (Care Hub)
into a complete dementia care pipeline with a 7-model ML pipeline running
across edge and cloud.

## Node Topology

```
                    ┌─────────────┐
                    │ Caregiver   │  (family + professional caregivers)
                    │ Mobile App  │
                    └──────┬──────┘
                           │ HTTPS
                    ┌──────▼──────┐
                    │   Cloud     │ (FastAPI + MQTT + InfluxDB + PostgreSQL)
                    └──────┬──────┘
                           │ MQTT / HTTPS
                    ┌──────▼──────┐
                    │ WanderSync  │ (ESP32-S3, Wi-Fi + 4G LTE backup)
                    │ Care Hub    │ (Sub-GHz 868 MHz TDMA mesh coordinator)
                    └──────┬──────┘
                           │ Sub-GHz 868 MHz TDMA Mesh (2+ km range)
          ┌────────────────┼────────────────┬──────────────┐
          │                │                │              │
   ┌──────▼─────┐  ┌──────▼─────┐  ┌───────▼─────┐  ┌─────▼──────┐
   │ Wander     │  │ Door       │  │ Room       │  │ Voice      │
   │ Band       │  │ Sentinel×N │  │ Sentinel×N │  │ Node       │
   │ nRF52840   │  │ ESP32-C3   │  │ ESP32-S3   │  │ ESP32-S3   │
   │ SX1262     │  │ SX1262     │  │ SX1262     │  │ SX1262     │
   │ L80-R GPS  │  │ Reed switch│  │ HLK-LD2410 │  │ INMP441    │
   │ LSM6DSL    │  │ Deadbolt   │  │ mmWave     │  │ I²S mic    │
   │ MAX30101   │  │ H-bridge   │  │ AM612 PIR  │  │ MAX98357A  │
   │ DRV2605L   │  │ Tamper SW  │  │ ADLNet CNN │  │ W25Q128    │
   │ SOS button │  │ CR123A×2   │  │ LiPo 1200  │  │ KeywordNet │
   │ LiPo 300   │  │ 12mo life  │  │ USB-C      │  │ LiPo 1500  │
   │ 7-day life │  │            │  │            │  │ USB-C      │
   └────────────┘  └────────────┘  └────────────┘  └───────────┘
```

## Data Flow

1. **Wander Band** samples GPS (1 Hz outdoor, 0.1 Hz indoor), IMU (50 Hz),
   PPG (15-min intervals) → WanderNet lite LSTM predicts wandering risk
   → on high risk, sends WANDER_ALERT → on fall, sends FALL_ALERT →
   SOS button sends SOS_ALERT → Sub-GHz (2+ km range)

2. **Door Sentinels** monitor door/window via reed switch → auto-lock
   motorized deadbolt when Wander Band approaches at unusual hours →
   reports door state changes + tamper to Hub

3. **Room Sentinels** use HLK-LD2410 mmWave radar for privacy-preserving
   presence + activity detection → ADLNet CNN classifies activities
   (walking, sitting, lying, eating, cooking, pacing) → sends ADL_UPDATE
   to Hub for 24-hour activity timeline → longitudinal changes feed
   CogDecline model

4. **Voice Node** receives REMINDER_TRIGGER from Hub → plays pre-recorded
   family voice reminder from W25Q128 flash → responds to keyword
   commands ("time", "help", "medicine") via on-device KeywordNet

5. **Care Hub** receives all telemetry → runs geofencing on GPS →
   coordinates door locking → schedules voice reminders → aggregates
   ADL timeline → on emergency (wander/fall/SOS), dispatches 911 via
   4G LTE with GPS coordinates + medical info

6. **Cloud** runs 7-model ML pipeline — WanderNet, ADLNet, CogDecline,
   AnomalyDetect, RoutePredict, ReminderOpt, SleepNet — generates
   neurologist-ready cognitive assessment reports, caregiver summaries,
   OTA firmware

## Communication

- **Sub-GHz 868 MHz:** SX1262 LoRa, TDMA mesh (Hub = coordinator)
- **2+ km range:** Critical for tracking wanderers beyond home perimeter
- **Hub→Cloud:** Wi-Fi/MQTT with 4G LTE cellular backup
- **Encryption:** AES-128-CTR (application layer, per-node key)
- **CRC:** CRC-16-CCITT (application layer end-to-end integrity)
- **Emergency preemption:** WANDER_ALERT/FALL_ALERT/SOS bypass TDMA (3× TX, <2s)

## Privacy Architecture

- **No cameras** anywhere in the system
- **No audio recording** — Voice Node processes keywords on-device, no audio leaves
- **mmWave radar** detects presence + activity without imaging (no visual data)
- **GPS data** stays on-device + encrypted cloud (no third-party sharing)
- **Family voice clips** stored in local flash, not cloud
- **Data sharing** is opt-in only (neurologist access)
- **All communication** encrypted with AES-128-CTR per-node keys

## Fire Safety Compliance

- Door Sentinels auto-unlock during daytime (configurable schedule)
- Interior thumbturn always works (motorized bolt doesn't block manual unlock)
- On fire alert, all doors immediately unlock (emergency_unlock command)
- On power loss, doors can always be manually unlocked from inside
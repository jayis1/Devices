# MosquitoSync — Architecture

## System Overview

MosquitoSync is a 5-node-type IoT system for mosquito detection, trapping, and
disease-risk prevention. It combines acoustic species identification (on-device
CNN), CO2 lure trapping with capture counting, motorized window barriers, and
a 6-model ML pipeline for activity forecasting and disease risk prediction.

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
                    │    Hub      │ (ESP32-S3, Wi-Fi + 4G LTE backup)
                    └──────┬──────┘
                           │ Sub-GHz 868 MHz TDMA Mesh
          ┌────────────────┼────────────────┬──────────────┐
          │                │                │              │
   ┌──────▼─────┐  ┌──────▼─────┐  ┌───────▼─────┐  ┌─────▼──────┐
   │ Acoustic   │  │ CO2 Trap   │  │ Window      │  │ Weather   │
   │ Sentinel×N │  │ ×1-3       │  │ Barrier ×N  │  │ Sentinel  │
   │ ESP32-S3   │  │ ESP32-S3   │  │ ESP32       │  │ nRF52840  │
   │ WingNet CNN│  │ CO2+heat   │  │ Motorized   │  │ BME280    │
   │ 4-mic I²S  │  │ IR+camera  │  │ screen      │  │ Rain+wind│
   └────────────┘  └────────────┘  └────────────┘  └────────────┘
```

## Data Flow

1. **Acoustic Sentinels** (indoor) continuously record 1-second audio from
   4-mic I²S arrays → on-device WingNet CNN classifies species by wingbeat
   frequency → detection events sent to Hub immediately

2. **CO2 Trap Nodes** (outdoor) generate CO2 from propane + heat (37°C) +
   octenol lure → IR beam counts insect entries → camera captures images
   every 15 min → telemetry to Hub every 15 min

3. **Window Barriers** close within 2 seconds of acoustic detection or hub
   command → reed switches confirm position → auto-open after 30 min

4. **Weather Sentinel** reports temperature, humidity, rainfall, wind every
   5 min (critical for mosquito activity forecasting — rain creates breeding
   sites 7–14 days later, peak activity at 27°C)

5. **Hub** aggregates all data, runs local BiteRisk heuristic, forwards to
   cloud via MQTT, uses 4G LTE cellular backup when Wi-Fi down

6. **Cloud** runs full 6-model ML pipeline:
   - WingNet: species classification (retraining)
   - ActivityForecast: 72-hour activity LSTM
   - DiseaseRisk: dengue/West Nile/malaria XGBoost ensemble
   - BiteRisk: personal bite risk XGBoost
   - CaptureCount: trap capture U-Net-tiny
   - SensorAnomaly: Isolation Forest

7. **Mobile App** receives push notifications and displays real-time
   BiteRisk Score, disease risk, 72-hour forecast, and species breakdown

## Communication

- **Sub-GHz:** 868 MHz (EU) / 915 MHz (US), LoRa SX1262, TDMA mesh
- **Hub→Cloud:** Wi-Fi/MQTT with 4G LTE backup
- **Encryption:** AES-128-CCM on all radio messages
- **Range:** 300 m LOS, up to 2 km with mesh relay
- **Max nodes:** 8 acoustic + 3 traps + 12 barriers + 1 weather = 24

## Power Architecture

| Node | Power | Battery | Solar | Autonomy |
|------|-------|---------|-------|----------|
| Hub | USB-C/PoE | — | — | Continuous |
| Acoustic | USB/Solar | 1200 mAh LiPo | 3W | 30+ days |
| CO2 Trap | Solar | 5000 mAh LiFePO4 | 10W | 14 days |
| Barrier | Solar | 2000 mAh LiPo | 2W | 90+ days |
| Weather | Solar | 3000 mAh LiFePO4 | 5W | 30 days |

## Safety

- **CO2 Trap:** Propane leak detection (MQ-4), overheat shutoff, trap full,
  rain pause, watchdog (TPL5010)
- **Window Barrier:** Motor stall (anti-pinch), limit switches, manual
  override, auto-open timeout, watchdog
- **Data:** SD card buffering (14-day), cellular backup, mesh relay,
  OTA with rollback, CRC + AES-128-CCM
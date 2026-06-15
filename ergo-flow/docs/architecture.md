# ErgoFlow — Architecture Document

## System Overview

ErgoFlow is a 5-node distributed embedded system that monitors, analyzes, and optimizes workspace ergonomics in real-time. The system consists of:

1. **Hub Node** (nRF5340 + ESP32-C6) — Central coordinator, mmWave pose detection, ML inference, lighting control
2. **Chair Pad Node** (nRF52832) — Pressure mapping, IMU tilt detection
3. **Desk Controller Node** (STM32G070CB + nRF52810) — Motor control, lighting, monitor tilt
4. **Wearable Tag Node** (nRF52833) — Wrist-worn IMU, activity classification, heart rate
5. **Cloud/Edge** (FastAPI + MQTT) — Dashboard, ML pipeline, mobile app backend

## Block Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                         CLOUD                                    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │  FastAPI      │  │  MQTT Broker │  │  ML Pipeline         │  │
│  │  REST API     │◄►│  (Mosquitto) │◄►│  PostureNet CNN      │  │
│  │  WebSocket    │  │              │  │  RSI-Risk LSTM        │  │
│  └──────┬───────┘  └──────┬───────┘  │  FocusNet TCN         │  │
│         │                  │          │  CircadianLight GBT    │  │
│         │                  │          │  ActivityNet CNN       │  │
│  ┌──────┴───────┐         │          └──────────────────────┘  │
│  │  PostgreSQL   │         │                                    │
│  │  Time-series  │         │          ┌──────────────────────┐  │
│  └──────────────┘         │          │  Mobile App           │  │
│                           │          │  React Native          │  │
│                           └─────────►│  BLE ↔ Hub            │  │
│                                      │  REST ↔ API            │  │
│                                      │  Push notifications    │  │
│                                      └──────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
                              │ WiFi6 + BLE
                              │
┌──────────────────────────────────────────────────────────────────┐
│  HUB NODE (nRF5340 + ESP32-C6)                                  │
│  ┌──────────┐  ┌───────────┐  ┌───────────┐  ┌───────────────┐  │
│  │ nRF5340  │  │ ESP32-C6  │  │ BGT60TR13C│  │ Peripherals   │  │
│  │ BLE mesh │◄►│ WiFi/BLE  │  │ mmWave    │  │ OLED, Audio   │  │
│  │ coord.   │  │ uplink    │  │ 60GHz     │  │ Light, Sensors│  │
│  └────┬─────┘  └───────────┘  └───────────┘  └───────────────┘  │
│       │                                                          │
│       │ BLE mesh 2.4GHz                                         │
│       │                                                          │
└───────┼──────────────────────────────────────────────────────────┘
        │
   ┌────┼────────────────────────────────────────┐
   │    │          BLE MESH                       │
   │    │                                          │
   │  ┌─┴──────────┐  ┌──────────────┐  ┌─────────┴────────┐
   │  │ CHAIR PAD  │  │ DESK CTRL    │  │ WEARABLE TAG      │
   │  │ nRF52832   │  │ STM32G070    │  │ nRF52833          │
   │  │ FSR×16     │  │ + nRF52810   │  │ ICM-42688-P       │
   │  │ LSM6DSOX   │  │ DRV8871×2    │  │ MMC5603            │
   │  │ ADS1115    │  │ Hall×2       │  │ MAX30101           │
   │  │ Mux×2      │  │ WS2812B strip│  │ ERM motor          │
   │  │ DRV2605L   │  │ PCA9685      │  │ LiPo 150mAh       │
   │  │ LiPo 1Ah   │  │ SG90 servo   │  └────────────────────┘
   │  └────────────┘  │ LiPo —       │
   │                   │ 12V 5A PSU   │
   │                   └──────────────┘
   └────────────────────────────────────────────┘
```

## Data Flow

### Real-time Posture Monitoring (500ms cycle)

1. Chair pad scans FSR grid (5Hz) → BLE mesh → Hub
2. Wearable tag reads IMU (100Hz) → local activity classification → BLE mesh → Hub
3. Hub receives pressure map + activity → PostureNet inference → posture score
4. Hub broadcasts posture score → all nodes → mobile app
5. If poor posture detected >30s: haptic alert → wearable tag + chair pad
6. If RSI risk rising: break reminder → haptic + audio → mobile app push

### Desk Control Flow

1. Hub calculates optimal desk height (sit/stand transition every 30min)
2. Hub sends DESK_COMMAND → BLE mesh → desk controller
3. Desk controller PID loop → motor → Hall sensor feedback
4. Desk controller sends DESK_STATUS → BLE mesh → Hub → cloud

### Circadian Lighting Flow

1. Hub reads ambient light (TSL2591) + time of day
2. CircadianLight model calculates optimal RGBW
3. Hub sends LIGHTING_CMD → BLE mesh → desk controller
4. Desk controller drives WS2812B strip via LP5562

## Security

- BLE mesh: AES-CCM 128-bit encryption, OOB numeric provisioning
- WiFi: WPA3, TLS 1.3 for MQTT
- API: JWT authentication, rate limiting
- OTA: SHA256 verification, signed firmware images
- No camera images stored — mmWave radar is privacy-first
- Health data encrypted at rest (AES-256)

## Scalability

- One hub supports up to 8 BLE mesh nodes (current: 4)
- MQTT broker handles 1000+ concurrent connections
- FastAPI backend horizontally scalable
- PostgreSQL time-series partitioned by week
- ML inference on-device (hub) for <15ms latency
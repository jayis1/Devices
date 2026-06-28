# PestSync — Architecture

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        PestSync System                                │
│                                                                      │
│  ┌──────────────┐  Sub-GHz 868  ┌──────────┐  Sub-GHz 868  ┌──────┐ │
│  │ Pest Sentinel│───────────────│   Hub    │──────────────│ Smart │ │
│  │ (ESP32-S3)   │               │ (ESP32)  │              │ Trap  │ │
│  │ OV2640+PIR   │               │ LoRa+BLE │              │(C3)  │ │
│  │ MLX90640     │               │ WiFi+SD  │              │ Reed │ │
│  │ YOLOv8-nano  │               └──────────┘              │ +LC  │ │
│  │ IR illum.    │      BLE         │                      └──────┘ │
│  └──────────────┘  ┌──────▼──────┐    Sub-GHz 868  ┌─────────────┐  │
│                    │ Mobile App  │───────────────│ Deterrent   │  │
│  ┌──────────────┐  │ (React      │                 │ (ESP32-C3)  │  │
│  │ Pest Sentinel│  │  Native)    │  Cloud (FastAPI │ US+Strobe   │  │
│  │ #2..#N       │  └─────────────┘  + MQTT + ML)   │ +Diffuser   │  │
│  └──────────────┘                                    └─────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

## Node Architecture

### Hub (ESP32-WROOM-32E)
- **Role**: Gateway coordinator, edge ML, cloud relay, mobile bridge
- **Comms**: Sub-GHz 868 MHz (SX1262), BLE 5.0 (NimBLE), WiFi
- **Tasks**: TDMA mesh coordinator, BLE GATT server, MQTT client, edge ML, OLED display, SD logger
- **Power**: USB-C primary + 18650 backup + solar trickle

### Pest Sentinel (ESP32-S3-N8R2)
- **Role**: AI pest detection camera
- **Comms**: Sub-GHz 868 MHz (SX1262), WiFi (config/OTA only)
- **Sensors**: OV2640 camera, MLX90640 thermal array, AM312 PIR, 850nm IR LEDs
- **ML**: On-device YOLOv8-nano (int8 quantized, 4MB in PSRAM)
- **Power**: 18650 battery, 14-day runtime (PIR-triggered)

### Smart Trap (ESP32-C3)
- **Role**: Catch detection & verification
- **Comms**: Sub-GHz 868 MHz (SX1262)
- **Sensors**: Reed switch (trap fire), HX711 + 50g load cell (catch weight), capacitive bait sensor, ADXL362 (tamper)
- **Power**: 2× AA alkaline, 6-12 month runtime (event-driven)
- **Retrofits**: Existing snap traps, glue boards, electronic traps

### Deterrent Node (ESP32-C3)
- **Role**: Adaptive multi-modal pest deterrence
- **Comms**: Sub-GHz 868 MHz (SX1262)
- **Actuators**: 2× piezo ultrasonic (20-65 kHz sweep), white strobe LED, piezo essential-oil diffuser
- **Power**: USB-C + 18650, 21-day runtime
- **Modes**: Off, Schedule, Adaptive (learns from sentinel activity), Always-on

## Communication Protocol

### Sub-GHz (PestSync Protocol — PSP)
- **Physical**: LoRa 868 MHz, SX1262, SF11, BW 125 kHz, 17 dBm
- **MAC**: TDMA mesh, 8-slot frame (8s), Hub is coordinator
- **Encryption**: AES-128-CCM
- **Range**: ~500m outdoor, penetrates walls/floors

### BLE 5.0
- Hub ↔ Mobile App: GATT server, JSON over characteristics
- Used for commissioning, live view, local access when WiFi down

### WiFi
- Hub ↔ Cloud: MQTT over TLS
- Pest Sentinel: Initial configuration + OTA firmware updates only

## Cloud Architecture

- **FastAPI** backend with async PostgreSQL/TimescaleDB
- **MQTT** broker (Mosquitto) for real-time ingestion
- **ML Pipeline**: XGBoost (infestation risk), LSTM (activity patterns), Logistic Regression (deterrent effectiveness), YOLOv8 (pest detection), MobileNetV3 (mobile pest ID)
- **Push notifications**: Firebase Cloud Messaging
- **Storage**: Time-series telemetry (hypertables), detection events, trap events, deterrent status
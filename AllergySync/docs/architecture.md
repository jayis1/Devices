# AllergySync — Architecture

## System Overview

AllergySync is a four-node mesh system for seasonal allergy management:

```
                          ┌──────────────┐
                          │   Cloud /     │
                          │  FastAPI +    │
                          │   MQTT        │
                          └──────┬───────┘
                                 │ MQTT/TLS over Wi-Fi
                          ┌──────┴───────┐
                          │  AllergySync │
                          │     Hub      │
                          │ (ESP32-S3)   │
                          └──┬───┬───┬───┘
             Sub-GHz 868 MHz  │   │   │  TDMA mesh
                 ┌────────────┘   │   └────────────┐
          ┌──────┴──────┐  ┌─────┴──────┐  ┌───────┴───────┐
          │ Room Sentinel│  │ Window Node│  │ Wearable Tag  │
          │ (ESP32-S3)   │  │ (nRF52840) │  │ (nRF52840)    │
          └──────────────┘  └────────────┘  └───────────────┘
```

## Data Flow

### Sensor → Cloud Path
1. **Room Sentinel** samples PM/CO₂/VOC at 1 Hz, runs PollenNet CNN every 10 s
2. Sentinel aggregates 1-min averages, transmits to Hub every 5 min via Sub-GHz
3. **Wearable Tag** samples personal PM every 5 min, transmits via Sub-GHz (or BLE to phone when outdoors)
4. **Hub** receives telemetry, runs local decision engine, forwards to cloud via MQTT/TLS
5. Cloud backend stores in PostgreSQL, runs ML inference (PollenForecast, SymptomPredict, AllergenSensitivity)
6. Mobile app polls REST API or receives WebSocket push

### Command Path (Cloud → Actuator)
1. Cloud or local decision engine determines action (e.g., "close windows")
2. Hub sends command packet via Sub-GHz to Window Node
3. Window Node actuates stepper motor, returns ACK
4. State propagated to cloud and mobile app

## Hardware Architecture

### Hub (ESP32-S3-WROOM-1-N16R8)
- **MCU:** Dual-core Xtensa LX7 @ 240 MHz, 16 MB flash, 8 MB PSRAM
- **Radio:** LR1121 (Sub-GHz 868 MHz, FSK 50 kbps, +22 dBm max)
- **Power:** USB-C 5V/1A or PoE 802.3af
- **Storage:** SPIFFS for 30-day exposure database

### Room Sentinel (ESP32-S3-WROOM-1-N16R8)
- **MCU:** Same as Hub
- **PM Sensor:** Sensirion SPS30 (laser scattering, 0.3–10 µm, 1 Hz)
- **CO₂:** Sensirion SCD41 (photoacoustic, ±40 ppm)
- **VOC:** Bosch BME688 (gas scanner, VOC index 0-500)
- **Temp:** TMP117 (±0.1°C)
- **Fan:** 50mm 5V brushless for active sampling

### Window Node (nRF52840)
- **MCU:** ARM Cortex-M4F @ 64 MHz, 1 MB flash, 256 KB RAM
- **Radio:** LR1121 (Sub-GHz 868 MHz)
- **Actuator:** NEMA17 stepper + TMC2209 driver (2000 steps full open)
- **Sensors:** Reed switch (window state), VEML7700 (light), INA260 (battery)
- **Power:** 4× AA NiMH (2000 mAh) or USB-C

### Wearable Tag (nRF52840)
- **MCU:** Same as Window Node
- **PM Sensor:** Plantower PMSA003I (mini laser, I²C/UART)
- **IMU:** BMI270 (6-axis, 50 Hz)
- **Radio:** LR1121 Sub-GHz + BLE 5.0
- **Power:** CR2032 (220 mAh), 9-month battery life
- **Duty cycle:** PM sensor 8s active / 5 min sleep

## Communication Protocol

### Sub-GHz TDMA Mesh (868 MHz)
- **Modulation:** FSK, 50 kbps, 12.5 kHz BW
- **TDMA:** 12 slots × 500 ms = 6 s frame
- **Slot 0:** Hub beacon (time sync + slot assignments)
- **Slots 1-11:** Node data slots
- **Mesh:** Up to 4 hops, AES-128-CCM encryption
- **Sync word:** 0xA51E9C47 (4 bytes)
- **Range:** 200 m line-of-sight

### BLE 5.0 (Wearable Tag ↔ Phone)
- GATT service for exposure data
- Used when wearable is out of Sub-GHz range
- Advertising interval: 20-40 ms (connectable)

### MQTT/TLS (Hub ↔ Cloud)
- QoS 1 (at-least-once delivery)
- Topics:
  - `allergysync/telemetry/node/{id}` — sensor data
  - `allergysync/cmd/hub` — cloud commands
  - `allergysync/ota/{node_id}` — firmware updates
- Auto-reconnect with exponential backoff

## Software Architecture

### Cloud Backend (FastAPI)
- **MQTT listener:** aiomqtt async loop
- **REST API:** 12 endpoints (see API spec)
- **WebSocket:** Real-time push to mobile app
- **Database:** PostgreSQL (asyncpg)
- **Cache:** Redis for latest state
- **ML inference:** Called on demand for forecasts

### ML Pipeline (6 models)
1. **PollenNet** — 1D-CNN, on-device (ESP32-S3, tflite-micro)
2. **PollenForecast** — LSTM, cloud, 24-h pollen forecast
3. **SymptomPredict** — XGBoost, cloud, 12-h symptom forecast
4. **AllergenSensitivity** — Bayesian logistic regression, cloud, personal allergen profile
5. **ActivityCNN** — TinyCNN, on-device (nRF52840, int8 quantized)
6. **AnomalyDetector** — Isolation Forest, cloud, pollen spike detection

### Mobile App (React Native)
- Tab navigation: Dashboard, Symptoms, Insights, Settings
- REST API polling (60s refresh) + WebSocket for real-time
- Expo for OTA app updates

## Power Budget

| Node | Active Current | Sleep Current | Battery | Estimated Life |
|------|---------------|---------------|---------|----------------|
| Hub | ~200 mA | N/A (always on) | USB/PoE | Continuous |
| Room Sentinel | ~350 mA | N/A (always on) | USB | Continuous |
| Window Node | ~150 mA (motor) / 15 mA (idle) | 5 µA | 4× AA NiMH 2000mAh | 3 months |
| Wearable Tag | ~30 mA (PM active) / 8 mA (BLE) | 3 µA | CR2032 220mAh | 9 months |

## Security

- **Mesh:** AES-128-CCM with per-node session keys (ECDH P-256 key exchange)
- **Cloud:** MQTT/TLS with client certificates
- **App:** OAuth2 + JWT
- **OTA:** Signed firmware images (Ed25519)
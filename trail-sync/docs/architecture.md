# TrailSync — Architecture

## System Overview

TrailSync is a 4-node wearable + trail infrastructure system for trail running and outdoor adventure safety. It continuously monitors biomechanics, navigation, and environment to detect injury risk, altitude sickness, storms, and falls — with emergency SOS coordination via LoRa mesh through trail beacons.

## Nodes

### 1. Wrist Unit
- **MCU:** nRF52832
- **Sensors:** PPG (MAX30101), barometric altimeter (BMP390), GPS (SAM-M10Q), IMU (LSM6DSL), skin temp (TMP117)
- **Radios:** Sub-GHz (SX1262) + LoRa (RFM95W) + BLE 5.3
- **Role:** Command center — fall detection, altitude sickness screening, storm prediction, navigation display, SOS trigger
- **Power:** 18650 LiFePO4 2600mAh (72-hr continuous)
- **Firmware:** nRF5 SDK, C

### 2. Shoe Pod (×2 — left + right)
- **MCU:** nRF52833
- **Sensors:** IMU (LSM6DSL), 24× FSR pressure insole, HX711 strain gauge
- **Radio:** Sub-GHz (SX1262)
- **Role:** Biomechanics lab — gait analysis at 200 Hz, injury prediction
- **Power:** CR2477 coin cell (30+ days)
- **Firmware:** nRF5 SDK, C

### 3. Trail Beacon (×N — community-maintained)
- **MCU:** nRF52833
- **Sensors:** BME280 (temp/humidity/pressure), PIR (AM312), optional PM2.5 (PMS5003)
- **Radios:** Sub-GHz (SX1262) + LoRa (RFM95W)
- **Role:** Trail intelligence — conditions broadcast, LoRa SOS relay, GPS reference point
- **Power:** 5W solar + 18650 LiFePO4 (2-year life)
- **Firmware:** nRF5 SDK, C

### 4. Hub (home/vehicle)
- **MCU:** ESP32-S3
- **Display:** 4.0" TFT (ILI9488)
- **Radios:** Sub-GHz (SX1262) + LoRa (RFM95W) + WiFi6 + BLE
- **Role:** Coordinator — group tracker, SOS relay, route planner, cloud bridge
- **Power:** 5V USB-C + 18650 backup (8-hr outage)
- **Firmware:** ESP-IDF, C

## Communication

| Link | Protocol | Frequency | Range | Power |
|------|----------|-----------|-------|-------|
| Shoe Pod → Wrist | Sub-GHz | 868/915 MHz | 50m | Low (coin cell) |
| Wrist ↔ Beacon | Sub-GHz + LoRa | 868/915 MHz | 50m / 5-15km | Medium |
| Wrist ↔ Hub | Sub-GHz | 868/915 MHz | 100m | Medium |
| Beacon ↔ Beacon | LoRa mesh | 868/915 MHz | 5-15km | Low (solar) |
| Hub → Cloud | WiFi6 | 2.4/5 GHz | Infrastructure | High |
| Wrist → App | BLE 5.3 | 2.4 GHz | 10m | Low |

## Data Flow

```
Shoe Pod (200 Hz gait) ──Sub-GHz──► Wrist Unit
                                       │
Wrist Unit ──Sub-GHz──► Hub ──WiFi──► Cloud (MQTT)
    │                       │
    └──BLE──► Mobile App    │
                            │
Trail Beacon ──LoRa──► Hub ──WiFi──► Cloud
    │                     │
    └──Sub-GHz──► Wrist   └──LoRa relay──► Emergency Services
```

## ML Pipeline

| Model | Input | Output | Platform |
|-------|-------|--------|----------|
| Gait LSTM | 2s IMU + pressure windows | Gait class + terrain + metrics | Cloud (full), Edge (TFLite binary) |
| Injury Risk | 7-day gait + HRV + training load | 12 injury risk scores | Cloud |
| Altitude Sickness | 6hr SpO2 + HRV + ascent | AMS/HACE risk (3-class) | Cloud + Edge (TFLite) |
| Storm Predictor | 3hr pressure + temp + humidity | Storm risk (3-class) | Cloud + Edge (TFLite) |
| Terrain CNN | Shoe pod IMU patterns | 8 terrain classes | Edge (TFLite Micro <150KB) |

## SOS Flow

1. **Trigger:** Manual (button hold 5s) or auto (fall + 10s stillness) or altitude (SpO2 < 88%)
2. **Wrist Unit:** Assembles SOS packet (GPS, HR, SpO2, HRV, injury type)
3. **LoRa TX:** Broadcasts to all beacons in range (5-15km)
4. **Beacon Relay:** Mesh-relays toward nearest hub with cell coverage
5. **Hub:** Receives SOS via LoRa, relays to cloud via WiFi/cellular
6. **Cloud:** Notifies emergency contacts, dispatches rescue, provides GPS + vitals
7. **ACK:** Hub sends SOS_ACK back through LoRa mesh to runner's Wrist Unit
8. **Runner:** Sees "SOS received — help is coming" on Wrist Unit OLED

## Power Budget

| Node | Active Current | Sleep Current | Battery | Life |
|------|---------------|---------------|---------|------|
| Wrist Unit | ~50 mA (GPS + PPG + Sub-GHz) | ~1 mA (GPS off) | 2600 mAh | 72 hr continuous |
| Shoe Pod | ~15 mA (200 Hz sampling + TX) | ~3 µA (deep sleep) | 1000 mAh (CR2477) | 30 days |
| Trail Beacon | ~25 mA (Sub-GHz + LoRa TX) | ~50 µA (sleep between beacons) | 1500 mAh (solar) | 2 years |
| Hub | ~300 mA (WiFi + display + Sub-GHz) | ~10 mA (sleep) | 3500 mAh | 8 hr (battery) |
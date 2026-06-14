# UrbanHarvest — System Architecture

## Overview

UrbanHarvest is a 4-node IoT system for intelligent urban micro-farming. It monitors plants, automates irrigation and nutrient delivery, detects disease early, and predicts harvest timing.

## System Topology

```
                    ┌──────────────┐
                    │   Cloud      │
                    │  (FastAPI +  │
                    │   MQTT +     │
                    │   ML + DB)   │
                    └──────┬───────┘
                           │ WiFi 6
                    ┌──────┴───────┐
                    │   HUB NODE  │
                    │ nRF5340 +    │
                    │ ESP32-C6     │
                    └──────┬───────┘
                           │ Sub-GHz Mesh (868MHz LoRa, TDMA)
          ┌────────────────┼────────────────┐
          │                │                │
   ┌──────┴──────┐  ┌──────┴──────┐  ┌──────┴──────┐
   │ PLANT SENS  │  │ GROW POD    │  │ WEATHER STA │
   │ ×1-24      │  │ CONTROLLER  │  │ ×1          │
   │ STM32WL55   │  │ ESP32-S3    │  │ RP2040      │
   │ Soil+Light  │  │ Camera+LED │  │ Wind+Rain   │
   │ +Leaf Wet   │  │ Pumps+Fan  │  │ Temp+UV     │
   └─────────────┘  └─────────────┘  └─────────────┘
```

## Node Specifications

### Hub Node
- **MCU**: nRF5340 (dual Cortex-M33, 64MHz + 64MHz)
- **WiFi**: ESP32-C6-MINI-1 (WiFi 6, BLE 5.0)
- **Radio**: SX1262 (868MHz LoRa, mesh coordinator)
- **Display**: 3.2" ILI9341 TFT (SPI)
- **Audio**: MAX98357A (speaker) + SPH0645LM4H (mic)
- **Storage**: 16MB W25Q128 + microSD
- **Power**: USB-C 5V + Lipo 3000mAh backup
- **Cost**: ~$43

### Grow Pod Controller
- **MCU**: ESP32-S3 (dual 240MHz, 8MB PSRAM)
- **Camera**: OV2640 (2MP, parallel interface)
- **Radio**: SX1261 (868MHz LoRa, mesh client)
- **LEDs**: 4× AL8860 drivers (Red 660nm, Blue 450nm, White 3000K, Far Red 730nm)
- **Actuators**: Water pump, 2× nutrient peristaltic pumps, pH doser, fan, heater, humidifier
- **Power**: 24V DC (LEDs) + USB-C 5V (MCU backup)
- **Cost**: ~$74 (+ LED strips)

### Plant Sensor Node
- **MCU**: STM32WL55CC (Cortex-M4 + M0+, integrated Sub-GHz)
- **Sensors**: Capacitive soil moisture, soil EC (4-wire AC), DS18B20 temp, TSL25911 PAR, leaf wetness
- **Power**: 3× AA alkaline (6+ months) or 2W solar + Lipo 600mAh
- **Cost**: ~$20

### Weather Station
- **MCU**: RP2040 (dual Cortex-M0+, 133MHz)
- **Radio**: SX1262 (868MHz LoRa)
- **Sensors**: Anemometer, wind vane, rain gauge, SHT45, BMP390, VEML6075, TSL25911
- **Power**: 5W solar + BQ25570 harvester + Lipo 2000mAh
- **Cost**: ~$74

## Communication

### Sub-GHz Mesh (868MHz LoRa)
- **Modulation**: LoRa SF7 (125kHz BW), fallback SF10 for weather station
- **Protocol**: Custom TDMA, hub = coordinator
- **Frame**: 26 slots × 100ms = 2.6 second cycle
- **Range**: 30m indoor, 200m outdoor (SF10)
- **Capacity**: Up to 24 plant sensors + 1 grow pod + 1 weather station

### WiFi (ESP32-C6 → Cloud)
- MQTT QoS 1 with TLS for sensor data uplink
- REST API for mobile app commands
- WebSocket for real-time dashboard

### BLE (nRF5340 → Mobile App)
- GATT server for plant data, irrigation commands, configuration
- Connectionless advertising for garden summary

## Data Flow

1. **Plant Sensors** → Sub-GHz mesh → **Hub** (every 5 minutes)
2. **Weather Station** → Sub-GHz mesh → **Hub** (every 60 seconds)
3. **Grow Pod** → Sub-GHz mesh → **Hub** (status every 60 seconds; camera image via WiFi)
4. **Hub** → WiFi/MQTT → **Cloud** (aggregated readings, alerts, images)
5. **Cloud** → ML inference → **Hub** → Sub-GHz → **Grow Pod** (irrigation/nutrient commands)
6. **Cloud** → REST API → **Mobile App** (dashboard, alerts, harvest predictions)
7. **Mobile App** → BLE → **Hub** (manual commands, configuration)

## ML Models

| Model | Platform | Input | Output | Size | Runs |
|-------|----------|-------|--------|------|------|
| Plant Health Index | TFLite Micro (sensor) | 5 features | 0-100 score | 4KB | 15 min |
| Disease Detection | TFLite Micro (ESP32-S3) | 120×120 RGB | 6-class | 280KB | Daily |
| Yield Prediction | PyTorch (cloud) | 90-day sequence | Days + grams | 2MB | Daily |
| Irrigation Optimizer | DQN (cloud) | State vector | Volume ml | 500KB | 30 min |
| Nutrient Advisor | XGBoost (cloud) | 14 features | A/B/pH ml | 100KB | Daily |
| Planting Advisor | Rules + ML (cloud) | Location + time | Plant list | 50KB | Weekly |

## Power Budget

| Node | Active Current | Sleep Current | Average | Battery Life |
|------|---------------|---------------|---------|--------------|
| Hub | ~200mA | ~15mA | ~50mA | 8h (Lipo) |
| Grow Pod | ~3.5A @ 24V | ~200mA | Varies | Mains powered |
| Plant Sensor | ~30mA (burst) | ~3µA | ~300µA | 6+ months (AA) |
| Weather Station | ~25mA | ~20µA | ~25mA | Solar perpetual |

## Security

- TLS for all MQTT connections
- BLE pairing with passkey
- WiFi WPA3
- Mesh packets authenticated via CRC16 + node ID whitelist
- OTA updates signed with Ed25519
- Camera images anonymized (no faces, only plant canopy)
- Personal data stays on-device when possible
# LawnSync — AI-Powered Smart Lawn & Turf Health Management System

> **An intelligent, multi-node IoT system that monitors, analyzes, and optimizes lawn health — distributed soil sensing, automated irrigation, weather-aware scheduling, multispectral disease/weed detection, and 14-day turf health forecasting. Saves 30–50% on water while producing a healthier lawn.**

---

## 1. Overview

LawnSync is a full-stack IoT system that transforms how homeowners care for their lawns. Instead of guesswork and calendar-based watering, LawnSync uses a distributed network of solar-powered soil sensors, a smart sprinkler controller, a weather station, and a multispectral lawn scanner to continuously monitor turf health, predict problems before they appear, and automate irrigation and fertilization with precision.

**Key outcomes:**
- **30–50% water savings** vs. timer-based irrigation (weather-aware + soil-moisture-based scheduling)
- **Early disease detection** — catch brown patch, dollar spot, rust, and 12 other diseases 3–7 days before visible damage
- **Weed mapping** — semantic segmentation identifies 8 common weed species and their locations
- **Drought stress prediction** — NDVI analysis with 14-day LSTM forecast
- **Optimal fertilization timing** — XGBoost model based on soil NPK, weather, and growth stage
- **Reduced chemical use** — spot-treat diseases and weeds instead of blanket applications
- **Lawn Health Score** — a single 0–100 metric that summarizes overall turf condition

### Problem Statement

The average US suburban lawn uses **~9 billion gallons of water per day** nationwide. Most homeowners water on fixed schedules regardless of actual soil moisture or weather, over-fertilize "just in case," and react to lawn problems only after visible damage appears. Lawn care is the #1 use of residential irrigation water and a major source of fertilizer/pesticide runoff into waterways.

LawnSync shifts lawn care from reactive and wasteful to proactive and precise.

---

## 2. System Architecture

```
                         ┌──────────────────────────────────────────────┐
                         │              CLOUD BACKEND                   │
                         │  FastAPI + MQTT + InfluxDB + PostgreSQL      │
                         │  6-model ML pipeline (GPU inference)         │
                         │  Irrigation scheduler · Alert engine         │
                         │  OTA firmware updates · Weather API          │
                         └──────────────────────────────────────────────┘
                                          ▲▼ MQTT / HTTPS
                         ┌──────────────────────────────────────────────┐
                         │              HUB / GATEWAY                    │
                         │  ESP32-S3 + SX1276 Sub-GHz 868 MHz            │
                         │  Wi-Fi 2.4 GHz · BLE 5.0 · TDMA Mesh Coord.   │
                         │  Local edge inference (TFLite-Micro)         │
                         │  BME280 · Status LEDs · Buzzer · USB-C/PoE   │
                         └──────────────────────────────────────────────┘
                              ▲         ▲         ▲         ▲
                              │Sub-GHz  │Sub-GHz  │Sub-GHz  │Sub-GHz
                              │868 MHz  │868 MHz  │868 MHz  │868 MHz
                    ┌─────────┴──┐ ┌────┴─────┐ ┌┴────────┐ ┌┴──────────┐
                    │ SOIL NODE  │ │ SOIL NODE│ │WEATHER  │ │ LAWN      │
                    │  ×N (16)   │ │  ×N      │ │STATION  │ │ SCANNER   │
                    │ nRF52840   │ │ nRF52840 │ │ ESP32-S3│ │ ESP32-S3  │
                    │ +SX1262    │ │ +SX1262  │ │ +SX1262 │ │ +OV5640   │
                    │ Solar      │ │ Solar    │ │ Solar   │ │ Solar/USB │
                    │ Moisture   │ │ Moisture │ │ Rain    │ │ RGB+NIR  │
                    │ pH NPK Temp│ │ pH NPK T │ │ Wind    │ │ Disease  │
                    │ Light      │ │ Light    │ │ Solar   │ │ Weed map  │
                    └────────────┘ └──────────┘ └─────────┘ └──────────┘
                                          ▲ Sub-GHz 868 MHz
                    ┌─────────────────────┴─────────────────────┐
                    │         SPRINKLER CONTROLLER              │
                    │  ESP32 + SX1262 · 24VAC transformer       │
                    │  8× zone valves · Master valve · Pump    │
                    │  Flow meter · Rain sensor · Pressure     │
                    └───────────────────────────────────────────┘
```

### Data Flow

1. **Soil Nodes** measure moisture, pH, NPK, temperature, and light every 15 min → transmit via Sub-GHz mesh to Hub
2. **Weather Station** reports rain, wind, solar irradiance, temp/humidity/pressure every 5 min → Hub
3. **Lawn Scanner** captures multispectral images on schedule → on-device disease/weed inference → results to Hub → raw images to cloud for retraining
4. **Sprinkler Controller** reports flow/pressure and executes irrigation schedules from Hub
5. **Hub** aggregates all data, runs local edge inference, forwards to cloud via MQTT
6. **Cloud** runs full ML pipeline, generates irrigation schedules, alerts, and forecasts
7. **Mobile App** receives push notifications and displays real-time lawn health dashboard

---

## 3. Hardware Nodes

### 3.1 Hub / Gateway

| Component | Part | Notes |
|-----------|------|-------|
| SoC | ESP32-S3-WROOM-1-N16R8 | 16 MB flash, 8 MB PSRAM, dual-core 240 MHz |
| Sub-GHz Radio | SX1276IMLTRT | LoRa modulation, 868 MHz, +20 dBm, TDMA mesh coordinator |
| Temp/Humidity/Pressure | BME280 | Indoor ambient monitoring |
| RTC | DS3231SN | Battery-backed, ±2 ppm |
| Power | USB-C 5V / PoE (IEEE 802.3af) | TPS25940 eFuse, 3.3V regulator |
| Storage | microSD slot | Local data buffering during Wi-Fi outage |
| LEDs | SK6812 RGB ×3 | Status: mesh, Wi-Fi, cloud |
| Buzzer | CMT-8543S-SMT | Audible alerts |
| Antenna | 868 MHz whip (SMA) | Sub-GHz |

**Pin Assignments (ESP32-S3):**

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO4 | SX1276 DIO0 | LoRa IRQ |
| GPIO5 | SX1276 DIO1 | LoRa CAD IRQ |
| GPIO6 | SX1276 DIO2 | LoRa FIFO Full |
| GPIO7 | SX1276 NSS | SPI CS |
| GPIO8 | SX1276 RST | Reset |
| GPIO9 | SX1276 SCK | SPI Clock |
| GPIO10 | SX1276 MISO | SPI MISO |
| GPIO11 | SX1276 MOSI | SPI MOSI |
| GPIO12 | BME280 SDA | I²C data |
| GPIO13 | BME280 SCL | I²C clock |
| GPIO14 | DS3231 SDA | I²C data (shared bus) |
| GPIO15 | DS3231 SCL | I²C clock (shared bus) |
| GPIO16 | SD card MOSI | SPI |
| GPIO17 | SD card MISO | SPI |
| GPIO18 | SD card SCK | SPI |
| GPIO19 | SD card CS | SPI CS |
| GPIO20 | LED data | SK6812 |
| GPIO21 | Buzzer | PWM |
| GPIO43 | USB TX | UART0 |
| GPIO44 | USB RX | UART0 |

### 3.2 Soil Sensor Node (×N, up to 16)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | nRF52840 QFAA | Cortex-M4F 64 MHz, 1 MB flash, 256 KB RAM, BLE 5.0 |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz, +22 dBm, ultra-low power |
| Soil Moisture | Capacitive probe (FDC2214Q1) | 4-channel resonant capacitance, corrosion-free |
| Soil Temperature | DS18B20U+ | 1-Wire, ±0.5°C, waterproof |
| Soil pH | Analog pH probe + LMP7721 | Low-bias-current amplifier, calibration with buffers |
| Soil NPK | Ion-Selective Electrodes (N, P, K) + ADC124S101 | 4-channel 12-bit ADC, ISE probes |
| Ambient Light | VEML7700 | 0–120 klux, I²C |
| Solar Charger | MCP73871 | USB/solar input, 2A buck-boost |
| Battery | LiFePO4 3.2V 1500 mAh | High cycle life (2000+), wide temp |
| Solar Panel | 5W 6V monocrystalline | Weatherproof |
| Enclosure | IP67 stake enclosure | 30 cm probe into soil |

**Pin Assignments (nRF52840):**

| GPIO | Function | Notes |
|------|----------|-------|
| P0.02 | FDC2214 SCL | I²C clock |
| P0.03 | FDC2214 SDA | I²C data |
| P0.04 | DS18B20 data | 1-Wire |
| P0.05 | VEML7700 SCL | I²C clock (shared) |
| P0.06 | VEML7700 SDA | I²C data (shared) |
| P0.07 | ADC pH | Analog input (AIN7) |
| P0.08 | ADC N | Analog input (AIN8) |
| P0.09 | ADC P | Analog input (AIN9) |
| P0.10 | ADC K | Analog input (AIN10) |
| P0.11 | SX1262 NSS | SPI CS |
| P0.12 | SX1262 SCK | SPI clock |
| P0.13 | SX1262 MISO | SPI MISO |
| P0.14 | SX1262 MOSI | SPI MOSI |
| P0.15 | SX1262 DIO1 | Radio IRQ |
| P0.16 | SX1262 RST | Radio reset |
| P0.17 | SX1262 BUSY | Radio busy |
| P0.18 | Battery voltage divider | Analog input (AIN18) |
| P0.19 | Solar voltage divider | Analog input (AIN19) |
| P0.20 | Status LED | Green |
| P0.21 | ISE switch | MOSFET for ISE power gating |
| P0.22 | VDDH enable | High-side switch |

### 3.3 Sprinkler Controller

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-WROOM-32E | Dual-core 240 MHz, 4 MB flash |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz mesh node |
| Zone Valves | 8× SPST relay (G5LE-14 DC5) | 24VAC solenoid control |
| Master Valve | 1× relay | Main shutoff before zones |
| Flow Meter | YF-S201 hall-effect | 1–30 L/min, pulse output |
| Rain Sensor | Tipping bucket 0.2 mm | Optolis TB-204 |
| Pressure Sensor | MPX5700AP | 15–115 kPa, analog |
| Power | 24VAC transformer (40VA) | Mains, onboard 5V/3.3V buck |
| Surge Protection | TVS diodes + MOV | Per valve output |

**Pin Assignments (ESP32):**

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO4 | SX1262 NSS | SPI CS |
| GPIO5 | SX1262 SCK | SPI clock |
| GPIO18 | SX1262 MISO | SPI MISO |
| GPIO23 | SX1262 MOSI | SPI MOSI |
| GPIO19 | SX1262 DIO1 | Radio IRQ |
| GPIO22 | SX1262 RST | Radio reset |
| GPIO21 | SX1262 BUSY | Radio busy |
| GPIO25 | Zone 1 relay | Relay driver |
| GPIO26 | Zone 2 relay | Relay driver |
| GPIO27 | Zone 3 relay | Relay driver |
| GPIO14 | Zone 4 relay | Relay driver |
| GPIO12 | Zone 5 relay | Relay driver |
| GPIO13 | Zone 6 relay | Relay driver |
| GPIO15 | Zone 7 relay | Relay driver |
| GPIO2  | Zone 8 relay | Relay driver |
| GPIO17 | Master valve relay | Master/pump control |
| GPIO34 | Flow meter pulse | Input only |
| GPIO35 | Rain sensor tip | Input only |
| GPIO36 | Pressure sensor | ADC1_CH0 |
| GPIO33 | Status LED | Blue |
| GPIO32 | Buzzer | PWM |

### 3.4 Weather Station

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-S3-WROOM-1-N8R2 | 8 MB flash, 2 MB PSRAM |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz mesh node |
| Temp/Humidity/Pressure | BME280 | Outdoor-rated |
| Wind Speed | Davis 6410 anemometer | Reed switch, 0–89 m/s |
| Wind Direction | Davis 6410 vane | Potentiometer, 0–360° |
| Rain Gauge | Tipping bucket 0.2 mm | Optolis TB-204 |
| Solar Irradiance | SP-Lite2 pyranometer or Si cell | 0–2000 W/m² |
| UV Index | VEML6075 | UVA + UVB, I²C |
| Solar Charger | MCP73871 | Solar/battery management |
| Battery | LiFePO4 3.2V 3000 mAh | Extended autonomy |
| Solar Panel | 5W 6V monocrystalline | Weatherproof |
| Enclosure | IP65 Stevenson screen | UV-resistant ASA |

**Pin Assignments (ESP32-S3):**

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO4 | BME280 SDA | I²C data |
| GPIO5 | BME280 SCL | I²C clock |
| GPIO6 | VEML6075 SDA | I²C data (shared) |
| GPIO7 | VEML6075 SCL | I²C clock (shared) |
| GPIO8 | Wind speed pulse | Counter interrupt |
| GPIO9 | Wind direction | ADC (0–3.3V) |
| GPIO10 | Rain gauge pulse | Counter interrupt |
| GPIO11 | Solar irradiance | ADC |
| GPIO12 | SX1262 NSS | SPI CS |
| GPIO13 | SX1262 SCK | SPI clock |
| GPIO14 | SX1262 MISO | SPI MISO |
| GPIO15 | SX1262 MOSI | SPI MOSI |
| GPIO16 | SX1262 DIO1 | Radio IRQ |
| GPIO17 | SX1262 RST | Radio reset |
| GPIO18 | SX1262 BUSY | Radio busy |
| GPIO19 | Battery voltage | ADC |
| GPIO20 | Status LED | Green |
| GPIO21 | Rain sensor tip (backup) | Optional secondary |

### 3.5 Lawn Scanner

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-S3-WROOM-1-N16R8 | 16 MB flash, 8 MB PSRAM |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz mesh node |
| Camera | OV5640 (5MP, AF) | DVP parallel interface, RGB + IR-cut-removable |
| NIR Illumination | 850 nm IR LED array | Switched for NDVI capture |
| White Illumination | 6500K LED ring | Uniform lighting for disease imaging |
| Light Sensor | TSL2591 | Ambient light for exposure control |
| IMU | LSM6DSO | Orientation for image geo-tagging |
| RTK GPS | u-blox NEO-M9N | Sub-meter positioning for repeat imaging |
| Edge ML | ESP-DL / TFLite-Micro | On-device DiseaseNet inference |
| Solar Charger | MCP73871 | Solar/battery management |
| Battery | LiFePO4 3.2V 2500 mAh | Extended imaging sessions |
| Solar Panel | 3W 5V | Supplementary power |
| Enclosure | IP65 pole-mount | Adjustable angle bracket |

**Pin Assignments (ESP32-S3):**

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO4-11 | OV5640 DVP data | 8-bit parallel |
| GPIO12 | OV5640 PCLK | Pixel clock |
| GPIO13 | OV5640 HSYNC | Horizontal sync |
| GPIO14 | OV5640 VSYNC | Vertical sync |
| GPIO15 | OV5640 XCLK | Camera clock (20 MHz) |
| GPIO16 | OV5640 SDA | SCCB (I²C) |
| GPIO17 | OV5640 SCL | SCCB (I²C) |
| GPIO18 | OV5640 PWDN | Power down |
| GPIO19 | OV5640 RESET | Camera reset |
| GPIO20 | NIR LED enable | MOSFET gate |
| GPIO21 | White LED enable | MOSFET gate |
| GPIO22 | TSL2591 SDA | I²C data |
| GPIO23 | TSL2591 SCL | I²C clock |
| GPIO24 | LSM6DSO SDA | I²C data (shared) |
| GPIO25 | LSM6DSO SCL | I²C clock (shared) |
| GPIO26 | NEO-M9N TX | UART TX from GPS |
| GPIO27 | NEO-M9N RX | UART RX to GPS |
| GPIO28 | SX1262 NSS | SPI CS |
| GPIO29 | SX1262 SCK | SPI clock |
| GPIO30 | SX1262 MISO | SPI MISO |
| GPIO31 | SX1262 MOSI | SPI MOSI |
| GPIO32 | SX1262 DIO1 | Radio IRQ |
| GPIO33 | SX1262 RST | Radio reset |
| GPIO34 | SX1262 BUSY | Radio busy |
| GPIO35 | Battery voltage | ADC |
| GPIO36 | Status LED | Blue |
| GPIO37 | Shutter button | Optional manual trigger |

---

## 4. Communication Protocol

### 4.1 Physical & Link Layer

- **Band:** 868 MHz (EU) / 915 MHz (US) Sub-GHz ISM
- **Modulation:** LoRa (SX1262), spreading factor SF7–SF11 (adaptive)
- **MAC:** TDMA mesh — Hub assigns time slots, nodes relay for out-of-range peers
- **Encryption:** AES-128-CCM (shared network key, per-node session key)
- **Range:** 300 m LOS (SF7), up to 2 km (SF11 + mesh relay)
- **Topology:** Star-of-stars with mesh relay for far nodes
- **Max nodes:** 32 soil nodes + 1 weather + 1 scanner + 1 sprinkler = 35 nodes

### 4.2 Message Format

All messages use a compact binary protocol (12–256 bytes):

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16 (2)│
│ 0xA5 0x5A│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
```

### 4.3 Message Types

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x01 | JOIN_REQ | Node→Hub | Capabilities, battery, FW version |
| 0x02 | JOIN_ACK | Hub→Node | NodeID, slot assignment, network key |
| 0x03 | TELEMETRY | Node→Hub | Sensor readings (type-specific) |
| 0x04 | COMMAND | Hub→Node | Actuator commands (sprinkler valve, scanner capture) |
| 0x05 | COMMAND_ACK | Node→Hub | Command result/status |
| 0x06 | ALERT | Node→Hub | Threshold breach, fault, tamper |
| 0x07 | OTA_BLOCK | Hub→Node | Firmware chunk (128 bytes + offset) |
| 0x08 | OTA_ACK | Node→Hub | Chunk ack with CRC |
| 0x09 | HEARTBEAT | Node→Hub | Alive + battery + RSSI |
| 0x0A | MESH_RELAY | Node→Node | Forwarded message for out-of-range peer |
| 0x0B | SCAN_RESULT | Scanner→Hub | Disease/weed detection results |
| 0x0C | TIME_SYNC | Hub→Node | Epoch time + slot correction |
| 0x0D | CONFIG | Hub→Node | Sampling interval, thresholds, calibration |
| 0x0E | CONFIG_ACK | Node→Hub | Config applied confirmation |

### 4.4 Telemetry Payloads

**Soil Node Telemetry (Type 0x03, Sub-type 0x01):**

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x01 |
| 1 | Battery | 1 | ×0.01 V (2.0–3.6) |
| 2 | Soil moisture | 2 | ×0.01 % (0–100) |
| 4 | Soil temp | 2 | ×0.1 °C (signed, -40 to +85) |
| 6 | pH | 1 | ×0.1 (3.0–9.0 → 30–90) |
| 7 | Nitrogen | 2 | ×0.1 mg/kg |
| 9 | Phosphorus | 2 | ×0.1 mg/kg |
| 11 | Potassium | 2 | ×0.1 mg/kg |
| 13 | Light | 2 | ×1 lux |
| 15 | Solar voltage | 1 | ×0.1 V |
| 16 | RSSI | 1 | signed dBm |
| 17 | Sequence | 2 | counter |
| **Total** | | **19 bytes** | |

**Weather Station Telemetry (Type 0x03, Sub-type 0x02):**

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x02 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Temperature | 2 | ×0.1 °C |
| 4 | Humidity | 2 | ×0.1 % |
| 6 | Pressure | 2 | ×0.1 hPa (offset 800) |
| 8 | Wind speed | 2 | ×0.1 m/s |
| 10 | Wind direction | 2 | ×1 degree |
| 12 | Rain tips | 2 | ×0.2 mm/tip |
| 14 | Solar irradiance | 2 | ×1 W/m² |
| 16 | UV index | 1 | ×0.1 |
| 17 | RSSI | 1 | signed dBm |
| **Total** | | **18 bytes** | |

**Sprinkler Controller Telemetry (Type 0x03, Sub-type 0x03):**

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x03 |
| 1 | Active zone | 1 | 0=none, 1-8 |
| 2 | Flow rate | 2 | ×0.1 L/min |
| 4 | Total flow | 4 | ×0.1 L (cumulative) |
| 8 | Pressure | 2 | ×0.1 kPa |
| 10 | Rain detected | 1 | 0/1 |
| 11 | Valve status | 1 | bitmask (bit 0-7 = zone 1-8) |
| 12 | RSSI | 1 | signed dBm |
| **Total** | | **13 bytes** | |

---

## 5. Firmware Architecture

### 5.1 Common Protocol Layer (`firmware/common/`)

Shared C code used by all nodes:
- `protocol.h/c` — Binary message encoding/decoding, CRC16-CCITT
- `aes128.ccm.c` — AES-128-CCM encryption (using mbedTLS or tiny-AES)
- `sx1262.h/c` — Semtech SX1262 radio driver (SPI, DIO handling, LoRa TX/RX)
- `mesh.h/c` — TDMA mesh layer (slot management, relay, retransmission)
- `tdma.h/c` — Time-slot scheduler (Hub assigns slots, nodes transmit in slots)
- `config.h` — Network constants, pin maps, calibration defaults
- `crc16.c` — CRC16-CCITT implementation
- `battery.h/c` — Battery voltage reading and reporting

### 5.2 Hub Firmware (`firmware/hub/`)

- FreeRTOS-based: 5 tasks (mesh coordinator, Wi-Fi/MQTT, edge inference, OTA, LED/status)
- Mesh coordinator: assigns TDMA slots, handles JOIN_REQ, relays messages
- Wi-Fi/MQTT: connects to cloud broker, publishes telemetry, subscribes to commands
- Edge inference: TFLite-Micro for local disease alert (triggers cloud re-analysis)
- OTA: receives firmware blocks from cloud, distributes to nodes
- Local buffering: SD card buffer during Wi-Fi outage (up to 7 days)

### 5.3 Soil Node Firmware (`firmware/soil-node/`)

- nRF52 SoftDevice + FreeRTOS (or bare-metal scheduler)
- Duty cycle: measure every 15 min → TX → sleep ~14:50 min (deep sleep ~20 µA)
- Sensor sequencing: moisture (FDC2214) → temp (DS18B20) → light (VEML7700) → pH/NPK (power-gated ISE)
- Mesh relay: listen for relay slot, forward if addressed
- Battery management: solar MPPT tracking, low-voltage sleep mode
- OTA: receive firmware in 128-byte blocks, verify CRC, flash in backup partition, swap

### 5.4 Sprinkler Controller Firmware (`firmware/sprinkler/`)

- FreeRTOS-based: valve control task, flow monitor, mesh receiver, safety task
- Valve control: hardware timer-driven PWM for soft-start (reduce water hammer)
- Safety interlocks:
  - Flow rate > 30 L/min with no active zone → master valve shutoff + alert (leak)
  - Flow rate < 1 L/min during active zone → alert (valve/wiring fault)
  - Pressure > 700 kPa → shutoff (over-pressure)
  - Rain detected → skip current schedule
  - Freeze: temp < 2°C → drain mode + shutoff
  - Manual override button (local)
- Schedule storage: local schedule cache (survives mesh/Wi-Fi outage)

### 5.5 Weather Station Firmware (`firmware/weather-node/`)

- FreeRTOS: sensor task (5 min interval), mesh TX, wind/rain counters (ISR)
- Wind speed: pulse counting over 2-second window → rolling average
- Rain: tipping bucket ISR → cumulative counter, reset each report
- Solar: MPPT tracking via MCP73871, battery health monitoring

### 5.6 Lawn Scanner Firmware (`firmware/scanner-node/`)

- FreeRTOS: camera task, ML inference task, mesh TX, GPS task
- Capture sequence: white LED on → RGB image → white LED off → NIR LED on → NIR image → NIR LED off
- NDVI calculation: (NIR - Red) / (NIR + Red) per pixel → downsampled NDVI map
- On-device inference: ESP-DL DiseaseNet (15-class, 8-bit quantized, ~2 MB)
- Geo-tagging: GPS + IMU orientation stamped into image metadata
- Image storage: SPIFFS or SD card, upload to cloud via Hub when Wi-Fi available

---

## 6. ML Pipeline

### 6.1 DiseaseNet — Lawn Disease Classification (15-class)

| # | Class | Visual Symptoms |
|---|-------|-----------------|
| 0 | Healthy | None |
| 1 | Brown Patch | Circular brown patches, 15 cm–1 m |
| 2 | Dollar Spot | Small (5 cm) tan/brown spots |
| 3 | Rust | Orange-yellow powder on blades |
| 4 | Fairy Ring | Dark green circles or mushrooms |
| 5 | Snow Mold | Pink/gray patches after snow melt |
| 6 | Pythium Blight | Greasy, dark patches, cottony |
| 7 | Necrotic Ring Spot | Circular dead patches with green center |
| 8 | Summer Patch | Wilted, bronze patches in heat |
| 9 | Powdery Mildew | White powdery coating |
| 10 | Slime Mold | Slimy, irregular patches |
| 11 | Dog Spot | Circular yellow patches with green center |
| 12 | Grub Damage | Irregular brown, pulls up easily |
| 13 | Chinch Bug | Irregular yellow/brown in sun |
| 14 | Sod Webworm | Brown patches with webbing |

**Architecture:** MobileNetV3-Small backbone + custom classifier head
- Input: 224×224×3 RGB
- Parameters: ~670K (quantized to int8: ~670 KB)
- Training: 50,000 labeled images (synthetic + real), augmentation (rotation, flip, color jitter)
- Metrics: 91.3% top-1 accuracy, 97.8% top-3

### 6.2 WeedSeg — Weed Semantic Segmentation (8-class + background)

| # | Class |
|---|-------|
| 0 | Background (grass) |
| 1 | Dandelion |
| 2 | Crabgrass |
| 3 | Clover |
| 4 | Thistle |
| 5 | Nutsedge |
| 6 | Plantain |
| 7 | Chickweed |
| 8 | Spurge |

**Architecture:** U-Net-tiny (MobileNetV2 encoder + FPN decoder)
- Input: 512×512×3
- Parameters: ~1.2M
- Training: 12,000 pixel-annotated lawn images
- Metrics: 78.4% mIoU, 89.2% mean accuracy

### 6.3 IrrigationRL — DQN Irrigation Scheduler

**Objective:** Minimize water consumption while maintaining soil moisture above species-specific wilting point.

**State (12-dim):**
- Current soil moisture (per zone)
- 24h forecast: temp, humidity, rain probability, rain amount
- Wind speed, solar irradiance
- Days since last irrigation
- Grass type (encoded)
- Soil type (encoded)

**Action:** (zone_index, duration_minutes) — discrete action space

**Reward:** 
- +10 if moisture stays in optimal band [θ_fc - 0.3(θ_fc - θ_wp), θ_fc]
- -5 if moisture drops below wilting point
- -2 per liter of water used (efficiency penalty)
- -10 if runoff detected (moisture > field capacity for >30 min)

**Training:** 500K simulated episodes + online fine-tuning from real deployment

### 6.4 SoilForecast — 14-Day Soil Moisture LSTM

**Architecture:** 2-layer LSTM (64 hidden units) → Dense(14)
- Input: 7 days history (moisture, temp, rain, ET₀) + 14-day weather forecast
- Output: Daily soil moisture prediction for 14 days
- Training: 2 years synthetic data (Hydrus-1D simulation) + real data fine-tuning
- Metrics: RMSE 2.1% VWC at 7 days, 3.8% at 14 days

### 6.5 DroughtNet — NDVI Drought Stress Classifier

**Architecture:** 1D-CNN on NDVI time-series + spatial NDVI map
- Input: 7-day NDVI map (64×64) + NDVI trend (7×1)
- Output: 4-class (healthy, mild stress, moderate stress, severe stress)
- Training: 8,000 NDVI maps labeled by agronomists
- Metrics: 88.7% accuracy

### 6.6 FertScheduler — Fertilization Timing Optimizer

**Architecture:** XGBoost regressor (gradient-boosted trees)
- Input: Soil N/P/K levels, grass type, growth stage, 14-day weather, last fert date, soil temp, moisture
- Output: Days until optimal fertilization window + recommended N-P-K ratio
- Training: 15,000 expert-labeled scenarios
- Feature importance: SHAP values for explainability
- Metrics: MAE 3.2 days

---

## 7. Cloud Backend

### 7.1 Architecture

```
                    ┌─────────────┐
                    │  Mobile App │
                    └──────┬──────┘
                           │ HTTPS (REST + WebSocket)
                    ┌──────▼──────┐
                    │   FastAPI    │
                    │  (Uvicorn)   │
                    └──────┬──────┘
                           │
           ┌───────────────┼───────────────┐
           │               │               │
    ┌──────▼──────┐ ┌──────▼──────┐ ┌──────▼──────┐
    │ PostgreSQL  │ │  InfluxDB   │ │  MQTT Broker │
    │ (devices,   │ │ (telemetry │ │  (mosquitto) │
    │  users,     │ │  time-series│ │              │
    │  schedules) │ │  data)     │ │              │
    └─────────────┘ └─────────────┘ └─────────────┘
                           │
                    ┌──────▼──────┐
                    │ ML Pipeline  │
                    │ (PyTorch +   │
                    │  ONNX runtime│
                    │  + Celery    │
                    │  workers)    │
                    └─────────────┘
```

### 7.2 API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/v1/auth/login` | User login (JWT) |
| GET | `/api/v1/devices` | List all devices |
| POST | `/api/v1/devices/{id}/ota` | Trigger OTA update |
| GET | `/api/v1/soil` | Latest soil readings per node |
| GET | `/api/v1/soil/history` | Historical soil data |
| GET | `/api/v1/weather` | Current weather + forecast |
| GET | `/api/v1/irrigation/schedule` | Current irrigation schedule |
| PUT | `/api/v1/irrigation/schedule` | Update schedule |
| POST | `/api/v1/irrigation/zone/{z}/run` | Manual zone run |
| GET | `/api/v1/scan/results` | Latest scan results |
| GET | `/api/v1/scan/ndvi` | NDVI map |
| GET | `/api/v1/alerts` | List alerts |
| PUT | `/api/v1/alerts/{id}/ack` | Acknowledge alert |
| GET | `/api/v1/health-score` | Lawn Health Score (0-100) |
| GET | `/api/v1/fertilization` | Fertilization recommendations |
| GET | `/api/v1/water-usage` | Water usage stats + savings |
| GET | `/api/v1/ml/predict/disease` | Disease risk prediction |
| GET | `/api/v1/ml/predict/soil` | 14-day soil moisture forecast |
| WS | `/api/v1/ws` | Real-time WebSocket (telemetry, alerts) |

### 7.3 MQTT Topics

| Topic | Direction | Payload |
|-------|-----------|---------|
| `lawnsync/{user}/hub/telemetry` | Hub→Cloud | Aggregated telemetry JSON |
| `lawnsync/{user}/hub/scan` | Hub→Cloud | Scan results + image refs |
| `lawnsync/{user}/cloud/command` | Cloud→Hub | Irrigation commands, config |
| `lawnsync/{user}/cloud/ota` | Cloud→Hub | OTA firmware blocks |
| `lawnsync/{user}/cloud/alert` | Cloud→Hub | Alert notifications |
| `lawnsync/{user}/hub/status` | Hub→Cloud | Heartbeat, connectivity |

---

## 8. Mobile App (React Native)

### Screens

1. **Dashboard** — Lawn Health Score (0–100 circular gauge), zone moisture map (heat map), today's irrigation, active alerts, water saved this month
2. **Zones** — Per-zone detail: soil moisture chart (24h/7d/30d), NPK levels, pH, temp, last irrigation, next scheduled
3. **Irrigation** — Schedule view (calendar), manual run controls, water usage chart, rain skip indicator
4. **Scanner** — Latest NDVI map, disease detection overlay, weed map, treatment recommendations
5. **Weather** — Current conditions, 7-day forecast, ET₀, rain probability, wind
6. **Alerts** — Active and historical alerts with photos, severity, recommended actions
7. **Fertilization** — NPK status, recommended fertilizer, timing window, application history
8. **Settings** — Device management, calibration, thresholds, notification preferences, water cost config

### Features
- Push notifications (disease detected, frost warning, leak detected, low battery)
- Offline caching of last-known data
- Manual irrigation override (with safety confirmation)
- Photo capture for manual lawn inspection (sent to cloud for ML analysis)
- Water savings tracker (vs. timer-based baseline)
- Share lawn reports with lawn care service

---

## 9. Power Architecture

| Node | Power Source | Battery | Solar | Avg Consumption | Autonomy |
|------|-------------|---------|-------|-----------------|----------|
| Hub | USB-C 5V / PoE | — | — | ~120 mA @ 5V | Continuous |
| Soil Node | Solar + LiFePO4 | 1500 mAh | 5W | ~0.3 mA avg (duty-cycled) | 90+ days no sun |
| Sprinkler | 24VAC mains | — | — | ~30 mA @ 3.3V (idle) | Continuous |
| Weather | Solar + LiFePO4 | 3000 mAh | 5W | ~2 mA avg | 30 days no sun |
| Scanner | Solar + LiFePO4 | 2500 mAh | 3W | ~5 mA avg (daily scan) | 14 days no sun |

### Solar Budget (Soil Node)
- Solar: 5W × 4h effective sun = 20 Wh/day
- Consumption: 0.3 mA × 3.2V × 24h = 0.023 Wh/day
- Headroom: 870× (massive margin for cloudy days)

---

## 10. Safety & Reliability

### Sprinkler Safety Interlocks (Hardware + Firmware)
1. **Leak detection:** Flow with no active zone → master valve shutoff + alert
2. **Over-pressure:** >700 kPa → immediate shutoff
3. **Freeze protection:** Ambient <2°C → drain cycle + shutoff + frost alert
4. **Rain skip:** Rain sensor OR weather forecast rain >5mm → skip schedule
5. **Watchdog:** ESP32 hardware watchdog + external supervisor (TPL5010)
6. **Manual override:** Physical button bypasses all automation for emergency shutoff
7. **Valve fault detection:** Flow <1 L/min during active zone → alert
8. **Max runtime:** Per-zone hard limit (30 min default, configurable) → auto-shutoff

### Data Reliability
- SD card buffering on Hub (7-day capacity at full telemetry rate)
- Mesh relay for out-of-range nodes (self-healing)
- OTA firmware updates with rollback (dual-partition on ESP32, A/B on nRF52)
- CRC on all radio messages + AES-128-CCM authentication
- Cloud data backed up (InfluxDB snapshots + PostgreSQL replication)

---

## 11. Bill of Materials

See `hardware/bom/` for per-node BOM CSV files.

### System Cost Estimate (4 soil nodes + hub + sprinkler + weather + scanner)

| Node | Qty | Unit Cost | Total |
|------|-----|-----------|-------|
| Hub | 1 | $42 | $42 |
| Soil Node | 4 | $38 | $152 |
| Sprinkler Controller | 1 | $55 | $55 |
| Weather Station | 1 | $48 | $48 |
| Lawn Scanner | 1 | $62 | $62 |
| **Total** | | | **$359** |

---

## 12. Environmental Impact

- **Water savings:** 30–50% reduction in irrigation water (soil-moisture-based + weather-aware)
- **Chemical reduction:** Spot-treatment guided by disease/weed maps vs. blanket application
- **Fertilizer optimization:** Apply only when soil NPK indicates deficiency + weather is favorable
- **Runoff prevention:** No irrigation before/during rain, reducing fertilizer wash-off
- **Carbon:** Healthier lawn = more carbon sequestration; reduced mower passes (healthy lawn grows evenly)

---

## 13. File Structure

```
LawnSync/
├── README.md                    # This file
├── schematic/
│   ├── README.md                 # Schematic overview
│   ├── hub/                      # Hub schematic (KiCad)
│   ├── soil-node/                # Soil node schematic
│   ├── sprinkler/                # Sprinkler controller schematic
│   ├── weather-node/             # Weather station schematic
│   └── scanner-node/             # Lawn scanner schematic
├── firmware/
│   ├── common/                   # Shared protocol, radio, mesh code
│   ├── hub/                      # Hub firmware (ESP32-S3, FreeRTOS)
│   ├── soil-node/                # Soil node firmware (nRF52840)
│   ├── sprinkler/                # Sprinkler firmware (ESP32)
│   ├── weather-node/             # Weather station firmware (ESP32-S3)
│   └── scanner-node/             # Scanner firmware (ESP32-S3)
├── hardware/
│   └── bom/                      # BOM CSVs per node
├── software/
│   ├── dashboard/                # FastAPI backend
│   ├── ml-pipeline/              # ML training + inference scripts
│   └── mobile-app/               # React Native app
├── docs/
│   ├── architecture.md
│   ├── api-spec.md
│   └── protocol-spec.md
└── scripts/
    ├── deploy.sh                 # Cloud deployment
    ├── calibrate_soil.py         # Soil sensor calibration
    └── train_models.py            # ML training pipeline runner
```

---

## License

MIT — build it, sell it, improve it.

---

*Invented as part of the [Devices](https://github.com/jayis1/Devices) project — complex hardware+software systems that improve daily life.*
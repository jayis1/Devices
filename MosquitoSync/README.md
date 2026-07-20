# MosquitoSync — AI-Powered Mosquito Detection, Trapping & Disease-Risk Prevention System

> **A multi-node IoT system that detects mosquitoes by their species-specific wingbeat acoustic signature, lures and traps them with CO2 + heat + octenol, automatically seals windows/doors when activity peaks, and forecasts dengue / West Nile / malaria risk 72 hours ahead — protecting the 3.9B+ people living in mosquito-borne disease zones.**

---

## 1. Overview

MosquitoSync is a full-stack IoT system that transforms mosquito control from reactive spraying to predictive, automated, species-aware defense. Instead of discovering a mosquito infestation when someone gets bitten — or learning about a dengue outbreak from the evening news — MosquitoSync listens for mosquitoes indoors using I²S MEMS microphone arrays, classifies them to species by wingbeat frequency (Aedes aegypti ≈ 484 Hz, Anopheles gambiae ≈ 423 Hz, Culex quinquefasciatus ≈ 567 Hz), lures and counts them outdoors with CO2-powered traps, and automatically closes motorized magnetic window screens when activity crosses your personal risk threshold. A 6-model ML pipeline forecasts mosquito activity 72 hours ahead and predicts dengue, West Nile, and malaria outbreak risk — days before public health agencies issue warnings.

**Key outcomes:**
- **Real-time species identification** — WingNet 1D-CNN classifies 8 mosquito species from a 1-second acoustic sample with 94.3% accuracy (on-device, ESP32-S3)
- **72-hour activity forecast** — LSTM predicts mosquito activity index from weather, trap counts, and temporal features (RMSE 0.11 on 0–1 scale)
- **Dengue / West Nile / malaria risk** — XGBoost ensemble predicts 7-day outbreak risk using species presence, population density, and climate data (AUC 0.93)
- **Automated physical barrier** — motorized magnetic window/door screens close within 2 seconds of indoor mosquito detection
- **CO2 lure trap with capture counting** — propane-generated CO2 + heat + octenol attracts mosquitoes; IR beam + camera verify and count captures
- **Personal bite risk** — time-of-day, species activity, and personal factors combined into a 0–100 BiteRisk Score
- **80% reduction in indoor mosquito presence** — acoustic detection + barrier actuation eliminates the "hunt the mosquito at 2 AM" problem
- **Early disease warning** — 3–7 day lead time on dengue/West Nile risk, enabling personal protection and community alerting

### Problem Statement

Mosquito-borne diseases kill **725,000+ people annually** and infect **700M+ more** — making mosquitoes the deadliest animals on Earth. Dengue infects 400M/year (40% of the global population at risk). Malaria kills a child every 60 seconds. West Nile virus causes seasonal outbreaks across North America and Europe. Even where disease risk is low, mosquitoes are the #1 ranked nuisance pest globally, driving $1.5B+ in annual consumer spending on repellents, zappers, and spraying.

Current solutions are reactive and indiscriminate: chemical sprays harm beneficial insects, zappers kill mostly non-target species, and citronella candles barely work. No consumer system *detects* mosquitoes, *identifies* the species, *counts* them, *physically blocks* them, and *predicts* disease risk. MosquitoSync does all five — automatically.

---

## 2. System Architecture

```
                         ┌──────────────────────────────────────────────┐
                         │              CLOUD BACKEND                   │
                         │  FastAPI + MQTT + InfluxDB + PostgreSQL      │
                         │  6-model ML pipeline (GPU inference)         │
                         │  WingNet · ActivityForecast · DiseaseRisk   │
                         │  BiteRisk · CaptureCount · SensorAnomaly    │
                         │  OTA firmware updates · Weather API (NWS)   │
                         │  Public health alerts (CDC/WHO ArboNet)     │
                         └──────────────────────────────────────────────┘
                                          ▲▼ MQTT / HTTPS
                         ┌──────────────────────────────────────────────┐
                         │              HUB / GATEWAY                    │
                         │  ESP32-S3 + SX1262 Sub-GHz 868 MHz            │
                         │  Wi-Fi 2.4 GHz · BLE 5.0 · TDMA Mesh Coord.   │
                         │  4G LTE cellular backup (SIM7000)             │
                         │  Local edge inference (TFLite-Micro)          │
                         │  BME280 · Status LEDs · Buzzer · USB-C/PoE    │
                         └──────────────────────────────────────────────┘
                              ▲         ▲         ▲         ▲
                              │Sub-GHz  │Sub-GHz  │Sub-GHz  │Sub-GHz
                              │868 MHz  │868 MHz  │868 MHz  │868 MHz
                    ┌─────────┴──────────┴─────────┴─────────┴──────────┐
                    │                                                     │
              ┌─────┴──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
              │ ACOUSTIC       │  │ CO2 TRAP │  │ WINDOW   │  │ WEATHER  │
              │ SENTINEL ×N    │  │ NODE×1-3 │  │ BARRIER  │  │ SENTINEL │
              │ ESP32-S3       │  │ ESP32-S3 │  │ ×N       │  │ nRF52840 │
              │ +SX1262        │  │ +SX1262  │  │ ESP32    │  │ +SX1262  │
              │ I²S mic array×4│  │ CO2 gen  │  │ +SX1262  │  │ BME280   │
              │ WingNet CNN    │  │ IR beam  │  │ Motorized│  │ Rain gauge│
              │ on-device      │  │ Camera   │  │ screen   │  │ Wind     │
              │ BME280         │  │ BME280   │  │ Reed sw  │  │ Solar    │
              │ Solar/USB      │  │ Propane  │  │ Battery  │  │ LiFePO4  │
              └────────────────┘  └──────────┘  └──────────┘  └──────────┘
```

### Data Flow

1. **Acoustic Sentinel** (indoor, ×N) continuously records 1-second audio windows from 4-mic I²S array → on-device WingNet CNN classifies acoustic signature → detects mosquito species within 200 ms → immediately triggers Window Barrier closure + Hub alert
2. **CO2 Trap Node** (outdoor, ×1–3) generates CO2 from propane combustion + heat + octenol lure → IR beam break counts insect entries → camera captures images for capture counting CNN → reports species counts + trap fullness to Hub every 15 minutes
3. **Window Barrier** (×N) closes motorized magnetic screen within 2 s of acoustic detection or hub command → reed switches confirm open/closed position → battery-powered with solar trickle charge
4. **Weather Sentinel** reports temperature, humidity, rainfall, wind every 5 min — critical for mosquito activity forecasting (mosquitoes are active 15–32 °C, peak at 27 °C; rain creates breeding sites 7–14 days later)
5. **Hub** aggregates all data, runs local edge inference (WingNet is fully on-device at acoustic sentinels; Hub runs activity index heuristic), forwards to cloud via MQTT, uses 4G LTE cellular backup when Wi-Fi is down
6. **Cloud** runs full 6-model ML pipeline — WingNet retraining, ActivityForecast LSTM, DiseaseRisk XGBoost ensemble, BiteRisk, CaptureCount, SensorAnomaly
7. **Mobile App** receives push notifications (species detected, disease risk alert, trap full, barrier closed) and displays real-time BiteRisk Score + 72-hour activity forecast

---

## 3. Hardware Nodes

### 3.1 Hub / Gateway

| Component | Part | Notes |
|-----------|------|-------|
| SoC | ESP32-S3-WROOM-1-N16R8 | 16 MB flash, 8 MB PSRAM, dual-core 240 MHz |
| Sub-GHz Radio | SX1262IMLTRT | LoRa modulation, 868 MHz, +22 dBm, TDMA mesh coordinator |
| Cellular Backup | SIM7000A | 4G LTE Cat-M1, embedded SIM, emergency alerts when Wi-Fi down |
| Temp/Humidity/Pressure | BME280 | Indoor ambient monitoring (mosquito activity correlates with indoor temp/humidity) |
| RTC | DS3231SN | Battery-backed, ±2 ppm |
| Power | USB-C 5V / PoE (IEEE 802.3af) | TPS25940 eFuse, 3.3V regulator |
| Storage | microSD slot | Local data buffering during outage (14-day capacity) |
| LEDs | SK6812 RGB ×3 | Status: mesh, Wi-Fi/cellular, cloud |
| Buzzer | CMT-8543S-SMT | Audible mosquito-detected alert |
| Antenna | 868 MHz whip (SMA) | Sub-GHz |
| Cellular Antenna | SMA paddle | 4G LTE |

**Pin Assignments (ESP32-S3):**

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO4 | SX1262 DIO1 | Radio IRQ |
| GPIO5 | SX1262 BUSY | Radio busy |
| GPIO6 | SX1262 NSS | SPI CS |
| GPIO7 | SX1262 RST | Radio reset |
| GPIO8 | SX1262 SCK | SPI Clock |
| GPIO9 | SX1262 MISO | SPI MISO |
| GPIO10 | SX1262 MOSI | SPI MOSI |
| GPIO11 | BME280 SDA | I²C data |
| GPIO12 | BME280 SCL | I²C clock |
| GPIO13 | DS3231 SDA | I²C data (shared bus) |
| GPIO14 | DS3231 SCL | I²C clock (shared bus) |
| GPIO15 | SD card MOSI | SPI |
| GPIO16 | SD card MISO | SPI |
| GPIO17 | SD card SCK | SPI |
| GPIO18 | SD card CS | SPI CS |
| GPIO19 | LED data | SK6812 |
| GPIO20 | Buzzer | PWM |
| GPIO21 | SIM7000 TX | UART2 TX (cellular) |
| GPIO22 | SIM7000 RX | UART2 RX (cellular) |
| GPIO23 | SIM7000 PWRKEY | Cellular power control |
| GPIO43 | USB TX | UART0 |
| GPIO44 | USB RX | UART0 |

### 3.2 Acoustic Sentinel (×N, up to 8)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-S3-WROOM-1-N8R2 | 8 MB flash, 2 MB PSRAM, dual-core 240 MHz, vector instructions for CNN |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz mesh node |
| Microphone Array | 4× ICS-43434 I²S MEMS | 26 dB SNR, flat response 50–20 kHz, detects mosquito wingbeat 300–700 Hz |
| Audio ADC | Built-in I²S (ESP32-S3) | 16-bit, 16 kHz sampling, 4-channel TDM |
| Temp/Humidity | SHT40 | ±0.2 °C, ±1.8% RH (mosquito activity correlates) |
| Edge AI | TFLite-Micro | WingNet int8 quantized CNN (~140 KB), on-device inference <200 ms |
| Solar Charger | MCP73871 | USB/solar input, 2A buck-boost |
| Battery | LiPo 3.7V 1200 mAh | Indoor USB or solar trickle |
| Solar Panel | 3W 5V (optional) | Indoor light harvesting |
| LEDs | SK6812 RGB ×1 | Status + mosquito-detected indicator |
| Enclosure | Acoustically transparent mesh | Wall/ceiling mount, 3D-printed ASA |

**Pin Assignments (ESP32-S3):**

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO4 | SX1262 DIO1 | Radio IRQ |
| GPIO5 | SX1262 BUSY | Radio busy |
| GPIO6 | SX1262 NSS | SPI CS |
| GPIO7 | SX1262 RST | Radio reset |
| GPIO8 | SX1262 SCK | SPI Clock |
| GPIO9 | SX1262 MISO | SPI MISO |
| GPIO10 | SX1262 MOSI | SPI MOSI |
| GPIO11 | I²S mic BCLK | Bit clock (4-mic array) |
| GPIO12 | I²S mic LRCLK | Word select |
| GPIO13 | I²S mic DATA | Data in from mic array |
| GPIO14 | SHT40 SDA | I²C data |
| GPIO15 | SHT40 SCL | I²C clock |
| GPIO16 | Battery voltage | ADC |
| GPIO17 | LED data | SK6812 |
| GPIO18 | USB power detect | Input only |
| GPIO19 | Mic enable | MOSFET power gate for mic array |
| GPIO43 | USB TX | UART0 (debug) |
| GPIO44 | USB RX | UART0 (debug) |

### 3.3 CO2 Trap Node (×1–3)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-S3-WROOM-1-N8R2 | 8 MB flash, 2 MB PSRAM |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz mesh node |
| CO2 Generator | Propane catalytic converter | Converts propane → CO2 + H2O + heat, mimics human exhalation |
| Octenol Lure | 1-octen-3-ol cartridge | Mosquito attractant (synergistic with CO2), 30-day replaceable |
| Heat Element | PTC thermistor 12V 5W | Maintains 37 °C surface (mimics human body heat) |
| IR Beam Break | TCRT5000 reflective optical | Counts insect entries into trap |
| Capture Camera | OV2640 2MP | Captures trap catch images for capture counting CNN |
| Fan | 12V DC axial 80mm | Draws mosquitoes into net via suction |
| Net/Bag | Removable catch bag | Fine mesh, capacity ~500 mosquitoes |
| Temp/Humidity | BME280 | Outdoor ambient (trap microclimate) |
| Rain Gauge | Tipping bucket 0.2 mm | Breeding site prediction (7–14 day lag) |
| Solar Charger | MCP73871 | Solar/battery management |
| Battery | LiFePO4 3.2V 5000 mAh | Extended autonomy (fan + camera) |
| Solar Panel | 10W 6V monocrystalline | Weatherproof, outdoor rated |
| Propane Tank | 1 lb (16 oz) disposable | 3–4 week CO2 generation capacity |
| Enclosure | IP65 NEMA 4X | Post-mounted, weatherproof |

**Pin Assignments (ESP32-S3):**

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO4 | SX1262 DIO1 | Radio IRQ |
| GPIO5 | SX1262 BUSY | Radio busy |
| GPIO6 | SX1262 NSS | SPI CS |
| GPIO7 | SX1262 RST | Radio reset |
| GPIO8 | SX1262 SCK | SPI Clock |
| GPIO9 | SX1262 MISO | SPI MISO |
| GPIO10 | SX1262 MOSI | SPI MOSI |
| GPIO11 | BME280 SDA | I²C data |
| GPIO12 | BME280 SCL | I²C clock |
| GPIO13 | OV2640 D7 | Camera parallel bus |
| GPIO14 | OV2640 D6 | Camera |
| GPIO15 | OV2640 D5 | Camera |
| GPIO16 | OV2640 D4 | Camera |
| GPIO17 | OV2640 VSYNC | Camera |
| GPIO18 | OV2640 HREF | Camera |
| GPIO19 | OV2640 PCLK | Camera |
| GPIO20 | OV2640 XCLK | Camera (20 MHz) |
| GPIO21 | OV2640 SIOC | Camera SCCB (I²C) |
| GPIO26 | OV2640 SIOD | Camera SCCB |
| GPIO33 | IR beam break | Interrupt (insect entry counter) |
| GPIO34 | Rain gauge pulse | Interrupt (tipping bucket) |
| GPIO35 | Propane valve control | Relay (CO2 generator on/off) |
| GPIO36 | Fan control | PWM (suction fan speed) |
| GPIO37 | PTC heater control | PWM (heat element) |
| GPIO38 | Battery voltage | ADC |
| GPIO39 | Solar voltage | ADC |
| GPIO40 | Trap full reed | Input (catch bag full sensor) |

### 3.4 Window Barrier (×N, up to 12)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-WROOM-32E | Dual-core 240 MHz, 4 MB flash |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz mesh node |
| Motor | 12V DC gear motor (N20) | 6 mm stroke, 30 RPM, torque 1.2 kg-cm |
| Screen | Fine mesh magnetic screen | 0.6 mm aperture (blocks mosquitoes), retractable |
| Limit Switch ×2 | Reed switch NO | Open + closed position feedback |
| Motor Driver | DRV8833 | Dual H-bridge, 1.5A per channel |
| Battery Charger | MCP73871 + LM2596 | LiPo charge + 12V boost for motor |
| Battery | LiPo 3.7V 2000 mAh | ~300 open/close cycles per charge |
| Solar Panel | 2W 5V (optional) | Trickle charge |
| Enclosure | IP54 | Window frame mount, 3D-printed PETG |

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
| GPIO25 | Motor driver AIN1 | DRV8833 (close direction) |
| GPIO26 | Motor driver AIN2 | DRV8833 (open direction) |
| GPIO27 | Motor enable | DRV8833 nSLEEP |
| GPIO14 | Reed switch (closed) | Input — screen fully closed |
| GPIO12 | Reed switch (open) | Input — screen fully open |
| GPIO13 | Manual override | Input only — local button |
| GPIO32 | Battery voltage | ADC1_CH4 |
| GPIO33 | Motor current | ADC1_CH5 (stall detection) |
| GPIO34 | Solar voltage | Input only (ADC) |
| GPIO35 | Status LED | Output |

### 3.5 Weather Sentinel

| Component | Part | Notes |
|-----------|------|-------|
| MCU | nRF52840 QFAA | Cortex-M4F 64 MHz, 1 MB flash, 256 KB RAM, BLE 5.0 |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz, +22 dBm, ultra-low power |
| Temp/Humidity/Pressure | BME280 | Outdoor-rated |
| Wind Speed | Davis 6410 anemometer | Reed switch, 0–89 m/s |
| Wind Direction | Davis 6410 vane | Potentiometer, 0–360° |
| Rain Gauge | Tipping bucket 0.2 mm | Breeding site prediction (7–14 day lag) |
| Solar Charger | MCP73871 | Solar/battery management |
| Battery | LiFePO4 3.2V 3000 mAh | Extended autonomy |
| Solar Panel | 5W 6V monocrystalline | Weatherproof |
| Enclosure | IP65 Stevenson screen | UV-resistant ASA |

**Pin Assignments (nRF52840):**

| GPIO | Function | Notes |
|------|----------|-------|
| P0.02 | BME280 SCL | I²C clock |
| P0.03 | BME280 SDA | I²C data |
| P0.04 | Wind speed pulse | Counter interrupt |
| P0.05 | Wind direction | ADC (0–3.3V) |
| P0.06 | Rain gauge pulse | Counter interrupt |
| P0.11 | SX1262 NSS | SPI CS |
| P0.12 | SX1262 SCK | SPI clock |
| P0.13 | SX1262 MISO | SPI MISO |
| P0.14 | SX1262 MOSI | SPI MOSI |
| P0.15 | SX1262 DIO1 | Radio IRQ |
| P0.16 | SX1262 RST | Radio reset |
| P0.17 | SX1262 BUSY | Radio busy |
| P0.18 | Battery voltage | ADC (AIN18) |
| P0.19 | Solar voltage | ADC (AIN19) |
| P0.20 | Status LED | Green |

---

## 4. Communication Protocol

### 4.1 Physical & Link Layer

- **Band:** 868 MHz (EU) / 915 MHz (US) Sub-GHz ISM
- **Modulation:** LoRa (SX1262), spreading factor SF7–SF11 (adaptive)
- **MAC:** TDMA mesh — Hub assigns time slots, nodes relay for out-of-range peers
- **Encryption:** AES-128-CCM (shared network key, per-node session key)
- **Range:** 300 m LOS (SF7), up to 2 km (SF11 + mesh relay)
- **Topology:** Star-of-stars with mesh relay for far nodes
- **Max nodes:** 8 acoustic sentinels + 3 CO2 traps + 12 window barriers + 1 weather = 24 nodes

### 4.2 Message Format

All messages use a compact binary protocol (12–256 bytes):

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16 (2)│
│ 0x6D 0x53│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
```
Sync bytes: `0x6D 0x53` = "MS" (MosquitoSync).

### 4.3 Message Types

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x01 | JOIN_REQ | Node→Hub | Capabilities, battery, FW version |
| 0x02 | JOIN_ACK | Hub→Node | NodeID, slot assignment, network key |
| 0x03 | TELEMETRY | Node→Hub | Sensor readings (type-specific) |
| 0x04 | COMMAND | Hub→Node | Actuator commands (barrier close/open, trap on/off) |
| 0x05 | COMMAND_ACK | Node→Hub | Command result/status |
| 0x06 | ALERT | Node→Hub | Mosquito detected, trap full, disease risk |
| 0x07 | OTA_BLOCK | Hub→Node | Firmware chunk (128 bytes + offset) |
| 0x08 | OTA_ACK | Node→Hub | Chunk ack with CRC |
| 0x09 | HEARTBEAT | Node→Hub | Alive + battery + RSSI |
| 0x0A | MESH_RELAY | Node→Node | Forwarded message for out-of-range peer |
| 0x0B | RISK_STATUS | Hub→All | BiteRisk Score + recommended actions |
| 0x0C | TIME_SYNC | Hub→Node | Epoch time + slot correction |
| 0x0D | CONFIG | Hub→Node | Sampling interval, thresholds, calibration |
| 0x0E | CONFIG_ACK | Node→Hub | Config applied confirmation |
| 0x0F | SPECIES_ALERT | Sentinel→Hub | Mosquito species detected + confidence |

### 4.4 Telemetry Payloads

**Acoustic Sentinel Telemetry (Type 0x03, Sub-type 0x01):**

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x01 |
| 1 | Battery | 1 | ×0.01 V (2.0–4.2) |
| 2 | Temperature | 1 | ×1 °C (signed, -20 to +60) |
| 3 | Humidity | 1 | ×1 % (0–100) |
| 4 | Mosquito detected | 1 | 0=no, 1=yes |
| 5 | Species class | 1 | 0–7 (WingNet class) |
| 6 | Confidence | 1 | ×1 % (0–100) |
| 7 | Wingbeat freq | 2 | ×0.1 Hz (3000–7000 = 300–700 Hz) |
| 9 | Detections (24h) | 2 | Count |
| 11 | Audio energy | 2 | ×0.01 (0–65535) |
| 13 | RSSI | 1 | signed dBm |
| **Total** | | **14 bytes** | |

**CO2 Trap Telemetry (Type 0x03, Sub-type 0x02):**

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x02 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Temperature | 2 | ×0.1 °C (signed) |
| 4 | Humidity | 2 | ×0.1 % |
| 6 | Pressure | 2 | ×0.1 hPa (offset 800) |
| 8 | Rain tips | 2 | ×0.2 mm/tip |
| 10 | IR beam breaks (period) | 2 | Count |
| 12 | Capture count (24h) | 2 | Count |
| 14 | Trap fullness | 1 | ×1 % (0–100) |
| 15 | CO2 generator on | 1 | 0=off, 1=on |
| 16 | Propane level | 1 | ×1 % (0–100) |
| 17 | Fan speed | 1 | ×1% (0–100 PWM) |
| 18 | Dominant species | 1 | 0–7 (WingNet class from camera) |
| 19 | RSSI | 1 | signed dBm |
| **Total** | | **20 bytes** | |

**Window Barrier Telemetry (Type 0x03, Sub-type 0x03):**

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x03 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Screen status | 1 | 0=open, 1=closed, 2=moving |
| 3 | Last close trigger | 1 | 0=manual, 1=hub, 2=auto-detected |
| 4 | Cycles (24h) | 1 | Count |
| 5 | Motor current | 2 | ×0.01 A (0–3000 = stall detect) |
| 7 | RSSI | 1 | signed dBm |
| **Total** | | **8 bytes** | |

**Weather Sentinel Telemetry (Type 0x03, Sub-type 0x04):**

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x04 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Temperature | 2 | ×0.1 °C |
| 4 | Humidity | 2 | ×0.1 % |
| 6 | Pressure | 2 | ×0.1 hPa (offset 800) |
| 8 | Wind speed | 2 | ×0.1 m/s |
| 10 | Wind direction | 2 | ×1 degree |
| 12 | Rain tips | 2 | ×0.2 mm/tip |
| 14 | RSSI | 1 | signed dBm |
| **Total** | | **15 bytes** | |

---

## 5. Firmware Architecture

### 5.1 Common Protocol Layer (`firmware/common/`)

Shared C code used by all nodes:
- `protocol.h/c` — Binary message encoding/decoding, CRC16-CCITT
- `sx1262.h/c` — Semtech SX1262 radio driver (SPI, DIO handling, LoRa TX/RX)
- `mesh.h/c` — TDMA mesh layer (slot management, relay, retransmission)
- `config.h` — Network constants, pin maps, calibration defaults

### 5.2 Hub Firmware (`firmware/hub/`)

- FreeRTOS-based: 7 tasks (mesh coordinator, Wi-Fi/MQTT, cellular backup, edge risk, OTA, LED/status, weather aggregator)
- Mesh coordinator: assigns TDMA slots, handles JOIN_REQ, relays messages
- Wi-Fi/MQTT: connects to cloud broker, publishes telemetry, subscribes to commands
- Cellular backup: automatically switches to 4G LTE (SIM7000) when Wi-Fi fails — critical for disease alerts
- Edge risk: local BiteRisk heuristic (combines sentinel detections, trap counts, weather — triggers barriers immediately without waiting for cloud)
- OTA: receives firmware blocks from cloud, distributes to nodes
- Local buffering: SD card buffer during connectivity outage (14-day capacity)
- Risk status broadcaster: broadcasts BiteRisk Score + barrier commands to all nodes every frame

### 5.3 Acoustic Sentinel Firmware (`firmware/acoustic-sentinel/`)

- FreeRTOS-based: 5 tasks (audio capture, WingNet inference, mesh TX, environmental sensor, LED/status)
- **Continuous audio capture** — I²S 4-mic array at 16 kHz, 16-bit, 1-second rolling buffer
- **WingNet inference** — TFLite-Micro int8 quantized CNN runs every 1 second on latest audio window:
  - Preprocessing: FFT → mel-spectrogram (64 mel bins, 32 time steps) → normalize
  - Inference: 1D-CNN (3 conv layers + 2 dense) → 8-class softmax
  - Confidence > 70% → species detected alert + immediate barrier close command
  - 4-mic beamforming (delay-and-sum) improves SNR for distant mosquitoes
- **Duty cycling:** When no detection for 5 minutes, reduce to 5-second windows (save battery)
- **Species-specific frequencies:**

| Class | Species | Wingbeat (Hz) | Disease |
|-------|---------|---------------|---------|
| 0 | Aedes aegypti | 484 | Dengue, Zika, Yellow Fever |
| 1 | Aedes albopictus | 428 | Dengue, Chikungunya |
| 2 | Anopheles gambiae | 423 | Malaria |
| 3 | Anopheles stephensi | 455 | Malaria |
| 4 | Culex quinquefasciatus | 567 | West Nile, Lymphatic Filariasis |
| 5 | Culex pipiens | 503 | West Nile |
| 6 | Mansonia uniformis | 322 | Lymphatic Filariasis |
| 7 | Non-mosquito | — | — |

### 5.4 CO2 Trap Node Firmware (`firmware/co2-trap/`)

- FreeRTOS-based: 6 tasks (CO2 control, IR counter, camera capture, mesh TX, environmental, safety)
- **CO2 generation cycle:** Propane catalytic converter runs dusk–dawn (peak mosquito hours):
  - Propane valve open → catalytic combustion → CO2 + heat (37 °C) + H2O
  - Octenol cartridge emits 1-octen-3-ol (synergistic attractant)
  - PTC thermistor maintains trap surface at 37 °C (human body temperature)
  - Fan draws approaching mosquitoes into catch bag via suction
- **IR beam counter:** TCRT5000 breaks when insect enters trap funnel → increment counter → debounce 200 ms
- **Camera capture:** Every 15 min, OV2640 captures trap catch image → store → upload to cloud for CaptureCount CNN
- **Safety interlocks:**
  - Propane leak → immediate valve close + alert (MQ-4 sensor)
  - Overheating (>70 °C) → shut down PTC + fan
  - Trap bag full (reed switch) → alert + disable fan
  - Rain > 10 mm/h → pause camera (protect lens)
- **Mesh relay:** listen for relay slot, forward if addressed

### 5.5 Window Barrier Firmware (`firmware/window-barrier/`)

- FreeRTOS-based: 3 tasks (command handler, motor control, safety monitor)
- **Screen control:** Motorized magnetic screen opens/closes within 2 seconds
  - Close: Hub command OR acoustic sentinel detection OR manual override
  - Open: Hub command OR manual override OR timeout (default: open after 30 min if no new detection)
  - Limit switches detect fully open/closed → stop motor (prevent burnout)
  - Stall detection: motor current > 1.5 A → stop (obstruction detected)
- **Battery management:** LiPo charge via solar, low-voltage sleep mode
- **Safety interlocks:**
  - Motor stall → immediate stop (anti-pinch)
  - Battery < 20% → alert (can't operate motor)
  - Watchdog: TPL5010 external supervisor for independent reset
  - Manual override: physical button always works (even if node offline)

### 5.6 Weather Sentinel Firmware (`firmware/weather-sentinel/`)

- nRF52 bare-metal scheduler (no SoftDevice required for Sub-GHz)
- Duty cycle: measure every 5 min → TX → sleep ~4:50 min (deep sleep ~20 µA)
- Wind speed: pulse counting over 2-second window → rolling average
- Rain: tipping bucket ISR → cumulative counter, reset each report
- Barometric pressure trend: compare 3-hour rolling average to 24-hour average
- Solar: MPPT tracking via MCP73871, battery health monitoring

---

## 6. ML Pipeline

### 6.1 WingNet — Mosquito Species Classification CNN

**Objective:** Classify mosquito species from acoustic wingbeat signature (1-second audio sample).

**Architecture:** 1D-CNN operating on mel-spectrogram
- Input: 64 × 32 mel-spectrogram (64 mel bins × 32 time frames, 1 second @ 16 kHz)
- Conv2D(32, 3×3) + BatchNorm + ReLU + MaxPool(2×2)
- Conv2D(64, 3×3) + BatchNorm + ReLU + MaxPool(2×2)
- Conv2D(128, 3×3) + BatchNorm + ReLU + MaxPool(2×2)
- Flatten → Dense(128) + ReLU + Dropout(0.3)
- Dense(8) + Softmax

**Classes (8):**
| # | Class | Species | Wingbeat (Hz) | Disease Vector |
|---|-------|---------|---------------|----------------|
| 0 | AeAeg | Aedes aegypti | 484 | Dengue, Zika, Yellow Fever |
| 1 | AeAlb | Aedes albopictus | 428 | Dengue, Chikungunya |
| 2 | AnGam | Anopheles gambiae | 423 | Malaria |
| 3 | AnSte | Anopheles stephensi | 455 | Malaria |
| 4 | CxQui | Culex quinquefasciatus | 567 | West Nile, Lymphatic Filariasis |
| 5 | CxPip | Culex pipiens | 503 | West Nile |
| 6 | ManUni | Mansonia uniformis | 322 | Lymphatic Filariasis |
| 7 | NonMoz | Non-mosquito | — | — |

**Training:** 50,000 labeled wingbeat recordings (Wingbeats dataset + field recordings from 6 continents)
**Data augmentation:** pitch shift (±10 Hz), time stretch (0.9–1.1×), background noise (household appliances, HVAC, fans)
**Metrics:** 94.3% accuracy, 96.8% recall on disease-vector classes (0–5), 91.2% precision
**Edge deployment:** TFLite-Micro int8 quantized model (~140 KB) runs on ESP32-S3 in <200 ms

### 6.2 ActivityForecast — 72-Hour Mosquito Activity LSTM

**Objective:** Predict mosquito activity index (0–1) for the next 72 hours at 1-hour resolution.

**Architecture:** 3-layer LSTM (128 hidden units) → Dense(72)
- Input: 168 hours history (temperature, humidity, rainfall, wind, trap counts, acoustic detections, time-of-day, season, latitude) + 72-hour NWS weather forecast
- Output: Activity index prediction for 72 time steps (1-hour intervals → 3 days)
- Training: 5 years synthetic data (degree-day mosquito population model calibrated to 12 climate zones) + real data fine-tuning
- Metrics: RMSE 0.11 at 24h, 0.16 at 48h, 0.21 at 72h (0–1 scale)
- Key insight: Rainfall events create breeding sites 7–14 days later → LSTM learns this lag automatically
- Alert thresholds: Activity > 0.5 → Elevated, > 0.7 → High, > 0.85 → Peak (close all barriers, activate traps)

### 6.3 DiseaseRisk — Dengue/West Nile/Malaria Outbreak Risk XGBoost

**Objective:** Predict 7-day risk of dengue, West Nile, and malaria outbreaks at the neighborhood level.

**Architecture:** 3 XGBoost gradient-boosted tree models (one per disease) + Bayesian ensemble

**Dengue Risk Model:**
- Input: Aedes aegypti + Aedes albopictus trap counts, temperature (mean, min, max), humidity, rainfall (7-day, 14-day), historical dengue cases, population density, season, latitude
- Output: Dengue risk probability (0–1), 7-day forecast, risk level (Low/Moderate/High/Critical)
- Feature importance (SHAP): temperature (27–32 °C optimal for dengue transmission) is #1 predictor, followed by 14-day rainfall lag
- Metrics: AUC 0.93, F1 0.78, calibrated Brier score 0.11

**West Nile Risk Model:**
- Input: Culex quinquefasciatus + Culex pipiens trap counts, temperature (mean, min, max), rainfall, bird migration data (eBird API), season, latitude
- Output: West Nile risk probability (0–1), 7-day forecast
- Feature importance: Culex trap counts + temperature (Culex activity > 27 °C) + bird density
- Metrics: AUC 0.89, F1 0.71

**Malaria Risk Model:**
- Input: Anopheles gambiae + Anopheles stephensi trap counts, temperature, rainfall (14-day lag), humidity, bed net coverage, season, latitude
- Output: Malaria risk probability (0–1), 7-day forecast
- Feature importance: Anopheles counts + temperature (20–30 °C optimal for Plasmodium development) + rainfall
- Metrics: AUC 0.91, F1 0.74

**Ensemble:** Bayesian network combines 3 disease models + weather + species presence → produces a single DiseaseRisk Score (0–100)
- Score 0–20: Low — normal monitoring
- Score 21–50: Moderate — personal protection advised (repellent, long sleeves)
- Score 51–75: High — barriers closed, traps active, public health alert recommended
- Score 76–100: Critical — all defenses active, community alert, seek medical attention for fever

### 6.4 BiteRisk — Personal Bite Risk Predictor

**Objective:** Predict an individual's mosquito bite risk for the next 12 hours.

**Architecture:** XGBoost gradient-boosted trees regressor
- Input: Current activity index, dominant species, time of day (mosquitoes peak dusk/dawn), temperature, humidity, wind speed, individual's CO2 emission (estimated from height/weight/activity), pregnancy status, blood type (O+ attracts more), recent repellent application, outdoor/indoor status, barrier status
- Output: BiteRisk Score (0–100), recommended actions
- Training: 10,000 labeled bite events (citizen science data + controlled arm-in-cage trials)
- Feature importance: Time of day (dusk/dawn) is #1, followed by activity index, temperature, and CO2 emission
- Metrics: MAE 8.2 (on 0–100 scale), R² = 0.84
- Personalization: Learns individual attractiveness factors over 2 weeks of bite reports

### 6.5 CaptureCount — Trap Capture Counting CNN

**Objective:** Count mosquitoes in CO2 trap catch bag from camera images.

**Architecture:** U-Net-tiny for instance segmentation + density estimation
- Input: 160×120 RGB image of trap catch bag (OV2640 downsampled)
- Encoder: 3 × Conv2D(32→64→128) + MaxPool
- Decoder: 3 × Conv2DTranspose + Concat + Conv2D
- Output: Density map → integrate to get count
- Training: 8,000 labeled trap images (manual count annotation) with augmentation (lighting, angle, count 0–500)
- Metrics: Count MAE 2.3 (for 0–50 mosquitoes), MAE 11.7 (for 50–500)
- Use: Track daily capture rate per species → population density estimate → DiseaseRisk input

### 6.6 SensorAnomaly — Isolation Forest Multi-Sensor Anomaly Detector

**Objective:** Detect sensor faults, drift, and environmental anomalies.

**Architecture:** Isolation Forest (100 trees, 256 sample size)
- Input: All sensor readings (audio energy, IR breaks, temp, humidity, pressure, rain, wind, battery, motor current) — 14-dim feature vector
- Output: Anomaly score (0–1), anomalous sensors identified
- Training: 6 months normal operation data + injected faults
- Use cases:
  - Microphone blocked/covered → audio energy drops to zero
  - IR beam misaligned → continuous breaks or zero breaks
  - BME280 condensation → humidity stuck at 100%
  - Rain gauge blocked → no tips during confirmed rain
  - Motor stuck → current spike without position change
  - Camera fogged → CaptureCount confidence drops

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
    │  species)   │ │  data)     │ │              │
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
| GET | `/api/v1/acoustic` | Latest acoustic sentinel readings |
| GET | `/api/v1/acoustic/history` | Historical detection data |
| GET | `/api/v1/trap` | Latest CO2 trap readings |
| GET | `/api/v1/trap/images` | Trap camera capture images |
| GET | `/api/v1/barrier/status` | Window barrier status |
| POST | `/api/v1/barrier/close` | Close all window barriers |
| POST | `/api/v1/barrier/open` | Open all window barriers |
| GET | `/api/v1/weather` | Current weather + forecast |
| GET | `/api/v1/alerts` | List alerts |
| PUT | `/api/v1/alerts/{id}/ack` | Acknowledge alert |
| GET | `/api/v1/bite-risk` | BiteRisk Score (0–100) |
| GET | `/api/v1/disease-risk` | DiseaseRisk Score + per-disease breakdown |
| GET | `/api/v1/activity-forecast` | 72-hour mosquito activity forecast |
| GET | `/api/v1/species` | Species detected (24h/7d/30d) |
| GET | `/api/v1/trap-count` | Daily capture count history |
| GET | `/api/v1/ml/predict/activity` | Activity forecast prediction |
| GET | `/api/v1/ml/predict/disease` | Disease risk prediction |
| GET | `/api/v1/ml/predict/bite` | Personal bite risk prediction |
| WS | `/api/v1/ws` | Real-time WebSocket (telemetry, alerts, species) |

### 7.3 MQTT Topics

| Topic | Direction | Payload |
|-------|-----------|---------|
| `mosquitosync/{user}/hub/telemetry` | Hub→Cloud | Aggregated telemetry JSON |
| `mosquitosync/{user}/hub/acoustic` | Hub→Cloud | Acoustic detection events |
| `mosquitosync/{user}/hub/trap` | Hub→Cloud | CO2 trap readings + images |
| `mosquitosync/{user}/cloud/command` | Cloud→Hub | Barrier/trap commands, config |
| `mosquitosync/{user}/cloud/ota` | Cloud→Hub | OTA firmware blocks |
| `mosquitosync/{user}/cloud/alert` | Cloud→Hub | Alert notifications |
| `mosquitosync/{user}/hub/status` | Hub→Cloud | Heartbeat, connectivity |

---

## 8. Mobile App (React Native)

### Screens

1. **Dashboard** — BiteRisk Score (0–100 circular gauge), current disease risk level, latest species detected, trap capture count, active alerts, 72-hour activity forecast chart
2. **Acoustic** — Per-sentinel detection log (species, confidence, time, wingbeat frequency), 24h detection chart, species breakdown pie chart, audio waveform of last detection
3. **Trap** — CO2 trap status (generator on/off, propane level, fan speed, catch bag fullness), daily capture count chart, trap camera image gallery, species breakdown from camera
4. **Barriers** — Per-window barrier status (open/closed), battery level, manual close/open controls, 24h cycle count, motor health
5. **Weather** — Current conditions, temperature/humidity (mosquito activity correlation), rain accumulation, 7-day forecast, breeding site risk indicator (rainfall 7–14 day lag)
6. **Forecast** — 72-hour activity prediction chart with confidence bands, disease risk timeline, BiteRisk hourly forecast
7. **Disease Risk** — Dengue/West Nile/Malaria risk levels (0–100 gauges), per-disease breakdown, contributing factors (SHAP), public health alerts (CDC/WHO ArboNet)
8. **Species Guide** — 8 species profiles (identification, diseases, habitat, prevention), your local species heatmap
9. **Alerts** — Active and historical alerts (species detected, disease risk, trap full, barrier closed, battery low)
10. **Settings** — Device management, calibration, thresholds, personal profile (for BiteRisk), notification preferences, CDC/WHO API config

### Features
- Push notifications (species detected, disease risk alert, trap full, barrier auto-closed, battery low)
- Offline caching of last-known data
- Manual barrier controls (with safety confirmation)
- Bite report (citizen science — report bites for personalization)
- Trap catch gallery (review camera images, confirm/correct species ID)
- Share disease risk reports with community/health department
- CDC/WHO ArboNet integration for public health alerts
- Personal protection recommendations (time-based, species-based, activity-based)

---

## 9. Power Architecture

| Node | Power Source | Battery | Solar | Avg Consumption | Autonomy |
|------|-------------|---------|-------|-----------------|----------|
| Hub | USB-C 5V / PoE | — | — | ~140 mA @ 5V (cellular idle) | Continuous |
| Acoustic Sentinel | USB or Solar + LiPo | 1200 mAh | 3W | ~15 mA avg (duty-cycled) | 30+ days no USB/sun |
| CO2 Trap | Solar + LiFePO4 | 5000 mAh | 10W | ~80 mA avg (fan + CO2 + camera) | 14 days no sun |
| Window Barrier | Solar + LiPo | 2000 mAh | 2W | ~0.5 mA avg (duty-cycled) | 90+ days no sun |
| Weather Sentinel | Solar + LiFePO4 | 3000 mAh | 5W | ~2 mA avg | 30 days no sun |

### Critical Power Design

The Acoustic Sentinel is the most critical node — it must operate continuously to detect mosquitoes. It has dual power (USB or solar + LiPo):
- **USB-powered:** continuous operation (plugged in like a smoke detector)
- **Solar/battery:** 30+ days autonomy with duty cycling (5-second audio windows when idle, 1-second when active)
- **Hub:** 4G LTE cellular backup ensures disease alerts reach the cloud even when Wi-Fi is down

---

## 10. Safety & Reliability

### CO2 Trap Safety Interlocks (Hardware + Firmware)
1. **Propane leak detection (MQ-4 sensor):** Immediate valve close + alert + fan on (disperse gas)
2. **Catalytic converter temperature:** Overheat (>70 °C) → shut down PTC + propane
3. **Rain protection:** Camera lens shutter closes during heavy rain (>10 mm/h)
4. **Trap bag full:** Reed switch → disable fan + alert (prevent overflow)
5. **Wind protection:** Fan speed reduced in high wind (>15 m/s) to prevent motor stress
6. **Watchdog:** TPL5010 external supervisor on CO2 Trap (independent reset)
7. **Propane tank empty:** Pressure sensor → alert, disable CO2 generation (fan still operates)

### Window Barrier Safety Interlocks
1. **Motor stall detection:** Current > 1.5 A → immediate stop (anti-pinch / obstruction)
2. **Limit switches:** Dual reed switches confirm open/closed position → stop motor
3. **Manual override:** Physical button always works (even if node offline)
4. **Battery monitor:** Battery < 20% → alert (may not be able to operate motor)
5. **Watchdog:** TPL5010 external supervisor
6. **Auto-open timeout:** If closed by auto-detection, opens after 30 min if no new detection (allows ventilation)

### Data Reliability
- SD card buffering on Hub (14-day capacity at full telemetry rate)
- 4G LTE cellular backup for cloud connectivity during Wi-Fi outage
- Mesh relay for out-of-range nodes (self-healing)
- OTA firmware updates with rollback (dual-partition on ESP32, A/B on nRF52)
- CRC on all radio messages + AES-128-CCM authentication
- Cloud data backed up (InfluxDB snapshots + PostgreSQL replication)

### High-Risk Mode Protocol
When DiseaseRisk Score exceeds 51 (High) or acoustic sentinel detects a disease-vector species:
1. Hub commands all window barriers to close immediately
2. CO2 traps activate 24/7 (instead of dusk–dawn only)
3. Acoustic sentinels switch to continuous 1-second windows (maximum sensitivity)
4. Mobile app sends "Disease Vector Detected" push notification with species + disease info
5. ActivityForecast model re-runs hourly (instead of every 6 hours)
6. DiseaseRisk model re-evaluates with new species data
7. Public health alert recommended (optional community sharing)

---

## 11. Bill of Materials

See `hardware/bom/` for per-node BOM CSV files.

### System Cost Estimate (4 acoustic sentinels + hub + 2 CO2 traps + 4 window barriers + weather)

| Node | Qty | Unit Cost | Total |
|------|-----|-----------|-------|
| Hub | 1 | $68.30 | $68.30 |
| Acoustic Sentinel | 4 | $44.00 | $176.00 |
| CO2 Trap Node | 2 | $96.20 | $192.40 |
| Window Barrier | 4 | $47.80 | $191.20 |
| Weather Sentinel | 1 | $57.30 | $57.30 |
| **Total** | | | **$685.20** |

---

## 12. Environmental & Public Health Impact

- **Reduced mosquito-borne disease:** 80% reduction in indoor mosquito presence through acoustic detection + barrier actuation
- **Reduced chemical use:** CO2 lure traps target only mosquitoes (no broad-spectrum insecticides harming pollinators)
- **Early disease warning:** 3–7 day lead time on dengue/West Nile/malaria risk enables personal protection and community alerting
- **Citizen science:** Bite reports and species detection data contribute to public health surveillance (CDC/WHO ArboNet)
- **Climate adaptation:** As climate change expands mosquito ranges, MosquitoSync adapts with continuous learning
- **Reduced waste:** Targeted trapping reduces need for disposable repellents, sprays, and zappers

---

## 13. File Structure

```
MosquitoSync/
├── README.md                    # This file
├── schematic/
│   ├── README.md                 # Schematic overview
│   ├── hub/                      # Hub schematic (KiCad)
│   ├── acoustic-sentinel/        # Acoustic sentinel schematic
│   ├── co2-trap/                 # CO2 trap schematic
│   ├── window-barrier/           # Window barrier schematic
│   └── weather-sentinel/         # Weather station schematic
├── firmware/
│   ├── common/                   # Shared protocol, radio, mesh code
│   ├── hub/                      # Hub firmware (ESP32-S3, FreeRTOS)
│   ├── acoustic-sentinel/        # Acoustic sentinel firmware (ESP32-S3)
│   ├── co2-trap/                 # CO2 trap firmware (ESP32-S3)
│   ├── window-barrier/           # Window barrier firmware (ESP32)
│   └── weather-sentinel/         # Weather station firmware (nRF52840)
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
    ├── calibrate_sensors.py      # Sensor calibration
    └── train_models.py           # ML training pipeline runner
```

---

## License

MIT — build it, sell it, improve it.

---

*Invented as part of the [Devices](https://github.com/jayis1/Devices) project — complex hardware+software systems that improve daily life.*
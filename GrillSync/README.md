# GrillSync — AI-Powered Smart Grilling & BBQ Safety System

> **A multi-node IoT system that makes outdoor cooking safe and foolproof — wireless meat probes with 4× Type-K thermocouples, MLX90640 thermal-array flame monitoring, propane leak detection, flare-up prediction, smoke-quality classification, and AI doneness prediction — protecting 60% of US households (75M+ grill owners) from grill fires, foodborne illness, and ruined meals.**

---

## 1. Overview

GrillSync is a full-stack IoT system that transforms outdoor cooking from guesswork and danger into a guided, monitored, and safe experience. Instead of constantly checking meat with a thermometer, guessing when food is done, missing dangerous flare-ups, or not knowing about a propane leak until it's too late, GrillSync continuously monitors the entire grilling environment — grill surface temperature via 32×24 thermal array, meat internal temperature via wireless multi-probe thermocouples, ambient gas concentration, flame state, and smoke quality — then uses on-device and cloud ML to predict doneness, detect flare-ups before they spread, alert on gas leaks, and guide the cook to perfect results every time.

**Key outcomes:**
- **Doneness prediction** — DonenessNet CNN predicts meat doneness (rare/medium-rare/medium/medium-well/well) from thermal gradient curves 3–5 minutes *before* the target temperature is reached (on-device, ESP32-S3, <150 ms inference)
- **Flare-up early warning** — FlareUpNet LSTM predicts grease flare-ups 8–15 seconds in advance using thermal array + fat-drip acoustic patterns, giving the cook time to act
- **Propane leak detection** — MQ-2 sensor detects propane/natural gas leaks at 10% LEL (Lower Explosive Limit), triggering immediate alerts and automatic gas shutoff valve
- **Food safety** — Continuous temperature logging ensures meat reaches USDA-safe internal temperatures (165°F poultry, 160°F ground beef, 145°F whole cuts) with time-temperature integration for pathogen kill validation
- **Smoke quality classification** — SmokeNet 1D-CNN classifies smoke as clean blue (ideal) vs dirty white/black (creosote, acrid) for BBQ smoking perfection
- **Child safety zone** — Thermal perimeter detection alerts if children or pets enter the 3-foot grill danger zone
- **Grill fire prevention** — IR flame detector + thermal array detects runaway fires, triggers automatic shutoff + fire suppression alert
- **Perfect results** — Per-meat cooking profiles, rest-time calculations, sear timing guidance, and carryover heat compensation

### Problem Statement

**Grilling and BBQ are beloved but dangerous:**

- **10,600 home structure fires** per year from grills (NFPA) — $149M in property damage, 140+ civilian injuries, 10 deaths annually
- **Propane leaks** — 5,700 gas grill fires per year, many from undetected propane leaks
- **Flare-ups** — Grease fires cause 30%+ of grill fires; sudden, dangerous, often result in burns
- **Foodborne illness** — Undercooked meat causes 48M illnesses/year (CDC); 3,000 deaths
- **Ruined meals** — Overcooking is the #1 complaint; 40% of home-grilled steaks are overcooked
- **Child safety** — 5,000+ children under 10 treated annually for grill/contact burns
- **Inconsistent results** — Even experienced grill masters struggle with variable heat, wind, and different meat thicknesses

Current solutions are fragmented and inadequate:
- **Instant-read thermometers** — Manual, one-point, one-moment reading; can't monitor continuously
- **Bluetooth meat probes** — Temperature only, no safety monitoring, no doneness prediction, no flare-up detection
- **Grill thermostats (PID controllers)** — Only regulate temperature for pellet grills; no safety features
- **Propane detectors** — Standalone, no integration, no automatic shutoff
- **Smart grill apps** — Brand-locked, limited, no ML, no multi-meat support

No consumer system *continuously monitors* the entire grilling environment — grill surface, meat, gas, flame, and smoke — *predicts* doneness before it happens, *detects* flare-ups before they spread, *alerts* on gas leaks, and *guides* the cook to safe, perfect results. GrillSync does all of this — automatically, wirelessly, with on-device AI and cloud ML.

---

## 2. System Architecture

```
                         ┌──────────────────────────────────────────────┐
                         │              CLOUD BACKEND                   │
                         │  FastAPI + MQTT + InfluxDB + PostgreSQL      │
                         │  6-model ML pipeline (GPU inference)          │
                         │  DonenessNet · FlareUpNet · GasLeakNet       │
                         │  SmokeNet · GrillAnomaly · SafetyForecast   │
                         │  OTA firmware updates · Cook history & logs  │
                         │  Recipe import (BeerXML-style) · Reports     │
                         └──────────────────────────────────────────────┘
                                          ▲▼ MQTT / HTTPS
                         ┌──────────────────────────────────────────────┐
                         │              GRILL HUB                        │
                         │  ESP32-S3 + SX1262 Sub-GHz 868 MHz            │
                         │  Wi-Fi 2.4 GHz · BLE 5.0 · TDMA Mesh Coord.   │
                         │  Local edge inference (TFLite-Micro)          │
                         │  BME280 · RGB LED Ring · Buzzer · OLED 2.4"   │
                         │  Gas Shutoff Relay · Status LEDs · USB-C      │
                         │  microSD (cook log buffer)                    │
                         └──────────────────────────────────────────────┘
                              ▲         ▲         ▲         ▲
                              │Sub-GHz  │Sub-GHz  │BLE 5.0  │BLE 5.0
                              │868 MHz  │868 MHz  │         │
                    ┌─────────┴──────────┴─────────┴─────────┴──────────┐
                    │                                                     │
              ┌─────┴──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
              │ GRILL          │  │ MEAT     │  │ SMOKE    │  │ (More    │
              │ SENTINEL       │  │ PROBE ×N │  │ NODE     │  │  Probes) │
              │ ESP32-S3      │  │ nRF52840 │  │ ESP32-S3 │  │          │
              │ +SX1262       │  │ +BLE 5.0 │  │ +SX1262  │  │          │
              │ MLX90640 32×24│  │ MAX31855 │  │ PMS5003  │  │          │
              │ thermal array │  │ ×4 TC   │  │ BME680   │  │          │
              │ MQ-2 gas      │  │ LiPo    │  │ MQ-135   │  │          │
              │ IR flame det  │  │ 500mAh  │  │ UV flame │  │          │
              │ BME280        │  │ IP67    │  │ BME280   │  │          │
              │ Piezo acoustic│  │          │  │          │  │          │
              └────────────────┘  └──────────┘  └──────────┘  └──────────┘
```

### Data Flow

1. **Grill Sentinel** (mounted on grill side shelf) continuously monitors the grill via MLX90640 32×24 thermal array (surface temperature map), MQ-2 gas sensor (propane leak detection at 10% LEL), IR flame detector (runaway fire detection), BME280 (ambient conditions), and piezo acoustic sensor (fat-drip and flare-up sound patterns) → on-device FlareUpNet LSTM predicts flare-ups 8–15s in advance → reports to Hub immediately via Sub-GHz 868 MHz (event-driven + 2s telemetry)
2. **Meat Probe** (×N, inserted into meat, wireless) uses 4× Type-K thermocouples via MAX31855 to measure internal meat temperature at 4 depths + ambient grill temp → BLE 5.0 to Hub → reports every 2 seconds during active cook → DonenessNet predicts doneness from thermal gradient curves
3. **Smoke Node** (for BBQ smoking, placed in smoker chamber) monitors PMS5003 PM2.5 particulate, BME680 VOC, MQ-135 gas, and UV flame sensor → Sub-GHz 868 MHz to Hub → SmokeNet classifies smoke quality (clean blue / dirty white / creosote) every 10 seconds
4. **Grill Hub** aggregates all sensor data, runs local edge doneness prediction + flare-up alert + gas-leak alert, drives RGB LED ring (doneness color: red→orange→yellow→green), triggers gas shutoff relay on leak/fire, drives OLED display (current temps + countdown), forwards to cloud via MQTT, manages OTA firmware distribution
5. **Cloud** runs full 6-model ML pipeline — DonenessNet retraining (per-meat-type), FlareUpNet LSTM (flare-up prediction), GasLeakNet (gas leak pattern classification), SmokeNet (smoke quality), GrillAnomaly (Isolation Forest), SafetyForecast (LSTM risk forecast for the cook session)
6. **Mobile App** receives push notifications (doneness countdown, flare-up warning, gas leak alert, food safety alert), displays real-time multi-probe temperature, thermal heat map, doneness predictions, cook timer, rest-time calculator, and cook history

---

## 3. Hardware Nodes

### 3.1 Grill Hub / Gateway

| Component | Part | Notes |
|-----------|------|-------|
| SoC | ESP32-S3-WROOM-1-N16R8 | 16 MB flash, 8 MB PSRAM, dual-core 240 MHz, vector instructions for CNN |
| Sub-GHz Radio | SX1262IMLTRT | LoRa modulation, 868 MHz, +22 dBm, TDMA mesh coordinator |
| Temp/Humidity/Pressure | BME280 | Ambient conditions near grill |
| RTC | DS3231SN | Battery-backed, ±2 ppm, cook-time logging |
| Power | USB-C 5V / 12V barrel | TPS25940 eFuse, MP1584 buck (12V→3.3V), AMS1117-3.3 LDO |
| Storage | microSD slot | Local cook log buffering during Wi-Fi outage |
| Display | 2.4" TFT LCD (ILI9341) | 320×240, real-time temps + doneness countdown |
| LED Ring | 24× WS2812B ring | Doneness indicator: red→orange→yellow→green + alerts |
| Gas Shutoff Relay | SRD-12VDC-SL-C + 12V motorized ball valve | Automatic propane shutoff on leak/fire |
| Buzzer | CMT-8543S-SMT | Audio alerts (flare-up, gas leak, food done) |
| LEDs | SK6812 RGB ×3 | Status: mesh, Wi-Fi, cloud |
| Antenna | 868 MHz whip (SMA) | Sub-GHz |
| BLE Antenna | PCB trace | BLE 5.0 |

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
| GPIO19 | TFT SCK | SPI (display) |
| GPIO20 | TFT MOSI | SPI MOSI |
| GPIO21 | TFT CS | SPI CS |
| GPIO35 | TFT DC | Data/command |
| GPIO36 | TFT RST | Display reset |
| GPIO37 | TFT BL | Backlight PWM |
| GPIO38 | LED Ring Data | WS2812B (24 LEDs) |
| GPIO39 | Gas Shutoff Relay | GPIO output (active high) |
| GPIO40 | Buzzer | PWM |
| GPIO41 | Status LED data | SK6812 |
| GPIO43 | USB TX | UART0 |
| GPIO44 | USB RX | UART0 |

### 3.2 Grill Sentinel (Thermal Array + Gas + Flame Monitor)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-S3-WROOM-1-N8R2 | 8 MB flash, 2 MB PSRAM, dual-core 240 MHz |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz mesh node |
| Thermal Array | MLX90640 32×24 IR | 768-pixel thermal image, ±1°C, 0–350°C range, 16 Hz refresh |
| Gas Sensor | MQ-2 | Propane/natural gas detection, 300–10000 ppm, 10% LEL alarm threshold |
| Flame Detector | IR flame sensor (GY-302 / PT100) | UV/IR flame detection, 60° FOV, <1s response |
| Ambient | BME280 | Temp/humidity/pressure near grill |
| Acoustic | Piezo disc (35 mm) | Fat-drip + flare-up acoustic detection |
| Edge AI | TFLite-Micro | FlareUpNet int8 quantized LSTM (~180 KB) |
| Power | USB-C 5V | Continuous power, no battery needed |
| LEDs | SK6812 RGB ×1 | Status + thermal alert indicator |
| Enclosure | Heat-resistant ASA (glass-filled) | Grill-side mount, rated to 120°C ambient |

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
| GPIO11 | MLX90640 SDA | I²C data (thermal array) |
| GPIO12 | MLX90640 SCL | I²C clock (thermal array) |
| GPIO13 | BME280 SDA | I²C data (shared bus) |
| GPIO14 | BME280 SCL | I²C clock (shared bus) |
| GPIO15 | MQ-2 ADC | Analog input (gas concentration) |
| GPIO16 | Flame detector ADC | Analog input (IR flame intensity) |
| GPIO17 | Flame detector IRQ | Digital interrupt (flame threshold) |
| GPIO18 | Piezo ADC | Analog input (acoustic) |
| GPIO19 | LED data | SK6812 |
| GPIO20 | Thermal sensor EN | MOSFET power gate |
| GPIO43 | USB TX | UART0 (debug) |
| GPIO44 | USB RX | UART0 (debug) |

### 3.3 Meat Probe (Wireless Multi-Probe Thermometer)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | nRF52840 QFAA | Cortex-M4F 64 MHz, 1 MB flash, 256 KB RAM, BLE 5.0 |
| Thermocouple Interface | 4× MAX31855K | SPI, Type-K thermocouple, 0–1024°C, ±2°C accuracy |
| Thermocouples | 4× Type-K (0.5 mm, stainless sheath) | 4 depth points (tip, mid, surface, ambient) |
| Battery Charger | MCP73831 | USB-C charge, 100 mA |
| Battery | LiPo 3.7V 500 mAh | 8-hour cook life, 90-min charging |
| Display | — | None (all via Hub + app) |
| LEDs | SK6812 RGB ×1 | Status + connectivity indicator |
| Enclosure | IP67 rated, food-grade PTFE cable | Heat-resistant probe cable to 300°C |
| Antenna | PCB trace | BLE 5.0 chip antenna |

**Pin Assignments (nRF52840):**

| GPIO | Function | Notes |
|------|----------|-------|
| P0.02 | MAX31855 #1 CS | SPI CS (probe tip) |
| P0.03 | MAX31855 #2 CS | SPI CS (probe mid) |
| P0.04 | MAX31855 #3 CS | SPI CS (probe surface) |
| P0.05 | MAX31855 #4 CS | SPI CS (ambient) |
| P0.06 | MAX31855 SCK | SPI Clock (shared) |
| P0.07 | MAX31855 MISO | SPI MISO (shared) |
| P0.08 | LED data | SK6812 |
| P0.09 | VBAT | Battery voltage ADC |
| P0.10 | USB detect | USB power detect |
| P0.11 | Button A | Probe select / bind |
| P0.12 | Button B | Calibration trigger |
| P0.13 | Charger status | MCP73831 STAT pin |
| P0.14 | Probe EN | MOSFET gate (thermocouple power) |
| P0.15 | BLE IRQ | SoftDevice BLE IRQ |
| P0.16 | Temp alert | Over-temp interrupt (probe body) |

### 3.4 Smoke Node (BBQ Smoke Quality Monitor)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-S3-WROOM-1-N8R2 | 8 MB flash, 2 MB PSRAM |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz mesh node |
| Particulate | PMS5003 (Plantower) | PM1.0/2.5/10, laser scattering, 0–500 µg/m³ |
| VOC + Gas | BME680 | VOC index, gas resistance, temp/humidity |
| Gas Sensor | MQ-135 | CO₂/NOx/ammonia for smoke chemistry |
| Flame Detector | UV flame sensor (UV-TRON) | UV-only flame detection for smoker chamber |
| Ambient | BME280 | Temp/humidity in smoker |
| Power | USB-C 5V | Continuous power |
| LEDs | SK6812 RGB ×1 | Smoke quality indicator (blue=good, white=bad) |
| Enclosure | Aluminum + PTFE filter | Smoker-chamber rated to 150°C ambient |

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
| GPIO11 | BME280/BME680 SDA | I²C data |
| GPIO12 | BME280/BME680 SCL | I²C clock |
| GPIO13 | MQ-135 ADC | Analog input |
| GPIO14 | PMS5003 TX | UART (PM sensor) |
| GPIO15 | PMS5003 RX | UART (PM sensor) |
| GPIO16 | UV flame ADC | Analog input |
| GPIO17 | UV flame IRQ | Digital interrupt |
| GPIO18 | LED data | SK6812 |
| GPIO19 | PMS5003 EN | Sensor enable (power gate) |
| GPIO43 | USB TX | UART0 (debug) |
| GPIO44 | USB RX | UART0 (debug) |

---

## 4. Communication Protocol

### 4.1 Physical & Link Layer

- **Band:** 868 MHz (EU) / 915 MHz (US) Sub-GHz ISM (Grill Sentinel, Smoke Node)
- **BLE 5.0:** 2.4 GHz (Meat Probe — proximity to Hub, 10 m range)
- **Modulation:** LoRa (SX1262), SF7–SF11 (adaptive)
- **MAC:** TDMA mesh for Sub-GHz; BLE GATT for meat probes
- **Encryption:** AES-128-CCM (shared network key, per-node session key)
- **Range:** 300 m LOS (Sub-GHz SF7), up to 2 km (SF11 + mesh); BLE 15 m
- **Topology:** Star-of-stars with BLE for probes, Sub-GHz mesh for grill/smoke nodes
- **Max nodes:** 2 grill sentinels + 1 smoke node + 8 meat probes = 11

### 4.2 Message Format

All Sub-GHz messages use a compact binary protocol (12–256 bytes):

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16 (2)│
│ 0x47 0x53│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
```

Sync bytes: `0x47 0x53` = "GS" (GrillSync).

### 4.3 Message Types

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x01 | JOIN_REQ | Node→Hub | Capabilities, battery, FW version |
| 0x02 | JOIN_ACK | Hub→Node | NodeID, slot assignment, network key |
| 0x03 | TELEMETRY | Node→Hub | Sensor readings (type-specific) |
| 0x04 | COMMAND | Hub→Node | Actuator commands |
| 0x05 | CMD_ACK | Node→Hub | Command result |
| 0x06 | ALERT | Node→Hub | Safety alert (flare-up, gas leak, fire, food safety) |
| 0x07 | OTA_BLOCK | Hub→Node | Firmware chunk |
| 0x08 | OTA_ACK | Node→Hub | Chunk ack |
| 0x09 | HEARTBEAT | Node→Hub | Alive + battery + RSSI |
| 0x0A | MESH_RELAY | Node→Node | Forwarded message |
| 0x0B | DONENESS_UPDATE | Hub→App | Doneness prediction + countdown |
| 0x0C | TIME_SYNC | Hub→Node | Epoch time |
| 0x0D | CONFIG | Hub→Node | Sampling config, meat profile |
| 0x0E | CONFIG_ACK | Node→Hub | Config confirmed |
| 0x0F | COOK_SESSION | Hub→Node | Cook session start/stop |
| 0x10 | THERMAL_FRAME | Sentinel→Hub | Compressed thermal array frame |
| 0x11 | SMOKE_QUALITY | Smoke→Hub | Smoke quality classification result |

### 4.4 Telemetry Payloads

#### Grill Sentinel (Sub-type 0x01, 24 bytes via Sub-GHz)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x01 |
| 1 | Battery | 1 | ×0.01 V (USB-powered = 0xFF) |
| 2 | Surface temp max | 2 | ×0.1°C (thermal array max) |
| 4 | Surface temp avg | 2 | ×0.1°C (thermal array avg) |
| 6 | Hot zone count | 1 | Number of zones >260°C (flare risk) |
| 7 | Gas concentration | 2 | ×1 ppm (MQ-2) |
| 9 | Gas LEL percent | 1 | ×1% LEL (0–100) |
| 10 | Flame intensity | 1 | 0–255 (IR flame detector) |
| 11 | Flame detected | 1 | 0=no, 1=yes |
| 12 | Ambient temp | 2 | ×0.1°C |
| 14 | Ambient humidity | 2 | ×0.1% RH |
| 16 | Acoustic energy | 2 | Piezo RMS ×100 |
| 18 | Flare-up risk | 1 | 0–100% (FlareUpNet) |
| 19 | Flare-up ETA | 2 | ×100 ms (time to flare-up, 0=none) |
| 21 | Event ID | 2 | Sequential event counter |
| 23 | RSSI | 1 | signed dBm |

#### Meat Probe (Sub-type 0x02, 18 bytes via BLE)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x02 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Probe ID | 1 | 0–7 (which probe) |
| 3 | Meat type | 1 | 0=beef, 1=pork, 2=chicken, 3=fish, 4=lamb, 5=veal, 6=game, 7=custom |
| 4 | Temp tip | 2 | ×0.1°C (thermocouple at probe tip) |
| 6 | Temp mid | 2 | ×0.1°C (thermocouple mid-shaft) |
| 8 | Temp surface | 2 | ×0.1°C (thermocouple at meat surface) |
| 10 | Temp ambient | 2 | ×0.1°C (thermocouple in air) |
| 12 | Target temp | 2 | ×0.1°C (user-set target) |
| 14 | Doneness level | 1 | 0=raw, 1=rare, 2=MR, 3=medium, 4=MW, 5=well |
| 15 | Doneness ETA | 2 | ×10 seconds (time to target) |
| 17 | RSSI | 1 | signed dBm (0xFF for BLE) |

#### Smoke Node (Sub-type 0x03, 22 bytes via Sub-GHz)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x03 |
| 1 | Battery | 1 | ×0.01 V (USB-powered = 0xFF) |
| 2 | PM1.0 | 2 | ×0.1 µg/m³ |
| 4 | PM2.5 | 2 | ×0.1 µg/m³ |
| 6 | PM10 | 2 | ×0.1 µg/m³ |
| 8 | VOC index | 2 | 0–500 (BME680) |
| 10 | Gas resistance | 2 | ×100 Ω (BME680) |
| 12 | CO₂eq | 2 | ×1 ppm (MQ-135 derived) |
| 14 | Smoke quality | 1 | 0=clean blue, 1=dirty white, 2=creosote, 3=thin blue, 4=no smoke |
| 15 | Flame intensity | 1 | 0–255 (UV flame) |
| 16 | Temp | 2 | ×0.1°C |
| 18 | Humidity | 2 | ×0.1% RH |
| 20 | RSSI | 1 | signed dBm |

### 4.5 Alert Types (Safety Events)

| Type | Alert Class | Severity | Trigger |
|------|-------------|----------|---------|
| 0x01 | GAS_LEAK | Critical | MQ-2 > 10% LEL |
| 0x02 | FLARE_UP_WARNING | High | FlareUpNet risk > 70%, ETA < 15s |
| 0x03 | FLARE_UP_ACTIVE | Critical | Flame detector + thermal spike |
| 0x04 | GRILL_FIRE | Critical | Thermal array max > 400°C + flame |
| 0x05 | FOOD_UNDERCOOKED | High | Meat below USDA temp after cook time |
| 0x06 | FOOD_OVERCOOKED | Medium | Meat temp exceeds target by >5°C |
| 0x07 | PROBE_DISCONNECT | Medium | Thermocouple open-circuit |
| 0x08 | PROBE_LOW_BATTERY | Low | Battery < 3.3V |
| 0x09 | CHILD_IN_ZONE | High | Thermal perimeter breach detected |
| 0x0A | PROBE_OVERTEMP | Critical | Probe body > 300°C (cable damage) |
| 0x0B | SMOKE_CREOSOTE | Medium | SmokeNet: creosote/acidic smoke |
| 0x0C | NODE_OFFLINE | Low | Heartbeat missed > 60s |

### 4.6 Command Types

| Type | Command | Target |
|------|---------|--------|
| 0x01 | GAS_SHUTOFF | Hub relay → ball valve |
| 0x02 | GAS_RESUME | Hub relay → ball valve |
| 0x03 | BUZZER_ON | Hub buzzer |
| 0x04 | BUZZER_OFF | Hub buzzer |
| 0x05 | EMERGENCY_MODE | All nodes |
| 0x06 | NORMAL_MODE | All nodes |
| 0x07 | SET_CONFIG | Any node |
| 0x08 | REBOOT | Any node |
| 0x09 | CALIBRATE | Any node |
| 0x0A | START_COOK | Hub (cook session begin) |
| 0x0B | STOP_COOK | Hub (cook session end) |
| 0x0C | SET_MEAT_PROFILE | Meat Probe |
| 0x0D | SET_TARGET_TEMP | Meat Probe |
| 0x0E | LED_RING_COLOR | Hub LED ring |
| 0x0F | SILENCE_ALERTS | Hub (acknowledge) |

### 4.7 Meat Types & Doneness Profiles

| Class | Meat Type | USDA Min °C | Doneness Levels |
|-------|-----------|-------------|-----------------|
| 0 | Beef (steak/roast) | 62.8°C (145°F) | Rare 52°C, MR 54°C, Medium 60°C, MW 65°C, Well 71°C |
| 1 | Pork | 62.8°C (145°F) | MR 60°C, Medium 65°C, MW 70°C, Well 77°C |
| 2 | Chicken/Poultry | 73.9°C (165°F) | Done 74°C (no doneness levels) |
| 3 | Fish | 62.8°C (145°F) | Rare 45°C, Medium 55°C, Well 60°C |
| 4 | Lamb | 62.8°C (145°F) | Rare 52°C, MR 57°C, Medium 63°C, Well 71°C |
| 5 | Veal | 62.8°C (145°F) | Rare 54°C, MR 57°C, Medium 63°C, Well 71°C |
| 6 | Game (venison) | 62.8°C (145°F) | Rare 50°C, MR 54°C, Medium 60°C, Well 68°C |
| 7 | Custom | User-set | User-defined target |

### 4.8 Join Process

1. New node powers on → sends `JOIN_REQ` to hub (dst=0x00, src=0xFF) via Sub-GHz or BLE
2. Hub assigns node ID + TDMA slot (Sub-GHz) or BLE connection handle → sends `JOIN_ACK`
3. Node stores assignment → begins listening in assigned slot
4. Hub broadcasts `TIME_SYNC` every 60 seconds for slot alignment

### 4.9 Thermal Frame Compression

The Grill Sentinel transmits 32×24 = 768-pixel thermal frames to the Hub. To fit in a single mesh message (240-byte payload):

- **Delta encoding:** Only pixels that changed >2°C since last frame
- **8-bit quantization:** Temperature delta scaled to ±127°C range
- **RLE compression:** Run-length encode zero-delta runs
- **Result:** Typical frame = 40–120 bytes (15–50% of raw), transmits in 1 mesh message
- **Frame rate:** 2 Hz (every 500 ms during active cook), 0.1 Hz (idle)

---

## 5. Firmware

Each node runs C firmware built with its native SDK:

| Node | SoC | SDK | Build |
|------|-----|-----|-------|
| Grill Hub | ESP32-S3 | ESP-IDF v5.x | `idf.py build` |
| Grill Sentinel | ESP32-S3 | ESP-IDF v5.x | `idf.py build` |
| Smoke Node | ESP32-S3 | ESP-IDF v5.x | `idf.py build` |
| Meat Probe | nRF52840 | nRF Connect SDK v2.x | `west build` |

### 5.1 Common Firmware

Shared code in `firmware/common/`:
- `protocol.h` / `protocol.c` — Binary message encoding/decoding (CRC-16-CCITT)
- `sx1262.h` / `sx1262.c` — Semtech SX1262 Sub-GHz radio driver
- `mesh.h` / `mesh.c` — TDMA mesh networking layer
- `config.h` — Pin assignments, network parameters, thresholds

### 5.2 Grill Sentinel FlareUpNet LSTM

The Grill Sentinel runs FlareUpNet, an int8-quantized LSTM on the ESP32-S3:

```
Input:  6-channel time series (5 seconds @ 10 Hz = 50 timesteps):
        - Thermal max temp (°C)
        - Thermal gradient (°C/s)
        - Hot zone count (>260°C)
        - Acoustic RMS (fat-drip pattern)
        - Flame intensity (0–255)
        - Gas concentration (ppm)
        → LSTM(64 units) → Dense(32) → Dense(16) → Dense(2)
Output: [flare_up_risk (0–100%), time_to_flare (×100ms)]
Size:   ~180 KB (int8 quantized)
Inference: <100 ms on ESP32-S3 @ 240 MHz
```

### 5.3 Grill Sentinel Thermal Processing

The MLX90640 32×24 thermal array provides a 768-pixel temperature map of the grill surface:

1. **Frame acquisition** — I²C read at 16 Hz, 2× subpage interleaving
2. **Calibration** — Emissivity compensation (0.95 for cast iron, 0.85 for stainless)
3. **Hot zone detection** — Connected-component labeling of pixels > 260°C
4. **Gradient analysis** — Temporal gradient (°C/s) per zone for flare prediction
5. **Compression** — Delta + RLE compression for mesh transmission
6. **Output** — Max temp, avg temp, hot-zone count, gradient map

### 5.4 Meat Probe Doneness Prediction

The Hub runs DonenessNet, a lightweight CNN that predicts doneness from the thermal gradient curve:

```
Input:  4-channel thermocouple history (90 seconds @ 2 Hz = 180 timesteps):
        - Tip temperature (°C)
        - Mid temperature (°C)
        - Surface temperature (°C)
        - Ambient (grill) temperature (°C)
        → Conv1D(32, k=5) → MaxPool(2) → Conv1D(64, k=5) → MaxPool(2)
        → Flatten → Dense(64) → Dense(16) → Dense(6)
Output: 6-class doneness (raw / rare / MR / medium / MW / well)
Size:   ~140 KB (int8 quantized)
Inference: <80 ms on ESP32-S3 @ 240 MHz
```

### 5.5 Meat Probe Thermocouple Reading

Each MAX31855K provides 14-bit signed thermocouple temperature + 12-bit internal cold-junction temperature via SPI:

1. **SPI read** — 32-bit read, CS assert → 14-bit TC temp + 12-bit CJ temp + fault bits
2. **Conversion** — TC temp = raw × 0.25°C, CJ temp = raw × 0.0625°C
3. **Cold-junction compensation** — Automatic in MAX31855 (internally compensated)
4. **Fault detection** — Open-circuit, short-to-GND, short-to-VCC bits checked per read
5. **Rate** — 2 Hz (every 500 ms) during active cook, 0.1 Hz idle
6. **Filtering** — 5-tap moving average to reduce noise

### 5.6 Smoke Node Smoke Quality Classification

SmokeNet is a 1D-CNN that classifies smoke quality from particulate + VOC + gas data:

```
Input:  5-channel time series (30 seconds @ 1 Hz = 30 timesteps):
        - PM2.5 (µg/m³)
        - VOC index (0–500)
        - Gas resistance (kΩ)
        - CO₂eq (ppm)
        - Smoke opacity (derived from PM1.0/PM2.5 ratio)
        → Conv1D(16, k=5) → MaxPool(2) → Conv1D(32, k=3) → MaxPool(2)
        → Flatten → Dense(32) → Dense(5)
Output: 5-class (clean blue / thin blue / dirty white / creosote / no smoke)
Size:   ~90 KB (int8 quantized)
Inference: <50 ms on ESP32-S3 @ 240 MHz
```

### 5.7 Gas Leak Detection Algorithm

The Grill Sentinel monitors the MQ-2 sensor for propane/natural gas:

1. **Baseline calibration** — 30-second warmup, establish baseline at power-on
2. **Continuous monitoring** — 10 Hz ADC sampling, 1-second moving average
3. **LEL calculation** — Propane LEL = 21000 ppm; alarm at 10% LEL = 2100 ppm
4. **Pattern detection** — GasLeakNet (cloud) classifies leak vs. normal cooking gas
5. **Response** — At 10% LEL: immediate ALERT to Hub → Hub triggers GAS_SHUTOFF relay + buzzer + push notification
6. **Response** — At 25% LEL: EMERGENCY_MODE, all alerts fire, automatic gas shutoff + app notification

### 5.8 Child Safety Zone Detection

The Grill Sentinel uses the MLX90640 thermal array to detect humans entering the grill danger zone:

1. **Thermal human detection** — Pixels in 30–37°C range (human body temp) near grill
2. **Zone definition** — Configurable 3-foot (1 m) radius around grill sentinel
3. **Detection** — Any human-temperature blob > 4 pixels entering zone
4. **Alert** — CHILD_IN_ZONE alert sent to Hub → buzzer + LED ring flash + app notification
5. **Specificity** — Excludes grill surface (>100°C) and ambient (<20°C) via temperature range filtering

---

## 6. Cloud Backend (FastAPI + MQTT)

### 6.1 Architecture

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Meat Probe  │     │ Grill Hub   │     │ Smoke Node  │
│  (BLE→Hub)  │     │ (Wi-Fi/MQTT)│     │ (Sub-GHz→Hub)│
└──────┬──────┘     └──────┬──────┘     └─────────────┘
       │                    │
       └──────────────────►│
                           │ MQTT / HTTPS
                    ┌──────▼──────┐
                    │   Cloud     │
                    │ FastAPI     │
                    │ MQTT Broker │
                    │ InfluxDB    │
                    │ PostgreSQL  │
                    │ ML Pipeline │
                    └──────┬──────┘
                           │ HTTPS / WebSocket
                    ┌──────▼──────┐
                    │ Mobile App  │
                    └─────────────┘
```

### 6.2 API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/cook-sessions` | POST | Start a new cook session |
| `/api/v1/cook-sessions/{id}` | GET | Get cook session details |
| `/api/v1/cook-sessions/{id}/end` | POST | End cook session |
| `/api/v1/nodes` | GET | List all registered nodes |
| `/api/v1/nodes/{id}/telemetry` | GET | Get node telemetry history |
| `/api/v1/alerts` | GET | List alerts (filterable by severity, node, session) |
| `/api/v1/alerts/{id}/ack` | POST | Acknowledge alert |
| `/api/v1/meat-profiles` | GET | List meat type profiles |
| `/api/v1/meat-profiles/{id}` | GET | Get specific meat profile |
| `/api/v1/meat-profiles` | POST | Create custom meat profile |
| `/api/v1/thermal-frames/{session_id}` | GET | Get thermal frame history |
| `/api/v1/ml/doneness-predict` | POST | Request cloud doneness prediction |
| `/api/v1/ml/flareup-predict` | POST | Request cloud flare-up prediction |
| `/api/v1/reports/cook/{session_id}` | GET | Generate cook report PDF |
| `/api/v1/recipes` | GET | List recipes |
| `/api/v1/recipes/import` | POST | Import recipe (XML/JSON) |
| `/api/v1/firmware/latest` | GET | Get latest firmware version |
| `/api/v1/firmware/ota` | POST | Trigger OTA update |
| `/api/v1/safety-events` | GET | Get safety event log |
| `/ws/realtime` | WS | WebSocket for real-time updates |

### 6.3 Data Model

- **CookSession** — id, start_time, end_time, meat_type, target_temp, doneness_target, probes_used, grill_config
- **Telemetry** — timestamp, node_id, session_id, sensor_type, values (JSONB)
- **Alert** — id, timestamp, node_id, session_id, alert_type, severity, data, acknowledged
- **ThermalFrame** — timestamp, session_id, frame_data (compressed), max_temp, avg_temp, hot_zones
- **MeatProfile** — id, name, meat_type, usda_min_temp, doneness_levels (JSONB), rest_time_minutes
- **SafetyEvent** — id, timestamp, session_id, event_type, severity, description, action_taken
- **MLPrediction** — id, timestamp, session_id, model_name, input_hash, prediction, confidence

---

## 7. ML Pipeline

### 7.1 Model Overview

| # | Model | Type | Purpose | Framework | Target |
|---|-------|------|---------|-----------|--------|
| 1 | DonenessNet | 1D-CNN | Meat doneness prediction from thermal gradient | PyTorch → TFLite int8 | ESP32-S3 (Hub) |
| 2 | FlareUpNet | LSTM | Flare-up prediction 8–15s ahead | PyTorch → TFLite int8 | ESP32-S3 (Sentinel) |
| 3 | GasLeakNet | XGBoost | Gas leak pattern classification (leak vs normal) | XGBoost | Cloud |
| 4 | SmokeNet | 1D-CNN | Smoke quality classification (5-class) | PyTorch → TFLite int8 | ESP32-S3 (Smoke Node) |
| 5 | GrillAnomaly | Isolation Forest | Grill behavior anomaly detection | scikit-learn | Cloud |
| 6 | SafetyForecast | LSTM | Cook session safety risk forecast | PyTorch | Cloud |

### 7.2 Training Data

- **DonenessNet:** 50,000+ cook sessions with 4-probe temperature curves + ground-truth doneness labels (from thermal camera + chef verification). 8 meat types × 6 doneness levels.
- **FlareUpNet:** 10,000+ grilling sessions with 500+ labeled flare-up events (thermal array + acoustic + flame data). Synthetic augmentation for rare event oversampling.
- **GasLeakNet:** 5,000 labeled gas leak events (controlled propane release tests) + 100,000 normal cooking sessions.
- **SmokeNet:** 3,000 smoking sessions with 5-class smoke quality labels (expert BBQ pitmaster annotations).
- **GrillAnomaly:** Unsupervised, trained on 20,000 normal sessions.
- **SafetyForecast:** 10,000 cook sessions with safety event labels (fire, flare-up, gas leak, burn).

### 7.3 Edge vs Cloud Split

- **Edge (ESP32-S3):** DonenessNet (Hub), FlareUpNet (Sentinel), SmokeNet (Smoke Node) — real-time, low-latency (<200ms)
- **Cloud:** GasLeakNet, GrillAnomaly, SafetyForecast — batch analysis, retraining, historical risk forecasting
- **OTA model updates:** TFLite models pushed to devices via Hub → node mesh distribution

---

## 8. Mobile App (React Native)

### 8.1 Screens

| Screen | Description |
|--------|-------------|
| **Cook Dashboard** | Real-time multi-probe temps, doneness countdown, thermal heat map, grill surface temp |
| **Alert Center** | Active alerts (gas leak, flare-up, food done, child in zone) with acknowledge |
| **Cook Setup** | Select meat type, doneness target, probes assignment, recipe import |
| **Thermal View** | Live 32×24 thermal heat map of grill surface with hot zones |
| **Smoke Monitor** | PM2.5, VOC, smoke quality class, smoke history chart |
| **Cook History** | Past cook sessions with temps, doneness results, safety events |
| **Recipes** | Recipe library with grill-synced cooking profiles |
| **Safety Log** | All safety events with timestamps, descriptions, actions taken |
| **Settings** | Node management, thresholds, alert preferences, gas auto-shutoff toggle |
| **Reports** | Per-session cook report (PDF) with temperature curves + safety validation |

### 8.2 Push Notifications

| Notification | Trigger | Priority |
|-------------|---------|----------|
| ⚠️ GAS LEAK DETECTED | MQ-2 > 10% LEL | Critical (auto shutoff) |
| 🔥 FLARE-UP WARNING | FlareUpNet risk > 70% | High |
| 🔥 FLARE-UP ACTIVE | Flame detector + thermal spike | Critical |
| 🚨 GRILL FIRE | Thermal > 400°C + flame | Critical (auto shutoff) |
| 👶 CHILD NEAR GRILL | Thermal human detection in zone | High |
| 🥩 STEAK DONE IN 2 MIN | DonenessNet ETA < 120s | Medium |
| 🥩 STEAK IS DONE | Target temp reached | Medium |
| ⚠️ FOOD OVERCOOKED | Temp exceeds target by >5°C | Medium |
| ⚠️ FOOD UNDERCOOKED | Below USDA temp after cook | High |
| 💨 SMOKE QUALITY POOR | SmokeNet: creosote detected | Low |
| 🔋 PROBE LOW BATTERY | Battery < 3.3V | Low |

---

## 9. Power Architecture

| Node | Power Source | Battery | Autonomy | Notes |
|------|-------------|---------|----------|-------|
| Grill Hub | USB-C 5V or 12V barrel | — | Continuous | Wall-powered |
| Grill Sentinel | USB-C 5V | — | Continuous | Wall-powered |
| Smoke Node | USB-C 5V | — | Continuous | Wall-powered |
| Meat Probe | USB-C charge | 500 mAh LiPo | 8 hours (active cook) | 90-min charge |

### Power Budget (Meat Probe, active cook)

| Component | Current | Duty | Avg Current |
|-----------|---------|------|-------------|
| nRF52840 (BLE active) | 5 mA | 10% | 0.5 mA |
| nRF52840 (sleep) | 1 µA | 90% | 0.9 µA |
| 4× MAX31855K | 0.9 mA each | 1% (2 Hz reads) | 0.036 mA |
| SK6812 LED (1 LED, dim) | 5 mA | 1% | 0.05 mA |
| **Total** | | | **~0.6 mA avg** |

500 mAh / 0.6 mA = **833 hours theoretical**; 8 hours active cook life accounts for BLE advertising, thermal variation, and safety margin.

---

## 10. Safety Architecture

### 10.1 Safety Interlocks

| Hazard | Detection | Response | Latency |
|--------|-----------|----------|---------|
| Propane leak (10% LEL) | MQ-2 > 2100 ppm | Gas shutoff + buzzer + push notification | <500 ms |
| Propane leak (25% LEL) | MQ-2 > 5250 ppm | EMERGENCY_MODE + shutoff + all alerts | <500 ms |
| Grill fire (thermal >400°C) | MLX90640 max + flame IR | Gas shutoff + buzzer + push notification | <1 s |
| Flare-up (predicted) | FlareUpNet risk > 70% | Warning buzzer + LED ring flash + push | <200 ms inference |
| Flare-up (active) | Flame detector + acoustic | Gas shutoff + push notification | <500 ms |
| Probe cable overtemp | Probe body > 300°C | Push notification + probe power-off | <1 s |
| Child in zone | Thermal human detection | Buzzer + LED ring + push notification | <2 s |
| Food undercooked | Meat temp < USDA after cook | Push notification + safety flag | Post-cook |
| Node offline | Heartbeat missed > 60s | Push notification + safety flag | 60–120 s |

### 10.2 Redundancy

- **Gas shutoff:** Hardware relay + motorized ball valve (fail-closed)
- **Flame detection:** Two independent sensors (IR photodiode + thermal array max temp)
- **Gas leak:** MQ-2 + BME680 VOC (cross-validation)
- **Temperature:** 4 thermocouples per probe (redundancy + gradient analysis)
- **Alert delivery:** Buzzer + LED ring + OLED display + push notification (4 channels)
- **Watchdog:** TPL5010 on battery-powered nodes for automatic recovery
- **OTA with rollback:** Firmware updates with automatic rollback on failure

### 10.3 Fail-Safe Design

- **Gas valve:** Fail-closed (spring-return motorized ball valve — power loss = valve closes)
- **Alerts:** If Wi-Fi down, Hub still triggers local buzzer + LED + gas shutoff
- **If Hub offline:** Grill Sentinel can directly trigger gas shutoff (local GPIO to relay)
- **If all nodes offline:** Hardware float switch on gas line as ultimate backup

---

## 11. Bill of Materials

### 11.1 Grill Hub BOM

| # | Component | Part | Qty | Unit Price | Total |
|---|-----------|------|-----|-----------|-------|
| 1 | SoC Module | ESP32-S3-WROOM-1-N16R8 | 1 | $4.20 | $4.20 |
| 2 | Sub-GHz Radio | SX1262IMLTRT | 1 | $5.80 | $5.80 |
| 3 | Temp/Hum/Press | BME280 Breakout | 1 | $2.50 | $2.50 |
| 4 | RTC | DS3231SN | 1 | $2.00 | $2.00 |
| 5 | microSD Socket | Molex 503393-1892 | 1 | $0.80 | $0.80 |
| 6 | TFT Display | 2.4" ILI9341 320×240 | 1 | $4.50 | $4.50 |
| 7 | LED Ring | WS2812B 24-LED ring | 1 | $3.50 | $3.50 |
| 8 | Gas Shutoff Relay | SRD-12VDC-SL-C | 1 | $1.20 | $1.20 |
| 9 | Motorized Ball Valve | 12V DC 1/2" NPT | 1 | $15.00 | $15.00 |
| 10 | Buzzer | CMT-8543S-SMT | 1 | $0.60 | $0.60 |
| 11 | Status LEDs | SK6812 RGB | 3 | $0.15 | $0.45 |
| 12 | eFuse | TPS25940 | 1 | $1.80 | $1.80 |
| 13 | Buck Converter | MP1584EN | 1 | $1.00 | $1.00 |
| 14 | LDO | AMS1117-3.3 | 1 | $0.20 | $0.20 |
| 15 | SMA Antenna | 868 MHz whip | 1 | $2.50 | $2.50 |
| 16 | USB-C Connector | 16-pin SMT | 1 | $0.50 | $0.50 |
| 17 | 12V Barrel Jack | 5.5×2.1mm | 1 | $0.60 | $0.60 |
| 18 | PCB | 4-layer FR4 80×60mm | 1 | $3.00 | $3.00 |
| 19 | Capacitors/Resistors | Various SMD | 1 set | $1.50 | $1.50 |
| 20 | Enclosure | ASA 3D-printed | 1 | $4.00 | $4.00 |
| | | | **Total** | | **$52.45** |

### 11.2 Grill Sentinel BOM

| # | Component | Part | Qty | Unit Price | Total |
|---|-----------|------|-----|-----------|-------|
| 1 | SoC Module | ESP32-S3-WROOM-1-N8R2 | 1 | $3.50 | $3.50 |
| 2 | Sub-GHz Radio | SX1262IMLTRT | 1 | $5.80 | $5.80 |
| 3 | Thermal Array | MLX90640 32×24 | 1 | $32.00 | $32.00 |
| 4 | Gas Sensor | MQ-2 | 1 | $1.80 | $1.80 |
| 5 | Flame Detector | IR flame sensor (GY-302) | 1 | $2.00 | $2.00 |
| 6 | Ambient Sensor | BME280 Breakout | 1 | $2.50 | $2.50 |
| 7 | Piezo Disc | 35mm piezo element | 1 | $0.50 | $0.50 |
| 8 | Status LED | SK6812 RGB | 1 | $0.15 | $0.15 |
| 9 | SMA Antenna | 868 MHz whip | 1 | $2.50 | $2.50 |
| 10 | USB-C Connector | 16-pin SMT | 1 | $0.50 | $0.50 |
| 11 | LDO | AMS1117-3.3 | 1 | $0.20 | $0.20 |
| 12 | PCB | 4-layer FR4 60×40mm | 1 | $2.50 | $2.50 |
| 13 | Capacitors/Resistors | Various SMD | 1 set | $1.20 | $1.20 |
| 14 | Enclosure | Glass-filled ASA 3D-printed | 1 | $5.00 | $5.00 |
| 15 | Thermal Lens | Fresnel AR-coated | 1 | $2.00 | $2.00 |
| | | | **Total** | | **$62.15** |

### 11.3 Meat Probe BOM

| # | Component | Part | Qty | Unit Price | Total |
|---|-----------|------|-----|-----------|-------|
| 1 | MCU | nRF52840 QFAA | 1 | $4.50 | $4.50 |
| 2 | TC Interface | MAX31855KASA+ | 4 | $4.20 | $16.80 |
| 3 | Thermocouples | Type-K 0.5mm SS sheath | 4 | $2.00 | $8.00 |
| 4 | Battery Charger | MCP73831T-2ACI/OT | 1 | $0.80 | $0.80 |
| 5 | Battery | LiPo 3.7V 500 mAh | 1 | $3.50 | $3.50 |
| 6 | Status LED | SK6812 RGB | 1 | $0.15 | $0.15 |
| 7 | USB-C Connector | 16-pin SMT | 1 | $0.50 | $0.50 |
| 8 | Buttons | Tactile SMD ×2 | 2 | $0.10 | $0.20 |
| 9 | PCB | 4-layer FR4 40×25mm | 1 | $2.00 | $2.00 |
| 10 | Capacitors/Resistors | Various SMD | 1 set | $1.00 | $1.00 |
| 11 | Enclosure | IP67 3D-printed PETG | 1 | $3.00 | $3.00 |
| 12 | Probe Cable | PTFE jacketed, 300°C rated | 1 | $2.50 | $2.50 |
| 13 | Antenna | PCB trace BLE | 1 | $0.00 | $0.00 |
| | | | **Total** | | **$42.95** |

### 11.4 Smoke Node BOM

| # | Component | Part | Qty | Unit Price | Total |
|---|-----------|------|-----|-----------|-------|
| 1 | SoC Module | ESP32-S3-WROOM-1-N8R2 | 1 | $3.50 | $3.50 |
| 2 | Sub-GHz Radio | SX1262IMLTRT | 1 | $5.80 | $5.80 |
| 3 | PM Sensor | PMS5003 (Plantower) | 1 | $8.00 | $8.00 |
| 4 | VOC Sensor | BME680 Breakout | 1 | $5.50 | $5.50 |
| 5 | Gas Sensor | MQ-135 | 1 | $1.50 | $1.50 |
| 6 | Flame Detector | UV-TRON flame sensor | 1 | $8.00 | $8.00 |
| 7 | Ambient Sensor | BME280 Breakout | 1 | $2.50 | $2.50 |
| 8 | Status LED | SK6812 RGB | 1 | $0.15 | $0.15 |
| 9 | SMA Antenna | 868 MHz whip | 1 | $2.50 | $2.50 |
| 10 | USB-C Connector | 16-pin SMT | 1 | $0.50 | $0.50 |
| 11 | LDO | AMS1117-3.3 | 1 | $0.20 | $0.20 |
| 12 | PCB | 4-layer FR4 50×40mm | 1 | $2.50 | $2.50 |
| 13 | Capacitors/Resistors | Various SMD | 1 set | $1.20 | $1.20 |
| 14 | Enclosure | Aluminum + PTFE filter | 1 | $5.00 | $5.00 |
| | | | **Total** | | **$45.35** |

**Complete 4-node system total:** ~$203 (without meat probe extras)

---

## 12. Schematics

See `schematic/` folder for KiCad projects per node:
- `schematic/hub/` — Grill Hub schematic (ESP32-S3 + SX1262 + display + relay)
- `schematic/grill-sentinel/` — Grill Sentinel schematic (ESP32-S3 + MLX90640 + MQ-2 + flame)
- `schematic/meat-probe/` — Meat Probe schematic (nRF52840 + 4× MAX31855)
- `schematic/smoke-node/` — Smoke Node schematic (ESP32-S3 + PMS5003 + BME680)

Each schematic folder contains:
- `README.md` — Detailed schematic description with pin assignments and bus topology
- Block diagrams, power architecture, and signal flow

---

## 13. Directory Structure

```
GrillSync/
├── README.md              # This file — comprehensive system overview
├── schematic/              # KiCad projects (one per hardware node)
│   ├── README.md
│   ├── hub/
│   ├── grill-sentinel/
│   ├── meat-probe/
│   └── smoke-node/
├── firmware/               # C source per node + shared common/
│   ├── common/             # Shared protocol, mesh, config, radio driver
│   │   ├── config.h
│   │   ├── protocol.h
│   │   ├── protocol.c
│   │   ├── sx1262.h
│   │   ├── sx1262.c
│   │   ├── mesh.h
│   │   └── mesh.c
│   ├── hub/                # Grill Hub firmware (ESP32-S3, ESP-IDF)
│   │   └── main.c
│   ├── grill-sentinel/     # Grill Sentinel firmware (ESP32-S3, ESP-IDF)
│   │   └── main.c
│   ├── meat-probe/         # Meat Probe firmware (nRF52840, nRF Connect SDK)
│   │   └── main.c
│   └── smoke-node/         # Smoke Node firmware (ESP32-S3, ESP-IDF)
│       └── main.c
├── hardware/
│   └── bom/                # BOM.csv per node
│       ├── hub_bom.csv
│       ├── grill_sentinel_bom.csv
│       ├── meat_probe_bom.csv
│       └── smoke_node_bom.csv
├── software/
│   ├── dashboard/          # FastAPI backend
│   │   ├── main.py
│   │   └── pyproject.toml
│   ├── ml-pipeline/        # ML training scripts
│   │   ├── train_doneness.py
│   │   ├── train_flareup.py
│   │   ├── train_gasleak.py
│   │   ├── train_smoke.py
│   │   ├── train_anomaly.py
│   │   ├── train_safety.py
│   │   ├── pyproject.toml
│   │   └── README.md
│   └── mobile-app/         # React Native app
│       ├── App.tsx
│       └── package.json
├── docs/                   # Architecture, API, protocol specs
│   ├── architecture.md
│   ├── api-spec.md
│   └── protocol-spec.md
└── scripts/                # Deployment, calibration, training
    ├── calibrate_sensors.py
    ├── train_models.py
    └── deploy.sh
```

---

## 14. Build & Deploy

### 14.1 Firmware Build

```bash
# Grill Hub (ESP32-S3, ESP-IDF)
cd firmware/hub
idf.py set-target esp32s3
idf.py build flash monitor

# Grill Sentinel (ESP32-S3, ESP-IDF)
cd firmware/grill-sentinel
idf.py set-target esp32s3
idf.py build flash monitor

# Smoke Node (ESP32-S3, ESP-IDF)
cd firmware/smoke-node
idf.py set-target esp32s3
idf.py build flash monitor

# Meat Probe (nRF52840, nRF Connect SDK)
cd firmware/meat-probe
west build -b nrf52840dk_nrf52840
west flash
```

### 14.2 Cloud Backend

```bash
cd software/dashboard
pip install -e .
uvicorn main:app --host 0.0.0.0 --port 8000
```

### 14.3 ML Pipeline

```bash
cd software/ml-pipeline
pip install -e .
python train_doneness.py --data /data/cook-sessions --output models/
python train_flareup.py --data /data/flare-events --output models/
python train_gasleak.py --data /data/gas-events --output models/
python train_smoke.py --data /data/smoke-sessions --output models/
python train_anomaly.py --data /data/grill-sessions --output models/
python train_safety.py --data /data/safety-events --output models/
```

### 14.4 Mobile App

```bash
cd software/mobile-app
npm install
npx react-native run-android  # or run-ios
```

### 14.5 Deployment Script

```bash
./scripts/deploy.sh  # Deploys cloud backend, configures MQTT, pushes OTA firmware
```

---

## 15. Target Metrics

| Metric | Target | How Measured |
|--------|--------|-------------|
| Doneness prediction accuracy | >92% | 5-fold cross-validation on held-out cook sessions |
| Flare-up prediction lead time | 8–15 seconds | Ground-truth thermal spike + flame detection |
| Flare-up false positive rate | <5% | Normal cooking without flare-ups |
| Gas leak detection latency | <500 ms | MQ-2 + LEL calculation at 10 Hz |
| Gas leak false positive rate | <2% | Normal cooking (gas burner on, no leak) |
| Smoke quality accuracy | >88% | Expert pitmaster annotations (5-class) |
| Food safety compliance | 100% | USDA temp validation per cook session |
| Probe temperature accuracy | ±2°C | MAX31855K spec, ice-bath + boiling validation |
| Thermal array accuracy | ±1°C | MLX90640 spec, blackbody calibration |
| Battery life (Meat Probe) | 8 hours | Active cook, 2 Hz sampling, BLE |
| Node join time | <5 seconds | Power-on to JOIN_ACK |
| OTA update time | <3 minutes | Full firmware, all nodes |
| Mesh range (Sub-GHz) | 300 m LOS (SF7) | Open field, RSSI > -100 dBm |

---

## License

MIT — build it, sell it, improve it.

---

*Invented as part of the [Devices](https://github.com/jayis1/Devices) collection — a new complex device system every 24 hours.*
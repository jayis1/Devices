# AllergySync — AI-Powered Seasonal Allergy & Pollen Management System

> **Tagline:** Know your pollen before it knows you. The world's first end-to-end allergy avoidance system — ambient pollen sensing, auto-ventilation control, wearable exposure tracking, and personalized allergen forecasting.

Allergies affect over 400 million people worldwide. Seasonal allergic rhinitis alone impacts 10–30% of adults and 40% of children, causing diminished productivity, poor sleep, and exacerbation of asthma. Yet management today is reactive — sufferers dose antihistamines only after symptoms appear. AllergySync flips this paradigm: it **detects pollen in real time**, **predicts exposure windows**, **automatically closes windows and triggers air purifiers** when allergen levels rise, and **learns each person's unique sensitivities** through a structured allergy profile and exposure-symptom correlation.

## System Overview

AllergySync is a four-node mesh system:

```
                          ┌──────────────┐
                          │   Cloud /     │
                          │  FastAPI +    │
                          │   MQTT        │
                          └──────┬───────┘
                                 │ MQTT over Wi-Fi
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

| Node | SoC | Role | Power |
|------|-----|------|-------|
| **Hub** | ESP32-S3 | Wi-Fi/MQTT gateway, TDMA coordinator, local inference | USB or PoE |
| **Room Sentinel** | ESP32-S3 | Laser PM + pollen classifier, VOC/CO₂/temp/humidity | USB or PoE |
| **Window Node** | nRF52840 | Motorized window actuator, air purifier relay, reed switch | 4× AA or USB |
| **Wearable Tag** | nRF52840 | Wearable optical particle counter, IMU activity, BLE to phone | CR2032 (9 mo) |

## Nodes in Detail

### 1. AllergySync Hub (ESP32-S3)

The central coordinator. Connects to home Wi-Fi and the cloud via MQTT. Manages a TDMA mesh on Sub-GHz 868 MHz (LR1121 transceiver) to communicate with battery-powered nodes. Runs local inference for immediate allergen-response decisions (no cloud round-trip for window closing). Maintains a rolling 30-day exposure database.

**Key components:**
- ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB PSRAM)
- LR1121 Sub-GHz + 2.4 GHz transceiver (868 MHz TDMA mesh)
- BMI270 IMU (tamper detection)
- RGB status LED (SK6812)
- USB-C power + CP2102N debug UART
- Optional PoE (TPS2378)

### 2. Room Sentinel (ESP32-S3)

The sensing powerhouse. Uses a laser-based optical particle counter to measure PM1.0/PM2.5/PM10 with size bins from 0.3–10 µm. Runs an on-device CNN to classify pollen grains by size distribution signature — birch (~22 µm), grass (~25–30 µm), ragweed (~15–20 µm), oak (~28 µm), pine (~50 µm), mold spores (~3–10 µm). Also measures VOCs (BME688), CO₂ (SCD41), temperature, and humidity.

**Key components:**
- ESP32-S3-WROOM-1-N16R8
- Sensirion SPS30 laser PM sensor (I²C, 0.3–10 µm size bins)
- Sensirion SCD41 photoacoustic CO₂ sensor (I²C)
- Bosch BME688 (VOC + temp + humidity + pressure)
- TMP117 (high-accuracy ambient temp, ±0.1°C)
- 6-pin JST for fan-assisted sampling (50 mm fan, 5 V)
- USB-C power

### 3. Window Node (nRF52840)

An actuator node installed at each operable window. Uses a stepper motor to drive a window actuator (chain or rack-and-pinion). Includes a reed switch for open/closed state feedback, a relay output for triggering an air purifier, and an ambient light sensor to respect natural-light preferences.

**Key components:**
- nRF52840 dongle module (Raytac MDBT50Q-1M)
- LR1121 Sub-GHz transceiver (868 MHz)
- TMC2209 stepper driver (window actuator)
- Latching reed switch (window state)
- 5 V relay (air purifier trigger)
- VEML7700 ambient light sensor
- 4× AA NiMH batteries (with TP4056 USB charging) or USB
- INA260 current monitor

### 4. Wearable Tag (nRF52840)

A coin-cell-powered wearable that clips to a lapel, backpack, or lanyard. Contains a miniaturized optical particle counter (PMSA003I) for personal pollen exposure tracking, an IMU for activity classification (indoor/outdoor/running), and a CR2032 battery. Communicates via Sub-GHz mesh (to the hub) when in range, or via BLE to the phone app when outside.

**Key components:**
- nRF52840 module (Raytac MDBT50Q-1M)
- Plantower PMSA003I (mini PM sensor, I²C)
- BMI270 IMU
- LR1121 Sub-GHz transceiver
- CR2032 coin cell (9-month battery life)
- SK6812 mini LED (status)

## Communication Architecture

| Link | Protocol | Band | Range | Notes |
|------|----------|------|-------|-------|
| Hub ↔ Cloud | Wi-Fi → MQTT/TLS | 2.4 GHz | Home Wi-Fi | QoS 1, auto-reconnect |
| Hub ↔ Nodes | Sub-GHz TDMA mesh | 868 MHz | 200 m LoS | 12-slot TDMA, 4-hop mesh, AES-128 |
| Wearable ↔ Phone | BLE 5.0 | 2.4 GHz | 10 m | GATT notifications when away from hub |
| Wearable ↔ Hub | Sub-GHz | 868 MHz | 100 m | When in range of home mesh |

**TDMA frame:** 12 time slots × 500 ms = 6 s cycle. Each node gets 1 guaranteed slot + contention slots for burst data. Hub broadcasts beacon at slot 0. AES-128-CCM encryption with per-node session keys derived via ECDH (P-256) on provisioning.

## ML Pipeline

AllergySync uses a 6-model ML pipeline:

| # | Model | Purpose | Edge/Cloud | Architecture |
|---|-------|---------|------------|--------------|
| 1 | PollenNet | 6-class pollen type classification from PM size distribution | Edge (Room Sentinel) | 1D-CNN, 12→32→64→6, tflite-micro |
| 2 | PollenForecast | 24-hour pollen concentration forecast | Cloud | LSTM, 72 timesteps, weather + season + PM history |
| 3 | SymptomPredict | 12-hour symptom severity forecast per user | Cloud | XGBoost, features: exposure, weather, medication, day-of-week |
| 4 | AllergenSensitivity | Personal allergen profile learning | Cloud | Bayesian logistic regression, online update |
| 5 | ActivityClassifier | 6-class activity (indoor/outdoor/running/walking/static/commuting) | Edge (Wearable) | TinyCNN, 16→32→6, IMU 6-axis |
| 6 | AnomalyDetector | Pollen anomaly detection (unexpected spikes) | Cloud | Isolation Forest, multivariate (PM + CO₂ + VOC) |

## Mobile App

React Native app with:
- **Exposure dashboard** — current pollen levels, personal allergen risk meter, 24-hour forecast
- **Symptom journal** — quick-tag symptoms (sneezing, itchy eyes, congestion), severity 0–5
- **Medication tracking** — antihistamine dose logging + reminder
- **Node management** — pair, configure, firmware update (OTA)
- **Allergy profile** — input skin-prick test results, known allergies, sensitivity sliders
- **Insights** — weekly/monthly exposure vs. symptom correlation, personalized tips
- **Notifications** — "Close windows: birch pollen rising in your area"
- **History** — 30-day exposure charts, symptom timeline

## Power Architecture

```
Hub:          USB-C 5 V / 1 A or PoE 802.3af (3.3 V LDO)
Room Sentinel: USB-C 5 V / 0.5 A (fan + sensors, ~350 mA active)
Window Node:  4× AA NiMH (2000 mAh) → 3.3 V LDO, 3-month battery
              or USB-C passthrough
Wearable Tag: CR2032 (220 mAh), duty-cycled PM sensor, 9-month battery
              PM sensor: 1 sample/5 min (8 s active, 292 s sleep)
              Sub-GHz TX: 1 packet/10 min
```

## Bill of Materials Summary

| Node | Est. BOM Cost (qty 1k) |
|------|----------------------|
| Hub | $18.50 |
| Room Sentinel | $42.00 |
| Window Node | $22.00 |
| Wearable Tag | $15.00 |
| **System (1 hub + 1 sentinel + 2 window + 1 tag)** | **$119.50** |

## Pin Assignments

### Hub (ESP32-S3)

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO4 | LR1121 SPI CS | SPI bus 2 |
| GPIO5 | LR1121 SPI CLK | |
| GPIO6 | LR1121 SPI MISO | |
| GPIO7 | LR1121 SPI MOSI | |
| GPIO8 | LR1121 DIO0 | IRQ |
| GPIO9 | LR1121 DIO1 | IRQ |
| GPIO10 | LR1121 RESET | |
| GPIO11 | LR1121 BUSY | |
| GPIO12 | SK6812 LED | RGB status |
| GPIO13 | BMI270 SDA | I²C bus 1 |
| GPIO14 | BMI270 SCL | |
| GPIO15 | BMI270 INT1 | Motion IRQ |
| GPIO43 | UART0 TX | Debug (CP2102N) |
| GPIO44 | UART0 RX | Debug (CP2102N) |

### Room Sentinel (ESP32-S3)

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO8 | SPS30 UART TX | PM sensor |
| GPIO9 | SPS30 UART RX | PM sensor |
| GPIO10 | SCD41 SDA | I²C bus 1 |
| GPIO11 | SCD41 SCL | |
| GPIO12 | BME688 SDA | I²C bus 2 (via mux) |
| GPIO13 | BME688 SCL | |
| GPIO14 | TMP117 SDA | I²C bus 1 (shared) |
| GPIO15 | TMP117 SCL | |
| GPIO4 | Fan PWM | 25 kHz PWM |
| GPIO5 | Fan enable | MOSFET gate |
| GPIO6 | SK6812 LED | |
| GPIO43 | UART0 TX | Debug |
| GPIO44 | UART0 RX | Debug |

### Window Node (nRF52840)

| Pin | Function | Notes |
|-----|----------|-------|
| P0.04 | LR1121 SPI CS | SPI master |
| P0.05 | LR1121 SPI CLK | |
| P0.06 | LR1121 SPI MISO | |
| P0.07 | LR1121 SPI MOSI | |
| P0.08 | LR1121 DIO0 | IRQ |
| P0.09 | LR1121 DIO1 | IRQ |
| P0.10 | LR1121 BUSY | |
| P0.11 | LR1121 RESET | |
| P0.13 | TMC2209 STEP | Step pulse |
| P0.14 | TMC2209 DIR | Direction |
| P0.15 | TMC2209 EN | Enable |
| P0.16 | TMC2209 UART TX | Config |
| P0.17 | TMC2209 UART RX | Config |
| P0.19 | Reed switch input | GPIO + pull-up |
| P0.20 | Relay control | Air purifier |
| P0.21 | VEML7700 SDA | I²C |
| P0.22 | VEML7700 SCL | |
| P0.24 | INA260 SDA | I²C (current monitor) |
| P0.25 | INA260 SCL | |
| P0.26 | SK6812 LED | |
| P0.27 | Button (calibrate) | |

### Wearable Tag (nRF52840)

| Pin | Function | Notes |
|-----|----------|-------|
| P0.04 | LR1121 SPI CS | |
| P0.05 | LR1121 SPI CLK | |
| P0.06 | LR1121 SPI MOSI | |
| P0.07 | LR1121 SPI MISO | |
| P0.08 | LR1121 DIO0 | IRQ |
| P0.09 | LR1121 DIO1 | IRQ |
| P0.10 | LR1121 BUSY | |
| P0.11 | LR1121 RESET | |
| P0.15 | PMSA003I TX | UART (PM sensor) |
| P0.16 | PMSA003I RX | UART (PM sensor) |
| P0.17 | PMSA003I EN | Enable (power gate) |
| P0.19 | BMI270 SDA | I²C |
| P0.20 | BMI270 SCL | |
| P0.21 | BMI270 INT1 | |
| P0.23 | SK6812 LED | |
| P0.25 | Button | Status / pairing |

## Schematic Diagrams

Each node has a full KiCad schematic in `schematic/<node>/`. The schematics include:

- Power regulation (3.3 V LDO, battery management)
- SoC + support components (decoupling, antenna, crystal)
- Sensor connections (I²C/UART/SPI pin assignments)
- Actuator circuits (stepper driver, relay)
- Debug interfaces (UART, SWD)
- Mechanical interfaces (connectors, mounting)

## Firmware

Firmware is written in C and built with:

- **ESP32-S3 nodes** → ESP-IDF v5.2
- **nRF52840 nodes** → nRF Connect SDK v2.6 (Zephyr RTOS)

Shared protocol code lives in `firmware/common/` and is compiled into each node's build.

### Firmware Features

**Hub:**
- TDMA mesh coordinator (beacon, slot assignment, mesh forwarding)
- MQTT client (TLS, auto-reconnect, QoS 1)
- Local decision engine (pollen → close windows, trigger purifier)
- OTA coordinator (firmware update to all nodes)
- 30-day local exposure database (SQLite on flash)

**Room Sentinel:**
- SPS30 PM sensor driver (continuous sampling, 1 Hz)
- PollenNet tflite-micro inference (6-class pollen classification)
- BME688 gas scan + VOC index calculation
- SCD41 CO₂ single-shot mode (1 sample/2 min)
- Data aggregation: 1-min averages → transmit to hub every 5 min
- Fan control (auto on during sampling)

**Window Node:**
- Stepper motor control (TMC2209, microstepping)
- Calibration routine (manual open/close end-stops)
- Reed switch monitoring (window state)
- Light-aware scheduling (don't close if user prefers natural light)
- Relay control (air purifier)
- Low-power sleep between Sub-GHz wake events

**Wearable Tag:**
- Duty-cycled PM sampling (8 s active, 5 min interval)
- Activity classification (TinyCNN, 6-class)
- Sub-GHz TX when in mesh range, BLE GATT when away
- Battery monitoring (ADC on CR2032)
- Deep sleep (3 µA system current)

## Cloud Backend

FastAPI + MQTT backend (`software/dashboard/`):

- **MQTT broker** (Mosquitto) receives telemetry from hub
- **FastAPI** REST API for mobile app
- **PostgreSQL** stores exposure history, symptom journals, allergy profiles
- **Redis** for real-time state caching
- **ML inference service** (Python) runs PollenForecast, SymptomPredict, AllergenSensitivity, AnomalyDetector
- **Weather API integration** (Open-Meteo) for pollen forecast features
- **WebSocket** for real-time mobile app updates
- **OTA firmware hosting** for all node types

### API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | /api/v1/exposure/current | Current pollen + allergen risk |
| GET | /api/v1/exposure/forecast | 24-h pollen forecast |
| GET | /api/v1/exposure/history | Historical exposure data |
| POST | /api/v1/symptoms | Log symptom entry |
| GET | /api/v1/symptoms | Retrieve symptom journal |
| POST | /api/v1/medication | Log medication dose |
| GET | /api/v1/profile | Get allergy profile |
| PUT | /api/v1/profile | Update allergy profile |
| GET | /api/v1/nodes | List registered nodes |
| POST | /api/v1/nodes/pair | Pair a new node |
| POST | /api/v1/nodes/{id}/ota | Trigger OTA update |
| GET | /api/v1/insights | Weekly/monthly insights |
| WS | /api/v1/ws | Real-time updates |

## Use Cases

### Scenario 1: Morning Birch Spike
1. Room Sentinel detects rising PM10 with birch pollen signature at 6:30 AM
2. Hub decision engine triggers: close bedroom window, activate air purifier relay
3. Mobile app sends notification: "Birch pollen rising — windows closing, purifier on"
4. SymptomPredict forecasts moderate symptoms for user today
5. App suggests antihistamine dose before going outside

### Scenario 2: Outdoor Exposure Tracking
1. User wears Wearable Tag on a hike
2. Tag measures personal PM exposure (higher than indoor) + activity classification
3. Tag logs exposure via BLE to phone (out of Sub-GHz range)
4. User logs mild eye itching in app
5. System correlates birch exposure with symptoms → updates sensitivity profile
6. Next hike: app warns "Bring antihistamine — birch pollen high on trail today"

### Scenario 3: Ragweed Season Forecast
1. PollenForecast LSTM predicts high ragweed in 48 hours (weather + season + PM trend)
2. App sends 48-hour advance notice
3. AllergenSensitivity model computes personalized risk (high for this user)
4. System pre-loads medication reminder for antihistamine
5. Windows pre-emptively closed before pollen peaks

## Clinical Impact

- **400M+** allergic rhinitis sufferers globally
- **$18B** annual antihistamine market
- **40%** of children affected by allergies
- AllergySync enables **proactive avoidance** instead of reactive dosing
- Personalized allergen profiles replace one-size-fits-all pollen count apps
- Exposure-symptom correlation provides **evidence for immunotherapy decisions**

## Directory Structure

```
AllergySync/
├── README.md                          # This file
├── schematic/
│   ├── hub/                           # KiCad project — Hub
│   ├── room-sentinel/                 # KiCad project — Room Sentinel
│   ├── window-node/                   # KiCad project — Window Node
│   └── wearable-tag/                  # KiCad project — Wearable Tag
├── firmware/
│   ├── hub/                           # ESP-IDF — Hub firmware
│   ├── room-sentinel/                 # ESP-IDF — Room Sentinel firmware
│   ├── window-node/                   # Zephyr — Window Node firmware
│   ├── wearable-tag/                  # Zephyr — Wearable Tag firmware
│   └── common/                        # Shared protocol, crypto, packet format
├── hardware/
│   └── bom/                           # BOM.csv per node
├── software/
│   ├── dashboard/                     # FastAPI backend
│   ├── ml-pipeline/                   # ML training scripts
│   └── mobile-app/                    # React Native app
├── docs/
│   ├── architecture.md
│   ├── protocol-spec.md
│   ├── api-spec.md
│   └── assembly-guide.md
└── scripts/
    ├── deploy-backend.sh
    ├── calibrate-window.sh
    └── train-models.sh
```

## License

MIT — build it, sell it, improve it.
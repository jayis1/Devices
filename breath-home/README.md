# BreathHome

**Smart indoor air quality and respiratory health management system.** Sees what you can't — VOCs, particulates, CO2, radon, mold spores, allergens — and acts on it automatically to keep your lungs safe.

---

## What It Does

BreathHome is a 4-node system that makes the invisible visible and the dangerous manageable:

1. **Monitors** air quality in every room — PM2.5, PM10, CO2, VOCs, formaldehyde, radon, mold spore count, temperature, humidity, barometric pressure, and light
2. **Tracks** your personal exposure with a wearable breath tag — what you're actually breathing, minute by minute
3. **Acts** automatically — controls HVAC, runs air purifiers, opens smart vents, activates kitchen/range hood exhaust when cooking fumes are detected
4. **Predicts** asthma and COPD exacerbation risk 4-6 hours ahead using personalized ML models
5. **Alerts** before problems become crises — "Mold risk rising in bathroom", "Radon elevated in basement", "CO2 building up in bedroom — open a window"
6. **Learns** your home's ventilation patterns, seasonal trends, and your respiratory health profile over time
7. **Integrates** with existing HVAC, smart vents, and air purifiers via Zigbee and WiFi

All room sensors communicate over a dedicated Sub-GHz mesh network (reliable, no WiFi dependency for health-critical alerts). The hub bridges to WiFi for cloud analytics and the mobile app. The wearable tag uses BLE. The HVAC controller uses both Sub-GHz (to the hub) and Zigbee (to existing smart home devices).

### The Problem It Solves

- 3.8 million people die annually from indoor air pollution (WHO)
- 339 million people have asthma worldwide; 251 million have COPD
- Radon causes 21,000 lung cancer deaths per year in the US — it's the #2 cause after smoking
- Mold affects 45% of US homes and causes respiratory illness, allergies, and asthma attacks
- CO2 above 1000ppm reduces cognitive function by 23%; above 2500ppm causes headaches and drowsiness
- Formaldehyde off-gassing from furniture and building materials is a known carcinogen
- Most people only discover air quality problems after they're already sick
- HVAC filters need changing but nobody knows when — too early wastes money, too late hurts health
- Cooking with gas stoves produces NO2, CO, and PM2.5 spikes that linger for hours
- Seasonal pollen infiltration affects 25% of the population

BreathHome detects all of this in real-time, acts on it automatically, and gives you personalized health insights. It's the air quality equivalent of a home security system — but instead of protecting your stuff, it protects your lungs.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                        BREATHHOME SYSTEM                                     │
│                                                                              │
│  ┌──────────────────┐   Sub-GHz    ┌──────────────────┐                    │
│  │  ROOM SENSOR #1  │◄───────────►│                  │                    │
│  │  (Bedroom)       │   868MHz    │                  │                    │
│  │  PM2.5/PM10      │   mesh     │                  │                    │
│  │  CO2/VOCs        │            │                  │                    │
│  │  Radon            │            │                  │                    │
│  │  Temp/RH/Pressure│            │                  │                    │
│  └──────────────────┘            │                  │                    │
│                                  │   HUB NODE       │                    │
│  ┌──────────────────┐            │  (nRF5340 +      │──── WiFi6 ────► Cloud
│  │  ROOM SENSOR #2 │◄──────────►│   ESP32-C6)      │                  Dashboard
│  │  (Kitchen)       │   Sub-GHz  │                  │                  + ML Pipeline
│  │  PM2.5/PM10      │   mesh     │  3.2" TFT        │                  + Alerts
│  │  CO2/VOCs/NO2    │            │  Air quality map  │
│  │  Temp/RH         │            │  Voice alerts     │─── BLE ──────► Wearable
│  └──────────────────┘            │                  │                  Breath Tag
│                                  │                  │                  (Personal
│  ┌──────────────────┐            │                  │                   Exposure)
│  │  ROOM SENSOR #3 │◄──────────►│                  │
│  │  (Bathroom)     │   Sub-GHz  │                  │
│  │  PM2.5/PM10     │   mesh     └──────┬───────────┘
│  │  VOCs/Mold risk │                   │
│  │  Temp/RH/CO2    │                   │ Sub-GHz mesh
│  │  (IP65 rated)   │                   │ (up to 16 room sensors
│  └──────────────────┘                   │  per hub)
│                                        │
│  ┌──────────────────┐                  │
│  │  HVAC CONTROLLER │◄────────────────┘
│  │  (Near furnace/  │   Sub-GHz
│  │   air handler)   │   mesh
│  │  Zigbee gateway  │
│  │  Smart vent ctrl │──── Zigbee ────► Smart Vents
│  │  Filter monitor  │                  Air Purifiers
│  │  Relay outputs   │──── 433MHz ────► Range Hood
│  └──────────────────┘                  Exhaust Fans
│
│  ┌──────────────────────────────────────────────────────────────────────┐
│  │                    CLOUD / EDGE SOFTWARE                           │
│  │  ┌──────────┐  ┌──────────────┐  ┌───────────────────────┐        │
│  │  │Dashboard │  │ ML Pipeline  │  │ Mobile App            │        │
│  │  │ (React)  │  │ Asthma risk │  │ (React Native)        │        │
│  │  │ Realtime │  │ Mold growth │  │ Push alerts           │        │
│  │  │ History  │  │ Pollen fore │  │ Personal exposure     │        │
│  │  │ Config   │  │ Filter life │  │ Medication reminders  │        │
│  │  └──────────┘  └──────────────┘  └───────────────────────┘        │
│  └──────────────────────────────────────────────────────────────────────┘
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per home)

The brain. Bridges Sub-GHz mesh to WiFi/BLE/cloud. Runs local ML models for real-time air quality assessment and asthma risk prediction. Displays a live air quality map of the home.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF5340 (dual Cortex-M33) | Application core runs mesh + ML; network core runs BLE 5.0 |
| WiFi Bridge | ESP32-C6-MINI-1 | WiFi 6 + BLE bridge to cloud |
| Radio | SX1262 (868MHz) | Sub-GHz LoRa mesh coordinator to all nodes |
| Display | 3.2" IPS TFT (ILI9341) | Air quality map: room-by-room AQI, trends, alerts |
| Audio Out | MAX98357A (I2S amp) + 3W speaker | Voice alerts: "High CO2 in bedroom, open window" |
| Audio In | SPH0645LM4H (I2S MEMS mic) | Voice commands, ambient sound analysis |
| On-board Sensors | SCD41 (CO2), SPS30 (PM2.5), BME688 (env), SGP41 (VOC+NOx) | Hub's own air quality monitoring |
| Storage | 16MB W25Q128 Flash + SD card | Data logging, OTA updates, ML model storage |
| LEDs | RGB status LED + 4× room zone LEDs | Visual air quality indicators |
| Emergency | Buzzer (piezo 3V) | Loud alarm for dangerous gas levels |
| Power | 5V USB-C + Lipo 3000mAh | Stays running during power outage (8+ hours) |
| Connectors | 2× I2C, 1× UART, 1× SPI, 8× GPIO | Expansion |

**Hub firmware responsibilities:**
- Mesh network coordinator (TDMA scheduler, assigns slots to all nodes)
- Aggregates air quality data from all room sensors + HVAC controller
- Runs TFLite Micro air quality classifier (multi-gas → AQI score)
- Runs TFLite Micro asthma exacerbation risk predictor (personalized model)
- WiFi uplink to MQTT broker (QoS 1, TLS)
- BLE GATT server for wearable breath tags and mobile app
- TFT dashboard rendering (room-by-room AQI map, trend graphs, alerts)
- Voice alerts via speaker for critical events (CO2 > 2000ppm, radon > 4 pCi/L, PM2.5 > 150)
- Two-way voice for configuration and queries ("How's the air in the bedroom?")
- Emergency alarm for dangerous gas levels
- OTA update distribution to all nodes
- Local alerts work even when WiFi is down

### 2. Room Sensor Node (1-16 per home)

Wall-mounted multi-sensor air quality monitor. The core sensing node. Measures everything you can't see but can breathe.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | STM32WB55CG | Dual-core (Cortex-M4 + M0+) with Sub-GHz + BLE, 512KB Flash, 256KB RAM |
| Particulate | Sensirion SPS30 | PM1.0, PM2.5, PM4.0, PM10 — optical particle counter |
| CO2 | Sensirion SCD41 | Photoacoustic CO2 sensor, 400-5000ppm, ±40ppm |
| VOC/NOx | Sensirion SGP41 | MOX gas sensor — VOC index + NOx index |
| Formaldehyde | Sensirion SFA30 | Formaldehyde (HCHO) sensor, 0-1ppm |
| Radon | RadonEye RD200M module | Pulsed ion chamber radon detector, 0-500 pCi/L |
| Environment | Bosch BME688 | Temperature, humidity, pressure, IAQ |
| Light | TSL25911 | Ambient light (lux) — day/night, UV estimation |
| Mold Indicator | Custom: SGP41 VOC trend + BME688 humidity | Mold growth risk index (computed, not a separate sensor) |
| Radio | SX1261 (868MHz) | Sub-GHz mesh client |
| Antenna | 868MHz chip antenna | Integrated |
| Power | 5V USB-C or 3× AA (4.5V) backup | Mains powered with battery backup |
| Enclosure | IP20 (indoor) or IP65 (bathroom) | 80×80×22mm wall-mount |

**Note on radon sensor:** The RD200M radon module is only installed in basement/ground-floor sensor nodes where radon risk is highest. Upper-floor nodes omit this component to save cost. The PCB has a footprint for it, populated as needed.

**Room sensor firmware:**
- SPS30 particulate sampling every 30 seconds (fan runs at low power between samples)
- SCD41 CO2 measurement every 60 seconds (photoacoustic, no warm-up)
- SGP41 VOC/NOx raw measurement every 10 seconds, processed to VOC index
- SFA30 formaldehyde sampling every 60 seconds
- BME688 environmental readings every 30 seconds
- TSL25911 light level every 60 seconds
- RD200M radon: 1-hour rolling average (radon requires long integration)
- All readings packetized and sent to hub via mesh TDMA slot
- On-board TFLite Micro mold risk classifier (BME688 IAQ + SGP41 VOC trend → mold risk 0-100%)
- Alert threshold checks locally: if PM2.5 > 150 or CO2 > 2000, flag packet as URGENT
- Self-calibration: SCD41 has automatic baseline correction; SGP41 auto-compensates humidity
- Ultra-low-power idle between readings (~300µA)

### 3. HVAC Controller Node (1 per home)

Installed near the furnace/air handler. Controls ventilation, air purifiers, and smart vents based on sensor data. This is the "actuator" node that makes the system close the loop — not just sensing, but acting.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 | Dual-core Xtensa, WiFi, 8MB PSRAM for Zigbee stack |
| Zigbee | CC2652R7 | Zigbee 3.0 coordinator — controls smart vents, air purifiers, HVAC |
| Radio | SX1261 (868MHz) | Sub-GHz mesh client to hub |
| Relays | 4× OMRON G5LE-14 5V | Dry contact: fan control, range hood, exhaust fan, furnace enable |
| 433MHz TX | FS1000A (433MHz) | Control dumb range hoods, exhaust fans via RF codes |
| Current Sensor | SCT013-030 (30A) | Monitor HVAC blower current — detect when filter is clogged |
| Pressure | BMP390 | Duct static pressure — detect filter clogging and airflow issues |
| Temperature | DS18B20 (waterproof) | Supply air temperature — verify HVAC is working |
| Environment | BME688 | Ambient temperature/humidity near air handler |
| Power | 24VAC from furnace transformer + 5V USB backup | HVAC-powered, auto-switches to USB on power loss |
| Enclosure | DIN rail mount, 100×70×40mm | Installs in furnace closet or air handler room |

**HVAC controller firmware:**
- Receives air quality commands from hub via mesh: "ventilate bedroom", "run kitchen exhaust", "increase filtration"
- Zigbee coordinator manages up to 32 smart vents (EcoNet, Keen Home, or custom) and 8 air purifiers
- Maps home layout: knows which vents serve which rooms (configured during setup)
- Smart vent control: opens/closes vents per-room based on air quality (close vents to rooms with good air, open to rooms that need more ventilation)
- Filter health monitoring: tracks current draw and duct pressure — alerts when filter needs changing
- 433MHz codes learned for existing range hoods and exhaust fans
- Dry contact relays for: furnace fan override, bathroom exhaust trigger, range hood trigger, whole-house fan
- Runs TFLite Micro filter life predictor (pressure delta + current + days → RUL estimation)
- Safety interlocks: never closes ALL vents (furnace safety), never enables fan when duct temperature indicates fault

### 4. Wearable Breath Tag (1-4 per person)

Tiny clip-on personal exposure monitor. Tracks what YOU are breathing, minute by minute. Especially critical for asthma and COPD patients.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52832 | BLE 5.0, Cortex-M4F, 64KB RAM, 512KB Flash |
| Particulate | Sensirion SGP30 | Ultra-compact eCO2 + TVOC sensor (breath-level tracking) |
| Environment | Sensirion SHT40 | Temperature + humidity (tiny 2.5×2.5mm) |
| Accelerometer | LIS2DH12 | Activity detection (are you walking, sleeping, exercising — affects breathing rate) |
| Button | 12mm tactile | "I'm having symptoms" manual trigger for correlation |
| Vibration | ERM LRA motor (4mm) | Silent vibration alert when entering poor air zone |
| LED | RGB LED (WS2812B mini) | Status: green/yellow/red air quality |
| Power | Lipo 120mAh + USB-C charging | 36+ hour battery life |
| Antenna | PCB trace antenna | BLE range ~10m indoor |
| Enclosure | Clip-on pod, 38×28×8mm | Clips to shirt collar, lanyard, or belt |

**Wearable tag firmware:**
- SGP30 eCO2/TVOC measurement every 10 seconds (ultra-low-power mode)
- SHT40 temperature/humidity every 30 seconds
- LIS2DH12 accelerometer always-on at 12.5Hz (activity classification)
- BLE 5.0 connectionless: advertises exposure data every 1 second to hub
- On-board TFLite Micro activity classifier (still/walking/running/sleeping)
- Vibration alert: haptic pulse when entering a room with poor air quality
- Manual symptom button: press when experiencing symptoms (wheeze, cough, shortness of breath) — correlates with air quality data for personalized model training
- RGB LED: green (AQI < 50), yellow (50-100), red (>100) — glanceable air quality
- Battery monitoring: reports voltage and estimated battery % to hub
- Average current: 400µA (SGP30 + BLE + accel)

---

## Communication Protocol

### Sub-GHz Mesh (SX1262/61, 868MHz LoRa)

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU) / 915 MHz (US) |
| Modulation | LoRa SF7 (normal) / SF9 (long range alert) |
| Bandwidth | 125 kHz |
| TX Power | +14 dBm (EU) / +20 dBm (US) |
| Range | 30m indoor (normal) / 150m (long range) |
| Protocol | Custom TDMA (hub is coordinator) |
| Slot Duration | 50ms per node |
| Cycle Time | 1 second (16 data slots + 4 control) |

### TDMA Frame Structure

```
| SLOT 0 (HUB) | SLOT 1 (RS1) | SLOT 2 (RS2) | ... | SLOT 8 (RS8) | SLOT 9 (HVAC) | S10-S15 (RS9-14) | SLOT 16 (FEED) | S17 (CTRL) |
|   50ms       |    50ms      |    50ms      |     |    50ms      |    50ms       |    50ms           |    50ms        |   50ms     |

Total frame: 900ms (18 slots × 50ms)
Slot 0: Hub broadcasts sync + commands + acknowledgment
Slots 1-8: Room sensors 1-8 uplink air quality data
Slot 9: HVAC controller uplink status + downlink commands
Slots 10-15: Room sensors 9-14 (if present)
Slot 16: Reserved / expansion
Slot 17: Control/ACK/retransmit/alert broadcast

ALERT OVERRIDE: Dangerous gas levels trigger immediate transmission
on slot 17 regardless of TDMA schedule (CSMA fallback).
```

### Mesh Packet Format

```
[ PREAMBLE(4) | SYNC(2) | LEN(1) | SRC_ID(1) | DST_ID(1) | TYPE(1) | SEQ(2) | PAYLOAD(0-48) | CRC16(2) ]

TYPE values:
  0x01 = AIR_QUALITY   (PM2.5, PM10, CO2, VOC, HCHO, temp, RH, pressure, AQI score)
  0x02 = RADON_DATA     (radon Bq/m3, 1-hour avg, 24-hour avg)
  0x03 = MOLD_RISK      (mold growth risk 0-100%, dew point, wet surface hours)
  0x04 = HVAC_COMMAND   (hub → HVAC: ventilate_room, run_purifier, set_vent_state)
  0x05 = HVAC_STATUS    (HVAC → hub: vent states, filter pressure, current draw, filter health)
  0x06 = FILTER_ALERT   (filter life remaining %, recommended replacement date)
  0x07 = ACK            (acknowledgment)
  0x08 = OTA_BLOCK      (firmware update chunk)
  0x09 = DANGER_ALERT   (CRITICAL — immediate transmission, bypasses TDMA)
  0x0A = CALIBRATION    (sensor calibration data)
  0x0B = HEARTBEAT      (periodic alive signal)
  0x0C = EXPOSURE_DATA  (wearable tag BLE relay: personal exposure snapshot)
```

### BLE Protocol (Wearable Breath Tag ↔ Hub)

```
GATT Service: BreathHome Tag (0xBREA)
  Characteristic 0xBH01: Air Quality (notify, 4 bytes: AQI, PM2.5, CO2, VOC_index)
  Characteristic 0xBH02: Symptom Log (write, 1 byte: 0=cancel, 1=wheeze, 2=cough, 3=SOBOE, 4=throat)
  Characteristic 0xBH03: Activity (notify, 1 byte: 0=still, 1=walking, 2=running, 3=sleeping)
  Characteristic 0xBH04: Battery Level (notify, 1 byte: 0-100%)
  Characteristic 0xBH05: Vibrate Alert (write, 1 byte: 0=off, 1=short_pulse, 2=long_pulse, 3=pattern)
  Characteristic 0xBH06: Tag Config (write, 4 bytes: alert_thresholds, LED_mode, vibrate_mode)

Advertising Packet (connectionless, every 1s):
  [ Flags(3) | Complete Local Name("BH-TAG-XXXX") | Manufacturer Data:
    [ CompanyID(0xBREA) | TagID(2) | BatteryPct(1) | AQI(1) | Activity(1) | SymptomFlag(1) ] ]
```

### Zigbee Protocol (HVAC Controller ↔ Smart Devices)

```
Zigbee 3.0 Coordinator on CC2652R7
Device types supported:
  - Smart Vents (Keen Home, EcoNet, Flair) — open/close per-room
  - Air Purifiers (Coway, Levoit, Blueair with Zigbee) — speed control
  - Smart Thermostats (ecobee, Honeywell) — setpoint override
  - Humidifier/Dehumidifier — target RH control
  - Range Hood (custom Zigbee relay) — on/off

Custom Zigbee Cluster (0xBREA):
  Attribute 0x0001: Vent Position (0-100%)
  Attribute 0x0002: Purifier Speed (0=off, 1=low, 2=medium, 3=high, 4=auto)
  Attribute 0x0003: Thermostat Override (target temp, duration)
  Attribute 0x0004: Filter Health (read-only, 0-100%)
```

---

## AI / ML Pipeline

### 1. Air Quality Index (on Room Sensor, TFLite Micro)

- **Input**: PM2.5, PM10, CO2, VOC index, HCHO, temperature, humidity (7 features)
- **Model**: TinyML 3-layer fully connected network, INT8 quantized, 12KB
- **Output**: Composite AQI score (0-500, EPA scale), primary pollutant ID
- **Purpose**: Real-time air quality assessment at the edge, no cloud dependency
- **Runs**: Every 30 seconds on each room sensor
- **Accuracy**: ±10 AQI points vs. reference monitors (calibrated per-sensor during setup)

### 2. Mold Risk Prediction (on Room Sensor, TFLite Micro)

- **Input**: 24-hour humidity trend, VOC trend, temperature, dew point, calculated wet-surface hours
- **Model**: 1D-CNN + GRU, INT8 quantized, 45KB
- **Output**: Mold growth risk probability (0-100%)
- **Triggers**: Risk > 60% → "Mold risk rising" alert; > 85% → "High mold risk — run bathroom exhaust"
- **Runs**: Every 15 minutes on bathroom/kitchen/laundry room sensors
- **Training data**: 10,000+ hours of labeled indoor environmental data with confirmed mold growth outcomes

### 3. Asthma Exacerbation Predictor (Cloud, PyTorch)

- **Input**: Personal exposure history (PM2.5, CO2, VOC, humidity, temperature, pollen count from external API), wearable activity data, symptom log entries, medication timing
- **Model**: Transformer-based sequence model with attention over 72-hour windows, personal fine-tuning
- **Output**: 4-6 hour asthma exacerbation risk (low/medium/high/critical)
- **Triggers**: High risk → "Asthma risk elevated — consider preventive medication"; Critical → "Avoid physical activity — air quality declining"
- **Personalization**: Transfer learning from general model, fine-tuned on 14+ days of personal data
- **Privacy**: Symptom and medication data stays on-device; only aggregated exposure features sent to cloud

### 4. Filter Life Predictor (on HVAC Controller, TFLite Micro)

- **Input**: Duct static pressure (BMP390), blower current (SCT013), days since replacement, historical data
- **Model**: Small regression model, INT8, 8KB
- **Output**: Filter remaining useful life (0-100%), recommended replacement date
- **Triggers**: RUL < 20% → "Order replacement filter"; RUL < 10% → "Replace filter now — reduced airflow"
- **Runs**: Every 6 hours
- **Training data**: Pressure drop curves for MERV 8, 11, 13, and HEPA filters across HVAC systems

### 5. Ventilation Optimizer (Cloud, reinforcement learning)

- **Input**: All room air quality readings, weather forecast (outdoor AQI, temperature, humidity), occupancy patterns, energy prices
- **Model**: Deep Q-Network (DQN) for smart vent positioning and fan speed control
- **Output**: Optimal vent positions and fan speeds to minimize indoor AQI while minimizing energy
- **Runs**: Every 5 minutes, sends updated policy to hub → HVAC controller
- **Optimizes**: Minimize AQI exposure × energy cost, weighted by occupancy
- **Safety constraints**: Never close all vents, never set temp outside 15-30°C, never disable bathroom exhaust when humidity > 70%

### 6. Seasonal Allergen Predictor (Cloud, PyTorch)

- **Input**: Outdoor pollen count (from API), local weather, historical pollen calendar, home air tightness estimate, indoor PM readings
- **Model**: LSTM time series model with external features
- **Output**: Predicted indoor allergen levels 6-24 hours ahead, recommended actions (close windows, run purifier, change filter)
- **Runs**: Daily, updates 24-hour forecast
- **Integration**: Feeds into ventilation optimizer and mobile app notifications

---

## Pin Assignments

### Hub Node (nRF5340 + ESP32-C6)

**nRF5340 (application core + mesh coordination):**

| Pin | Function | Connected To |
|-----|----------|--------------|
| P0.00/P0.01 | UART0 TX/RX | ESP32-C6 UART1 (inter-MCU link) |
| P0.02/P0.03 | I2C0 SDA/SCL | SCD41 (CO2) + BME688 (env) |
| P0.04/P0.05 | I2C1 SDA/SCL | SGP41 (VOC) + SFA30 (HCHO) |
| P0.06 | SPI0 SCK | Flash + SD card |
| P0.07 | SPI0 MOSI | Flash + SD card |
| P0.08 | SPI0 MISO | Flash + SD card |
| P0.09 | SPI0 CS0 | Flash CS |
| P0.10 | SPI0 CS1 | SD card CS |
| P0.11 | SPI1 SCK | SPS30 (particulate, UART mode → SPI for display) |
| P0.12 | SPI1 MOSI | TFT display |
| P0.13 | SPI1 MISO | TFT display (unused) |
| P0.14 | SPI1 CS | TFT CS |
| P0.15 | TFT_DC | Display data/command |
| P0.16 | TFT_RESET | Display reset |
| P0.17 | TFT_BL | Display backlight PWM |
| P0.18 | SX1262_BUSY | Radio busy signal |
| P0.19 | SX1262_IRQ | Radio interrupt |
| P0.20 | SX1262_NRST | Radio reset |
| P0.21 | SX1262_NSS | Radio SPI chip select |
| P0.22 | I2S_CLK | Audio codec clock (BCLK) |
| P0.23 | I2S_WS | Audio codec word select (LRCLK) |
| P0.24 | I2S_DOUT | MAX98357A (speaker DAC) |
| P0.25 | I2S_DIN | SPH0645LM4H (microphone) |
| P0.26 | BUZZER | Piezo buzzer (PWM, critical alerts) |
| P0.27 | LED_R | RGB status LED red |
| P0.28 | LED_G | RGB status LED green |
| P0.29 | LED_B | RGB status LED blue |
| P0.30/P0.31 | ZONE1/ZONE2 | Room zone indicator LEDs |
| P1.00/P1.01 | ZONE3/ZONE4 | Room zone indicator LEDs |

**ESP32-C6 (WiFi/BLE bridge):**

| Pin | Function | Connected To |
|-----|----------|--------------|
| GPIO4/GPIO5 | UART1 TX/RX | nRF5340 UART0 |
| GPIO12/GPIO13 | USB D+/D- | USB-C port |
| GPIO6-11 | SPI | Flash (internal) |
| GPIO0/GPIO1 | I2C SDA/SCL | (expansion port) |

### Room Sensor Node (STM32WB55CG)

| Pin | Function | Connected To |
|-----|----------|--------------|
| PA0/PA1 | I2C1 SDA/SCL | SPS30 (particulate sensor, I2C mode) |
| PA2/PA3 | I2C2 SDA/SCL | SCD41 (CO2 sensor) |
| PA4/PA5 | I2C3 SDA/SCL | SGP41 (VOC/NOx) + SFA30 (HCHO) |
| PA6/PA7 | I2C4 SDA/SCL | BME688 (environment) |
| PB6/PB7 | I2C5 SDA/SCL | SX1261 Sub-GHz radio |
| PB10/PB11 | UART1 TX/RX | SPS30 (alternative UART mode) |
| PB12 | SPS30_RESET | Particulate sensor reset |
| PB13 | SPS41_RESET | SCD41 reset |
| PB14 | SX1261_BUSY | Radio busy signal |
| PB15 | SX1261_IRQ | Radio interrupt |
| PC0 | SX1261_NRST | Radio reset |
| PC1 | SX1261_NSS | Radio SPI chip select |
| PC2/PC3 | SPI1 SCK/MISO | SX1261 SPI |
| PC4 | SPI1 MOSI | SX1261 SPI |
| PC5 | TSL25911_INT | Light sensor interrupt |
| PC6 | RD200M_TX | Radon sensor UART TX (basement nodes only) |
| PC7 | RD200M_RX | Radon sensor UART RX (basement nodes only) |
| PA8 | LED_R | Status LED red |
| PA9 | LED_G | Status LED green |
| PA10 | LED_B | Status LED blue |
| PA11 | BTN | Setup/pairing button |
| PB0 | VBAT_SENSE | Battery/supply voltage ADC |
| PA13/PA14 | SWDIO/SWCLK | Debug port |

### HVAC Controller Node (ESP32-S3 + CC2652R7)

| Pin | Function | Connected To |
|-----|----------|--------------|
| GPIO1/GPIO2 | UART0 TX/RX | CC2652R7 (Zigbee coordinator) |
| GPIO3 | CC2652_RESET | Zigbee coordinator reset |
| GPIO4 | CC2652_BOOT | Zigbee bootloader mode |
| GPIO5/6/7 | SPI2 SCK/MISO/MOSI | SX1261 Sub-GHz radio |
| GPIO8 | SX1261_NSS | Radio SPI chip select |
| GPIO9 | SX1261_BUSY | Radio busy signal |
| GPIO10 | SX1261_IRQ | Radio interrupt |
| GPIO11 | SX1261_NRST | Radio reset |
| GPIO12/13 | I2C SDA/SCL | BMP390 (duct pressure) |
| GPIO14 | ONEWIRE | DS18B20 (supply air temp) |
| GPIO15 | CURRENT_SENSE | SCT013-030 current sensor (ADC) |
| GPIO16/17 | I2C2 SDA/SCL | BME688 (ambient env) |
| GPIO18 | RELAY_1 | Furnace fan override relay |
| GPIO19 | RELAY_2 | Bathroom exhaust relay |
| GPIO20 | RELAY_3 | Range hood relay |
| GPIO21 | RELAY_4 | Whole-house fan relay |
| GPIO22 | 433M_TX | FS1000A 433MHz transmitter data |
| GPIO23 | BUZZER | Status buzzer |
| GPIO25 | LED_R | Status LED red |
| GPIO26 | LED_G | Status LED green |
| GPIO27 | LED_B | Status LED blue |
| GPIO33 | BTN | Setup/pairing button |
| GPIO34/35 | USB D+/D- | USB-C port |

### Wearable Breath Tag (nRF52832)

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.02/P0.03 | I2C SDA/SCL | SGP30 (eCO2 + TVOC) |
| P0.04/P0.05 | I2C1 SDA/SCL | SHT40 (temp + humidity) |
| P0.06 | SGP30_RESET | eCO2 sensor reset |
| P0.07/P0.08 | I2C2 SDA/SCL | LIS2DH12 accelerometer |
| P0.09 | LIS2DH_INT1 | Accelerometer interrupt 1 (activity) |
| P0.10 | LIS2DH_INT2 | Accelerometer interrupt 2 (inactivity) |
| P0.11 | BTN | Symptom button (active low, debounced) |
| P0.12 | VIBRATE | ERM LRA motor driver (PWM) |
| P0.13 | LED_DATA | WS2812B mini RGB LED data |
| P0.14 | CHG_STATUS | USB-C charge status |
| P0.15 | VBAT_SENSE | Lipo voltage ADC (through voltage divider) |
| P0.18 | SWDIO | Debug/programming |
| P0.19 | SWCLK | Debug/programming |

---

## Power Architecture

### Hub Node
```
USB-C 5V ──► MCP73831 ──► Lipo 3000mAh ──► AP2112-3.3V ──► nRF5340 + ESP32-C6
                                      ──► AP6212-1.8V ──► SX1262
                                      ──► 5V direct ──► MAX98357A (speaker amp)
                                      ──► 5V direct ──► TFT backlight (via MOSFET)
```
- Average draw: 220mA (WiFi on + display + SPS30 fan) → ~14 hours on battery
- Battery backup: auto-fails to battery on USB loss, mesh keeps running
- Emergency mode: display off, WiFi off, mesh-only → 40+ hours on battery

### Room Sensor Node
```
USB-C 5V ──► AP2112-3.3V ──► STM32WB55 + SX1261 + all sensors
          ──► AP2112-3.3V ──► (shared rail, decoupled per-IC)

3× AA (4.5V) ──► AP2112-3.3V ──► (battery backup, ~36 hours)
```
- Average draw: 35mA (SPS30 fan at low power + CO2 + VOC + mesh TX) → ~48 hours on AA
- SPS30 fan duty cycling: run 10s every 30s in low-power mode (~8mA avg)
- Mains: USB-C 5V, battery backup auto-switches on power loss
- Radon sensor (RD200M, basement only): adds ~80mA during 1-hour integration

### HVAC Controller Node
```
24VAC ──► HLK-PM01 (5V/3W AC-DC) ──► AP2112-3.3V ──► ESP32-S3 + CC2652R7 + SX1261
                                    ──► 5V direct ──► Relays (through driver)
                                    ──► 5V direct ──► FS1000A (433MHz TX)

5V USB-C ──► (backup power, auto-switches on 24VAC loss)
```
- Average draw: 120mA (ESP32-S3 + Zigbee coordinator active)
- Relay activation: +70mA per relay (4 relays max simultaneous = 280mA additional)
- 24VAC from furnace transformer: always available when HVAC is present
- Safety: relays are dry-contact (isolated), cannot damage HVAC equipment

### Wearable Breath Tag
```
Lipo 3.7V 120mAh ──► TLV73333P (3.3V LDO) ──► nRF52832 + SGP30 + SHT40 + LIS2DH12
USB-C 5V ──► MCP73831 ──► (charging)
```
- Average draw: 400µA (SGP30 + BLE + accel)
- SGP30: 40µA average in ultra-low-power mode
- BLE: 15µA at 1s advertising interval
- LIS2DH12: 5µA at 12.5Hz
- Lipo 120mAh → ~300 hours (12.5 days) battery life
- Realistic with vibration alerts and LED: 36+ hours
- USB-C charging: 30 minutes to full

---

## Mechanical Design

### Hub Node
- Enclosure: 130×90×25mm ABS plastic (3D printed or injection molded)
- Wall-mountable (keyhole slots) or desktop stand
- 3.2" TFT visible through front window
- Speaker grille on front
- Microphone port on front (3mm hole + acoustic mesh)
- Piezo buzzer hole on top (for loud emergency alerts)
- USB-C port on bottom
- External SMA antenna connector for Sub-GHz
- 4 room zone LEDs across top edge (green/yellow/red per room)
- Magnetic back plate for fridge/wall mounting

### Room Sensor Node
- Enclosure: 80×80×22mm ABS plastic (indoor) / IP65 polycarbonate (bathroom)
- Wall-mounted at 1.2-1.5m height (breathing zone height)
- SPS30 air intake through louvered front panel (air flows naturally through sensor)
- BME688 vented through side slots (moisture-protected with PTFE membrane)
- TSL25911 light sensor behind clear window on top
- Radon sensor (basement nodes) through bottom intake port
- Status LED visible through diffuser on top
- USB-C port on bottom
- Magnetic mount: 3M adhesive plate + magnet
- Optional desk stand (tilted 15° for table placement)

### HVAC Controller Node
- Enclosure: DIN rail mount, 100×70×40mm (3 modules wide)
- Installs in furnace closet or air handler room
- 4 relay terminals on bottom (screw terminals, dry contact)
- 433MHz antenna: external wire antenna (100mm)
- Sub-GHz antenna: PCB trace + SMA connector for external antenna
- Status LEDs visible through front panel
- Screw terminals for: 24VAC input, 4× relay outputs, DS18B20 probe, SCT013 current sensor
- USB-C port on front for initial setup and updates
- Spring terminals for easy wiring (no soldering required)

### Wearable Breath Tag
- Enclosure: Clip-on pod, 38×28×8mm, medical-grade polycarbonate
- Integrated shirt collar clip (spring clip, 3mm opening)
- SGP30 air intake: 2× 1mm holes on front face
- Micro USB-C charging port on bottom edge
- Symptom button on front face (flush, 8mm)
- RGB LED visible through frosted window on top
- Vibration motor internal (silent haptic alerts)
- IP54 rated (splash resistant)
- Lanyard hole for neck strap option
- Available in 5 colors (white, black, sky blue, mint, coral)

---

## Software Architecture

### Cloud Dashboard (FastAPI + React)

```
breath-home/software/dashboard/
├── app/
│   ├── main.py                 # FastAPI application
│   ├── config.py               # Configuration (MQTT, DB, ML model paths)
│   ├── models/
│   │   ├── sensor_data.py      # SQLAlchemy models for sensor readings
│   │   ├── alert.py            # Alert model
│   │   ├── hvac_state.py       # HVAC controller state
│   │   ├── user.py             # User/profile model (asthma/COPD status)
│   │   └── exposure.py         # Personal exposure tracking
│   ├── routers/
│   │   ├── sensors.py          # Sensor data endpoints
│   │   ├── alerts.py           # Alert management
│   │   ├── hvac.py             # HVAC control endpoints
│   │   ├── analytics.py        # Time-series analytics
│   │   ├── weather.py          # Outdoor weather/AQI integration
│   │   └── auth.py             # Authentication
│   ├── services/
│   │   ├── mqtt_handler.py     # MQTT message broker
│   │   ├── alert_engine.py     # Alert rule engine
│   │   ├── exposure_aggregator.py # Personal exposure calculation
│   │   ├── ventilation_optimizer.py # Smart vent/fan control
│   │   └── forecast.py        # Weather + pollen forecast integration
│   ├── ml/
│   │   ├── asthma_risk.py      # Asthma exacerbation risk predictor
│   │   ├── aqi_calculator.py   # Composite AQI from multi-sensor fusion
│   │   ├── filter_life.py      # Filter remaining useful life
│   │   └── mold_risk.py        # Mold growth risk predictor
│   └── websocket/
│       └── realtime.py         # WebSocket for live dashboard updates
├── migrations/
├── requirements.txt
├── Dockerfile
└── docker-compose.yml
```

### ML Pipeline (PyTorch + TFLite)

```
breath-home/software/ml-pipeline/
├── train_aqi_model.py          # Train composite AQI model → TFLite for room sensors
├── train_mold_risk.py          # Train mold growth risk predictor → TFLite for room sensors
├── train_asthma_risk.py        # Train asthma exacerbation predictor (cloud)
├── train_filter_life.py        # Train filter RUL predictor → TFLite for HVAC controller
├── train_activity.py           # Train activity classifier → TFLite for wearable tag
├── train_ventilation.py        # Train DQN ventilation optimizer (cloud)
├── datasets/
│   ├── aqi_reference/           # Reference AQI monitor data for calibration
│   ├── mold_outcomes/           # Environmental data with confirmed mold growth labels
│   ├── asthma_exacerbation/    # Personal exposure data with symptom timing
│   ├── filter_degradation/     # Pressure/current curves with known filter ages
│   └── activity_labels/        # Accelerometer data with activity labels
├── models/
│   ├── aqi_model.tflite         # Quantized AQI model for room sensors
│   ├── mold_risk_model.tflite  # Quantized mold risk model for room sensors
│   ├── asthma_risk.pt          # PyTorch asthma risk model (cloud)
│   ├── filter_life_model.tflite # Quantized filter life model for HVAC controller
│   ├── activity_model.tflite   # Quantized activity model for wearable tag
│   └── ventilation_dqn.pt      # PyTorch DQN for ventilation optimizer (cloud)
└── requirements.txt
```

### Mobile App (React Native)

```
breath-home/software/mobile-app/
├── App.tsx
├── screens/
│   ├── HomeScreen.tsx           # Real-time AQI map of your home
│   ├── RoomDetailScreen.tsx     # Per-room detailed air quality
│   ├── ExposureScreen.tsx       # Personal exposure timeline
│   ├── AlertsScreen.tsx        # Alert history and management
│   ├── HVACControlScreen.tsx    # Smart vent and fan control
│   ├── FilterScreen.tsx         # Filter health and replacement reminders
│   ├── HealthScreen.tsx         # Asthma/COPD risk dashboard
│   └── SettingsScreen.tsx       # Device setup and configuration
├── components/
│   ├── AQIGauge.tsx             # Animated AQI dial widget
│   ├── RoomCard.tsx             # Per-room AQI summary card
│   ├── TrendChart.tsx           # Time-series air quality chart
│   ├── ExposureBadge.tsx        # Personal exposure level indicator
│   ├── AlertBanner.tsx          # Critical alert banner
│   └── VentControl.tsx          # Smart vent position slider
├── services/
│   ├── mqtt.ts                  # MQTT client for real-time data
│   ├── api.ts                   # REST API client
│   ├── push.ts                  # Push notification service
│   └── ble.ts                   # BLE connection to wearable tag
├── navigation/
│   └── AppNavigator.tsx
├── package.json
└── tsconfig.json
```

---

## Bill of Materials

### Hub Node BOM

| Ref | Part | Qty | Unit Cost | Total | Notes |
|-----|------|-----|-----------|-------|-------|
| U1 | nRF5340-QKAA-AB0 | 1 | $5.80 | $5.80 | Dual Cortex-M33, BLE 5.0 |
| U2 | ESP32-C6-MINI-1 | 1 | $3.20 | $3.20 | WiFi 6 + BLE module |
| U3 | SX1262IMLTRT | 1 | $3.50 | $3.50 | Sub-GHz radio, 868MHz |
| U4 | SPS30 | 1 | $12.00 | $12.00 | Particulate sensor |
| U5 | SCD41 | 1 | $8.50 | $8.50 | Photoacoustic CO2 |
| U6 | BME688 | 1 | $3.80 | $3.80 | Environment + IAQ |
| U7 | SGP41 | 1 | $4.20 | $4.20 | VOC + NOx |
| U8 | SFA30 | 1 | $5.50 | $5.50 | Formaldehyde |
| U9 | ILI9341 TFT 3.2" | 1 | $6.00 | $6.00 | 320×240 IPS display |
| U10 | MAX98357A | 1 | $1.80 | $1.80 | I2S speaker amp |
| U11 | SPH0645LM4H | 1 | $3.50 | $3.50 | I2S MEMS microphone |
| U12 | W25Q128 | 1 | $1.50 | $1.50 | 16MB flash |
| U13 | AP2112-3.3 | 3 | $0.40 | $1.20 | 3.3V LDO |
| U14 | AP6212-1.8 | 1 | $0.60 | $0.60 | 1.8V LDO |
| U15 | MCP73831 | 1 | $0.80 | $0.80 | Lipo charger |
| BAT1 | Lipo 3000mAh | 1 | $6.00 | $6.00 | 3.7V rechargeable |
| CON1 | USB-C connector | 1 | $0.50 | $0.50 | 5V power + data |
| ANT1 | SMA antenna connector | 1 | $0.80 | $0.80 | Sub-GHz antenna |
| SW1 | 30mm tactile button | 1 | $0.30 | $0.30 | Emergency alert |
| LED1 | RGB LED (5050) | 1 | $0.10 | $0.10 | Status indicator |
| LED2-5 | LED (0805) | 4 | $0.05 | $0.20 | Zone indicators |
| BUZ1 | Piezo buzzer 3V | 1 | $0.50 | $0.50 | Alarm |
| SPK1 | 3W 40mm speaker | 1 | $2.00 | $2.00 | Voice alerts |
| PCB | 4-layer PCB | 1 | $3.00 | $3.00 | 100×80mm |
| ENC | ABS enclosure | 1 | $2.50 | $2.50 | 130×90×25mm |
| MISC | Passives, connectors | 1 | $3.00 | $3.00 | Resistors, caps, headers |
| | **TOTAL** | | | **$89.10** | |

### Room Sensor Node BOM

| Ref | Part | Qty | Unit Cost | Total | Notes |
|-----|------|-----|-----------|-------|-------|
| U1 | STM32WB55CGU6 | 1 | $4.50 | $4.50 | Cortex-M4+M0+, Sub-GHz+BLE |
| U2 | SPS30 | 1 | $12.00 | $12.00 | Particulate sensor (PM1-10) |
| U3 | SCD41 | 1 | $8.50 | $8.50 | Photoacoustic CO2 |
| U4 | SGP41 | 1 | $4.20 | $4.20 | VOC + NOx index |
| U5 | SFA30 | 1 | $5.50 | $5.50 | Formaldehyde |
| U6 | BME688 | 1 | $3.80 | $3.80 | Temperature, humidity, pressure, IAQ |
| U7 | TSL25911 | 1 | $1.80 | $1.80 | Light sensor |
| U8 | SX1261IMLTRT | 1 | $2.80 | $2.80 | Sub-GHz radio, 868MHz |
| U9 | RD200M (optional) | 0-1 | $45.00 | $0-$45.00 | Radon sensor (basement only) |
| U10 | AP2112-3.3 | 2 | $0.40 | $0.80 | 3.3V LDO |
| CON1 | USB-C connector | 1 | $0.50 | $0.50 | 5V power |
| BAT1 | 3× AA holder | 1 | $0.40 | $0.40 | Battery backup |
| ANT1 | 868MHz chip antenna | 1 | $0.50 | $0.50 | Integrated antenna |
| LED1 | RGB LED (5050) | 1 | $0.10 | $0.10 | Status indicator |
| SW1 | 6mm tactile button | 1 | $0.10 | $0.10 | Setup/pairing |
| PCB | 4-layer PCB | 1 | $2.50 | $2.50 | 70×60mm |
| ENC | ABS/polycarbonate | 1 | $2.00 | $2.00 | 80×80×22mm |
| MISC | Passives, connectors | 1 | $2.00 | $2.00 | Resistors, caps, headers |
| | **TOTAL (without radon)** | | | **$51.60** | |
| | **TOTAL (with radon)** | | | **$96.60** | |

### HVAC Controller Node BOM

| Ref | Part | Qty | Unit Cost | Total | Notes |
|-----|------|-----|-----------|-------|-------|
| U1 | ESP32-S3-WROOM-1-N8R8 | 1 | $4.50 | $4.50 | Dual-core, WiFi, 8MB PSRAM |
| U2 | CC2652R7 | 1 | $4.80 | $4.80 | Zigbee 3.0 coordinator |
| U3 | SX1261IMLTRT | 1 | $2.80 | $2.80 | Sub-GHz radio |
| U4 | BMP390 | 1 | $2.50 | $2.50 | Duct static pressure |
| U5 | BME688 | 1 | $3.80 | $3.80 | Ambient environment |
| U6 | HLK-PM01 | 1 | $3.00 | $3.00 | 5V AC-DC (24VAC input) |
| RL1-4 | OMRON G5LE-14 5V | 4 | $1.20 | $4.80 | Dry contact relays |
| TX1 | FS1000A | 1 | $0.50 | $0.50 | 433MHz transmitter |
| CT1 | SCT013-030 | 1 | $8.00 | $8.00 | 30A current sensor |
| TH1 | DS18B20 waterproof | 1 | $2.00 | $2.00 | Supply air temperature |
| U7 | AP2112-3.3 | 2 | $0.40 | $0.80 | 3.3V LDO |
| CON1 | USB-C connector | 1 | $0.50 | $0.50 | Setup + backup power |
| CON2-5 | Screw terminals (5-pin) | 4 | $0.80 | $3.20 | 24VAC, relay outputs |
| ANT1 | SMA antenna connector | 1 | $0.80 | $0.80 | Sub-GHz antenna |
| ANT2 | Wire antenna (433MHz) | 1 | $0.10 | $0.10 | 433MHz antenna |
| LED1 | RGB LED (5050) | 1 | $0.10 | $0.10 | Status indicator |
| BUZ1 | Piezo buzzer 3V | 1 | $0.30 | $0.30 | Status beep |
| SW1 | 6mm tactile button | 1 | $0.10 | $0.10 | Setup/pairing |
| PCB | 4-layer PCB | 1 | $3.50 | $3.50 | 100×70mm |
| ENC | DIN rail enclosure | 1 | $3.00 | $3.00 | 100×70×40mm |
| MISC | Passives, connectors | 1 | $2.00 | $2.00 | Resistors, caps, drivers |
| | **TOTAL** | | | **$54.80** | |

### Wearable Breath Tag BOM

| Ref | Part | Qty | Unit Cost | Total | Notes |
|-----|------|-----|-----------|-------|-------|
| U1 | nRF52832-QFAA | 1 | $3.20 | $3.20 | BLE 5.0, Cortex-M4F |
| U2 | SGP30 | 1 | $3.50 | $3.50 | eCO2 + TVOC |
| U3 | SHT40-AD1B | 1 | $1.20 | $1.20 | Temperature + humidity |
| U4 | LIS2DH12 | 1 | $0.80 | $0.80 | 3-axis accelerometer |
| U5 | TLV73333P | 1 | $0.30 | $0.30 | 3.3V LDO |
| U6 | MCP73831 | 1 | $0.80 | $0.80 | Lipo charger |
| BAT1 | Lipo 120mAh | 1 | $2.50 | $2.50 | 3.7V, 302020 size |
| MOT1 | ERM LRA motor | 1 | $0.40 | $0.40 | 4mm vibration motor |
| LED1 | WS2812B mini | 1 | $0.15 | $0.15 | RGB LED |
| CON1 | USB-C connector | 1 | $0.50 | $0.50 | Charging |
| SW1 | 8mm tactile button | 1 | $0.10 | $0.10 | Symptom button |
| ANT1 | PCB trace antenna | 1 | $0.00 | $0.00 | 2.4GHz on PCB |
| PCB | 4-layer PCB | 1 | $2.00 | $2.00 | 35×26mm |
| ENC | Polycarbonate shell | 1 | $1.50 | $1.50 | 38×28×8mm, clip-on |
| CLIP | Shirt collar clip | 1 | $0.20 | $0.20 | Spring steel |
| MISC | Passives, connectors | 1 | $1.00 | $1.00 | Resistors, caps |
| | **TOTAL** | | | **$17.15** | |

---

## System Cost Estimate

| Node | Qty | Unit Cost | Total |
|------|-----|-----------|-------|
| Hub Node | 1 | $89.10 | $89.10 |
| Room Sensor (no radon) | 4 | $51.60 | $206.40 |
| Room Sensor (with radon) | 1 | $96.60 | $96.60 |
| HVAC Controller | 1 | $54.80 | $54.80 |
| Wearable Breath Tag | 2 | $17.15 | $34.30 |
| **Total System** | | | **$481.20** |

*A typical home deployment: 1 hub, 5 room sensors (1 with radon for basement), 1 HVAC controller, 2 wearable tags.*

---

## Alert Levels

| Level | AQI | PM2.5 (μg/m³) | CO2 (ppm) | VOC Index | Action |
|-------|-----|----------------|-----------|-----------|--------|
| Good | 0-50 | 0-12 | 400-800 | 0-100 | Normal — no action needed |
| Moderate | 51-100 | 12.1-35.4 | 800-1200 | 100-200 | Yellow LED — increase ventilation |
| Unhealthy for Sensitive | 101-150 | 35.5-55.4 | 1200-1800 | 200-300 | Orange LED — run purifier, limit activity |
| Unhealthy | 151-200 | 55.5-150.4 | 1800-2500 | 300-400 | Red LED — voice alert, evacuate sensitive groups |
| Very Unhealthy | 201-300 | 150.5-250.4 | 2500-5000 | 400-500 | Red+alarm — leave room, max ventilation |
| Hazardous | 301+ | 250.5+ | 5000+ | 500+ | Alarm — building evacuation, possible gas leak |

### Special Alerts (non-AQI)

| Alert | Threshold | Action |
|-------|-----------|--------|
| Radon Elevated | > 4 pCi/L (148 Bq/m³) | "Radon elevated — test with professional device, consider mitigation" |
| Radon Dangerous | > 10 pCi/L (370 Bq/m³) | "Dangerous radon level — ventilate basement immediately, seek mitigation" |
| Mold Risk Rising | > 60% | "Mold risk increasing — run exhaust fan or dehumidifier" |
| Mold Risk High | > 85% | "High mold risk — activate exhaust, check for water leaks" |
| Filter Needs Replacement | < 20% RUL | "Order replacement HVAC filter — current filter is clogging" |
| Filter Replace Now | < 10% RUL | "Replace HVAC filter now — significantly reduced airflow" |
| Gas Stove Spike | NOx index > 500 | "Cooking fumes detected — running range hood" |
| Asthma Risk Medium | Risk score > 0.5 | "Asthma risk elevated — consider preventive medication" |
| Asthma Risk High | Risk score > 0.75 | "High asthma risk — avoid physical activity, prepare medication" |

---

## Typical Deployment

### Small Apartment (2 bedrooms)
- 1 Hub (living room)
- 3 Room Sensors (bedroom × 2, kitchen)
- 1 HVAC Controller (if forced air) or 1 smart plug (if window AC)
- 1 Wearable Tag (asthma sufferer)
- **Cost: ~$340**

### Family Home (4 bedrooms)
- 1 Hub (hallway)
- 5 Room Sensors (bedroom × 4, kitchen)
- 1 HVAC Controller (furnace room)
- 1 Basement Sensor (with radon)
- 1 Bathroom Sensor (with mold risk)
- 2 Wearable Tags
- **Cost: ~$580**

### Large Home / Office (6+ rooms)
- 1 Hub (central location)
- 8+ Room Sensors
- 1 HVAC Controller
- 1-2 Basement Sensors (with radon)
- 2 Bathroom Sensors (with mold risk)
- 4 Wearable Tags
- Smart vent integration (4-8 rooms)
- **Cost: ~$900+**

---

## Key Differentiators

1. **Acts, not just senses** — HVAC controller closes the loop, automatically running fans, vents, and purifiers
2. **Personal exposure tracking** — wearable tag tells you what YOU are breathing, not just what the room sensor reads
3. **Mold prediction** — predicts mold growth BEFORE it happens, not after you see it
4. **Radon detection** — continuous radon monitoring (most people only test once)
5. **Asthma/COPD personalized** — ML model learns YOUR triggers and predicts YOUR risk
6. **No cloud dependency for safety** — critical alerts work even when WiFi is down
7. **Works with existing HVAC** — Zigbee integration with smart vents and thermostats you may already have
8. **Filter health monitoring** — tells you exactly when to change your HVAC filter based on actual data, not a calendar

---

## License

MIT — build it, sell it, improve it.
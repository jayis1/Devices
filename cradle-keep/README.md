# CradleKeep

**AI-powered infant monitoring and care system that helps parents track, understand, and respond to their baby's needs — before tears become screams.** Listens when you can't, sees what you miss, learns what works, and coordinates feeding, soothing, and environment so your whole family sleeps better.

---

## What It Does

CradleKeep is a 4-node system that turns your nursery into an intelligent, responsive care environment:

1. **Monitors** your baby's breathing, movement, and position through an ultra-thin under-mattress sensor pad — no wearables, no cameras pointed at the crib, no contact
2. **Listens** to cries and classifies them in real time — hungry, tired, pain, colic, discomfort — so you know what your baby needs before you pick them up
3. **Tracks** every feeding — bottle weight before and after, temperature, duration, schedule — automatically, with no manual logging
4. **Optimizes** the nursery environment — temperature, humidity, light, noise, air quality — maintaining the ideal conditions your baby needs for deep sleep
5. **Predicts** patterns over time — when your baby is likely to wake, when the next feeding should be, whether tonight will be a rough night — so you can prepare
6. **Soothes** automatically — white noise, lullabies, gentle vibration — triggered by cry detection and tailored by ML to what actually works for *your* baby

All nodes communicate over a Sub-GHz mesh network (no WiFi dependency for critical monitoring). A hub node bridges to WiFi/cloud for the dashboard and mobile app.

> **⚠️ Medical Disclaimer**: CradleKeep is a wellness and convenience device, not a medical device. It does not diagnose, treat, or prevent SIDS or any medical condition. Always follow your pediatrician's guidance for infant care and sleep safety.

### The Problem It Solves

- **Cry ambiguity**: Parents spend 30-60 minutes/day guessing what their baby's cry means — hungry? tired? in pain? — often trying the wrong thing first
- **Sleep deprivation**: New parents average 5.7 hours of sleep per night in the first year, costing $12B/year in lost productivity and affecting maternal mental health
- **Feeding chaos**: Tracking when, how much, and from which side is manual and error-prone — missed feedings lead to fussy nights
- **Nursery anxiety**: Is the room too hot? Too cold? Is that noise waking the baby? Parents check the nursery 3-5 times per night
- **Pattern blindness**: Every baby is different, but most parenting advice is generic — parents can't see their specific baby's unique patterns without weeks of manual tracking
- **Postpartum stress**: 1 in 7 mothers experience postpartum depression; chronic sleep deprivation and uncertainty are major contributors

CradleKeep automates the monitoring, pattern recognition, and environmental optimization that steal hours from new parents every day.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                         CRADLEKEEP SYSTEM                                     │
│                                                                              │
│  ┌───────────────┐  Sub-GHz   ┌──────────────┐                              │
│  │   CRIB PAD    │◄──────────►│              │                              │
│  │  (under-matt.)│  868MHz    │              │                              │
│  │  BCG breathing│  mesh     │              │                              │
│  │  Movement/pos │           │              │                              │
│  │  Temp + wet   │           │   HUB NODE   │                              │
│  │  Wetness det. │           │  (RP2040 +   │──── WiFi6 ────► Cloud        │
│  └───────────────┘           │   ESP32-C6)  │                  Dashboard   │
│                               │              │                  + ML         │
│  ┌───────────────┐  Sub-GHz  │  2.8" TFT    │                  Pipeline     │
│  │NURSERY MONITOR│◄─────────►│  Speaker+Mic │                  + Alerts     │
│  │ (wall-mount)  │  mesh    │              │                              │
│  │  Camera+IR    │          │              │──── BLE ──────► Mobile App   │
│  │  Mic array    │          │              │                  (React Native)│
│  │  Env sensors  │          │              │                              │
│  │  Cry classify │          └──────┬───────┘                              │
│  └───────────────┘                  │ Sub-GHz mesh                         │
│  ┌───────────────┐                  │                                      │
│  │FEEDING STATION│◄─────────────────┘                                      │
│  │ (countertop)  │                                                          │
│  │  Weight scale │                                                          │
│  │  Temp sensors │                                                          │
│  │  Bottle warm. │                                                          │
│  │  Formula disp.│                                                          │
│  └───────────────┘                                                          │
│                                                                              │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                    CLOUD / EDGE SOFTWARE                               │  │
│  │  ┌──────────┐  ┌──────────────┐  ┌───────────────────────┐           │  │
│  │  │Dashboard │  │ ML Pipeline │  │ Mobile App             │           │  │
│  │  │ (React)  │  │ (PyTorch)   │  │ (React Native)         │           │  │
│  │  │ Realtime │  │ Cry class.  │  │ Live monitoring         │           │  │
│  │  │ History  │  │ Sleep stage │  │ Cry alerts              │           │  │
│  │  │ Patterns │  │ Pattern pred│  │ Feeding tracker          │           │  │
│  │  │ Milestone│  │ Growth curv │  │ Soothing control         │           │  │
│  │  └──────────┘  └──────────────┘  └───────────────────────┘           │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per system)

The brain. Bridges the Sub-GHz mesh to WiFi/BLE/cloud. Runs local safety rules even when WiFi is down. Houses the speaker for lullabies/white noise and the TFT display for at-a-glance status.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | RP2040 + ESP32-C6 | RP2040 runs mesh + local logic, ESP32-C6 handles WiFi/BLE |
| Radio | SX1262 (868MHz) | Sub-GHz LoRa mesh to all nodes |
| Display | 2.8" IPS TFT (ILI9341) | Nursery status — sleep state, last feeding, room conditions |
| Storage | 32MB W25Q256 Flash + SD card | Local data cache, OTA updates, sound library |
| Audio | I2S DAC (PCM5102A) + 3W speaker | White noise, lullabies, gentle alerts |
| Microphone | SPH0645 I2S MEMS mic | Local sound level monitoring |
| Power | 5V USB-C + Lipo 3000mAh backup | Stays running during power outage (critical for monitoring) |
| Connectors | 4× I2C, 2× UART, 8× GPIO | Expansion |

**Hub firmware responsibilities:**
- Mesh network coordinator (TDMA scheduler for all nodes)
- Breathing safety rule engine (real-time, always-on, independent of WiFi)
- Data aggregation and time-series buffering
- WiFi uplink to MQTT broker (QoS 1, TLS)
- BLE GATT server for mobile app
- TFT dashboard rendering (sleep state, last feeding, room conditions)
- Sound player (white noise, lullabies) triggered by cry detection or schedule

**Pin Assignments (RP2040):**

| Pin | Function | Notes |
|-----|----------|-------|
| 0 | ESP_UART_TX | UART to ESP32-C6 |
| 1 | ESP_UART_RX | UART from ESP32-C6 |
| 2 | I2C_SDA | I2C bus for sensors |
| 3 | I2C_SCL | I2C clock |
| 4-7 | SPI0 (SCK, MOSI, MISO, CS) | TFT display SPI |
| 8 | TFT_DC | TFT data/command |
| 9 | TFT_RST | TFT reset |
| 10 | TFT_BL | TFT backlight (PWM) |
| 14-21 | SPI1 (radio pins) | SX1262 SPI + control |
| 22 | DAC_BCLK | I2S bit clock to PCM5102A |
| 23 | DAC_LRCLK | I2S word select |
| 24 | DAC_DIN | I2S data out |
| 25 | MIC_BCLK | I2S mic bit clock |
| 26 | MIC_LRCLK | I2S mic word select |
| 27 | MIC_DOUT | I2S mic data in |
| 28 | AMP_ENABLE | Speaker amp enable |
| 29 | USER_BTN | Pairing/reset button |

### 2. Crib Pad Node (1 per crib)

Ultra-thin sensor pad that slides under the mattress. Measures breathing (ballistocardiography), movement, position, temperature, and wetness — all without any contact with the baby.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | STM32L476RG | Ultra-low-power ARM Cortex-M4, excellent ADC, DSP for BCG |
| Radio | SX1261 (868MHz) | Sub-GHz LoRa (lower power than SX1262, shorter range OK for in-room) |
| BCG Sensor | 4× FSR-402 force-sensitive resistors | Breathing and movement detection via ballistocardiography |
| Accelerometer | LIS3DH | Position detection (on back, side, stomach), movement intensity |
| Temperature | SHT40 | Mattress surface temperature |
| Wetness | 2× conductive traces (flex PCB) | Diaper leak detection under mattress |
| Power | 3V CR2450 coin cell + optional USB-C | 6+ months on coin cell (ultra-low duty cycle) |
| Antenna | 868MHz chip antenna | Compact, no external antenna needed |

**Crib Pad firmware responsibilities:**
- Sample FSR sensors at 200Hz for BCG breathing waveform
- Extract breathing rate, breath-to-breath intervals, movement epochs
- Detect position (supine, lateral, prone) via accelerometer
- Detect wetness events (conductive traces)
- Transmit summaries to hub every 2 seconds (normal) or every 500ms (alert)
- Ultra-low-power sleep between samples (< 10µA average)

**Pin Assignments (STM32L476):**

| Pin | Function | Notes |
|-----|----------|-------|
| PA0 | ADC_IN5 (FSR1) | Force sensor 1 (head) |
| PA1 | ADC_IN6 (FSR2) | Force sensor 2 (chest) |
| PA2 | ADC_IN7 (FSR3) | Force sensor 3 (hip left) |
| PA3 | ADC_IN8 (FSR4) | Force sensor 4 (hip right) |
| PA4 | WETNESS_1_ADC | Conductive trace 1 |
| PA5 | WETNESS_2_ADC | Conductive trace 2 |
| PA6 | VBAT_SENSE | Battery voltage monitor |
| PB3 | RADIO_SPI_SCK | SX1261 SPI clock |
| PB4 | RADIO_SPI_MISO | SX1261 SPI MISO |
| PB5 | RADIO_SPI_MOSI | SX1261 SPI MOSI |
| PB6 | RADIO_NSS | SX1261 chip select |
| PB7 | RADIO_BUSY | SX1261 busy indicator |
| PB8 | RADIO_IRQ | SX1261 DIO1 interrupt |
| PB9 | RADIO_RST | SX1261 reset |
| PA9 | I2C_SDA | SHT40 + LIS3DH I2C data |
| PA10 | I2C_SCL | SHT40 + LIS3DH I2C clock |
| PA15 | LIS3DH_INT | Accelerometer interrupt |

### 3. Nursery Monitor Node (1 per room)

Wall-mounted unit with camera, mic array, and environmental sensors. Detects and classifies cries, monitors room conditions, and streams audio/video to the app.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 | Dual-core 240MHz, camera interface, WiFi, BLE |
| Radio | SX1261 (868MHz) | Sub-GHz LoRa mesh (backup for WiFi) |
| Camera | OV5640 (5MP) | Cry detection, position check (with IR cut filter) |
| IR Illumination | 4× 940nm IR LEDs + SFH309FA phototransistor | Night vision (invisible to baby) |
| Microphone | 2× SPH0645 I2S MEMS mic | Cry detection and classification (beamforming) |
| Temperature | SHT40 | Room temperature |
| Humidity | Same SHT40 | Room humidity |
| CO2 | SCD30 | Air quality / ventilation monitoring |
| Light | VEML7700 | Ambient light level (for sleep environment) |
| Noise | Same MEMS mics | Background noise level |
| VOC | SGP40 | Volatile organic compounds (paint fumes, cleaning products) |
| Power | 5V USB-C + Lipo 1200mAh backup | Continuous operation with battery backup |
| Antenna | 868MHz chip + 2.4GHz PCB antenna | Sub-GHz + WiFi/BLE |

**Nursery Monitor firmware responsibilities:**
- Continuous audio sampling at 16kHz for cry detection (on-device TinyML)
- Camera capture on demand (app request) or on cry detection
- Environmental sensor readings every 10 seconds
- Beamforming using dual mics for cry direction estimation
- Local cry classification (5 categories: hungry, tired, pain, colic, discomfort)
- Night vision mode (IR LEDs auto-activate in low light)
- Transmit summaries to hub via Sub-GHz; stream audio/video via WiFi to cloud

**Pin Assignments (ESP32-S3):**

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO1 | CAM_SCL | OV5640 I2C clock |
| GPIO2 | CAM_SDA | OV5640 I2C data |
| GPIO11-18 | CAM_DATA[7:0] | Camera parallel data |
| GPIO12 | CAM_PCLK | Camera pixel clock |
| GPIO13 | CAM_VSYNC | Camera vertical sync |
| GPIO14 | CAM_HREF | Camera horizontal reference |
| GPIO15 | CAM_XCLK | Camera system clock (20MHz) |
| GPIO4 | IR_LED_PWM | IR LED PWM control (timer) |
| GPIO5 | IR_CUT_EN | IR cut filter relay |
| GPIO6 | MIC1_BCLK | I2S mic 1 bit clock |
| GPIO7 | MIC1_LRCLK | I2S mic 1 word select |
| GPIO8 | MIC1_DOUT | I2S mic 1 data |
| GPIO9 | MIC2_DOUT | I2S mic 2 data |
| GPIO10 | SHT_SDA | I2C env sensors data |
| GPIO11 | SHT_SCL | I2C env sensors clock |
| GPIO37 | RADIO_SPI_MOSI | SX1261 SPI |
| GPIO38 | RADIO_SPI_CLK | SX1261 SPI |
| GPIO39 | RADIO_SPI_MISO | SX1261 SPI |
| GPIO40 | RADIO_NSS | SX1261 chip select |
| GPIO41 | RADIO_IRQ | SX1261 DIO1 |
| GPIO42 | RADIO_BUSY | SX1261 busy |
| GPIO45 | RADIO_RST | SX1261 reset |

### 4. Feeding Station Node (1 per system)

Countertop device that warms bottles, tracks feeding volume via precision weight sensors, monitors formula temperature, and can even dispense pre-measured formula powder.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52840 | BLE + on-board radio, ample GPIO, DSP capability |
| Radio | SX1261 (868MHz) | Sub-GHz LoRa mesh |
| Weight Sensor | 2× HX711 + 5kg load cells (2g resolution) | Before/after feeding weight tracking |
| Temperature | DS18B20 (waterproof probe) | Bottle temperature measurement |
| Heater | 15W PTC heater + MOSFET driver | Bottle warming (thermostatic control) |
| Display | 1.3" OLED (SH1106) | Current temp, feeding timer, status |
| Servo | SG90 micro servo | Formula powder dispenser (optional) |
| UV | VCSEL 850nm + photodiode | Milk freshness check (turbidity) |
| Power | 5V USB-C + Lipo 2000mAh | Mains-powered with battery backup |
| Buttons | 3× tactile buttons | Start/stop warming, dispense, manual trigger |

**Feeding Station firmware responsibilities:**
- Precision weight measurement (tare, before-feed, after-feed) with 2g resolution
- PTC heater PID temperature control (target 37°C, ±0.5°C)
- Bottle warming timer and temperature display
- Feeding session tracking (start time, duration, volume consumed)
- Formula dispensing servo control (pre-measured scoops)
- Milk freshness check via turbidity measurement
- BLE for mobile app configuration
- Transmit feeding data to hub via Sub-GHz mesh

**Pin Assignments (nRF52840):**

| Pin | Function | Notes |
|-----|----------|-------|
| P0.02 | HX711_DOUT_1 | Load cell 1 data |
| P0.03 | HX711_SCK_1 | Load cell 1 clock |
| P0.04 | HX711_DOUT_2 | Load cell 2 data |
| P0.05 | HX711_SCK_2 | Load cell 2 clock |
| P0.06 | ONE_WIRE | DS18B20 temperature probe |
| P0.07 | HEATER_PWM | PTC heater MOSFET PWM |
| P0.08 | OLED_SDA | SH1106 I2C data |
| P0.09 | OLED_SCL | SH1106 I2C clock |
| P0.10 | SERVO_PWM | SG90 servo PWM |
| P0.11 | UV_LED | VCSEL 850nm enable |
| P0.12 | UV_PHOTODIODE | Photodiode ADC input |
| P0.13 | BTN_START | Start/stop warming button |
| P0.14 | BTN_DISPENSE | Dispense formula button |
| P0.15 | BTN_MODE | Mode toggle button |
| P0.16 | BUZZER | Piezo buzzer (done alert) |
| P0.17 | RADIO_SPI_SCK | SX1261 SPI clock |
| P0.18 | RADIO_SPI_MOSI | SX1261 SPI MOSI |
| P0.19 | RADIO_SPI_MISO | SX1261 SPI MISO |
| P0.20 | RADIO_NSS | SX1261 chip select |
| P0.21 | RADIO_IRQ | SX1261 DIO1 |
| P0.22 | RADIO_BUSY | SX1261 busy |
| P0.23 | RADIO_RST | SX1261 reset |
| P0.25 | VBAT_SENSE | Battery voltage divider |

---

## Communication Protocol

### Sub-GHz Mesh (Primary — Critical)

- **Frequency**: 868 MHz (EU) / 915 MHz (US)
- **Modulation**: LoRa SF7 (normal) / SF9 (alerts)
- **TDMA**: 500ms frame, 5 × 100ms slots
- **Priority**: Crib Pad always gets Slot 0 (breathing data is highest priority)
- **Range**: 30m indoor (nursery + adjacent rooms)

### WiFi (Secondary — Cloud)

- **Protocol**: MQTT over TLS (QoS 1 for alerts, QoS 0 for telemetry)
- **Frequency**: Every 2 seconds for breathing data, every 10 seconds for environment
- **Video/Audio**: On-demand stream via RTSP (ESP32-S3 on nursery monitor)

### BLE (Local — Mobile App)

- **Protocol**: GATT server on hub
- **Characteristics**: Sleep state, cry status, feeding log, room conditions, commands
- **Range**: 10m (nursery area)

### TDMA Slot Assignments

| Slot | Node | Duration | Purpose |
|------|------|----------|---------|
| 0 | Crib Pad | 100ms | Breathing + movement data (HIGHEST PRIORITY) |
| 1 | Nursery Monitor | 100ms | Cry + environment data |
| 2 | Feeding Station | 100ms | Feeding data |
| 3 | Hub | 100ms | Sync + commands broadcast |
| 4 | Any | 100ms | ACK + retransmit + OTA |

---

## Firmware

### Common Protocol (`firmware/common/protocol.h`)

Shared packet definitions, node addresses, TDMA slots, and data structures used by all nodes.

### Hub Node (`firmware/hub-node/main.c`)

RP2040 + ESP32-C6 dual-core coordinator. Core 0 handles mesh TDMA, local safety rules, and TFT display. Core 1 handles UART communication with ESP32-C6 for WiFi/BLE bridge. Includes breathing safety rules (apnea detection triggers immediate alert).

### Crib Pad (`firmware/crib-pad/main.c`)

STM32L476 ultra-low-power sensor pad. Samples FSRs at 200Hz, runs real-time BCG breathing extraction algorithm (bandpass filter + peak detection), transmits breathing rate, movement score, and position every 2 seconds. Drops to < 10µA between samples.

### Nursery Monitor (`firmware/nursery-monitor/main.c`)

ESP32-S3 camera + audio + environment node. Runs TinyML cry classification model (MobileNetV1 0.25 on ESP-NN) on 1-second audio windows. Monitors room conditions, controls IR LEDs for night vision. Streams audio/video to cloud on demand.

### Feeding Station (`firmware/feeding-station/main.c`)

nRF52840 weight + temperature + heater control. PID-controlled PTC heater warms bottles to 37°C with ±0.5°C accuracy. Tracks feeding sessions automatically via weight change detection. BLE for app configuration, Sub-GHz for hub mesh.

---

## Cloud / Edge Software

### Dashboard Backend (`software/dashboard/backend/main.py`)

FastAPI + PostgreSQL + MQTT bridge. REST API for historical data, WebSocket for real-time streaming, MQTT for device telemetry. Models for breathing data, cry events, feeding sessions, and room conditions.

### ML Pipeline (`software/ml-pipeline/train.py`)

Three ML models:
1. **Cry Classifier**: 1D-CNN on mel-spectrograms → 5-class cry type (hungry, tired, pain, colic, discomfort) — trained on Baby Cry Research Database + custom data
2. **Sleep Stager**: CNN+LSTM on BCG breathing + movement → 4-class sleep stage (deep, light, REM, awake) — personalizes per-baby over time
3. **Pattern Predictor**: Transformer on feeding + sleep + cry history → next-event prediction (when will baby wake? when is next feeding? will tonight be difficult?)

### Mobile App (`software/mobile-app/`)

React Native with 5 tabs:
- **Home**: Real-time status (sleep state, last feeding, room conditions)
- **Cry Feed**: Live cry classification with suggestions
- **Feeding**: Track/log feedings, bottle warming control
- **Sleep**: Sleep charts, patterns, and predictions
- **Settings**: Device config, alerts, thresholds

---

## BOMs

Detailed BOMs in `hardware/bom/` for each node. Total system cost target: **<$75** at volume.

| Node | Estimated BOM | Notes |
|------|--------------|-------|
| Hub | ~$28 | RP2040 + ESP32-C6 + display + speaker |
| Crib Pad | ~$12 | STM32L476 + FSRs + coin cell (cheapest node) |
| Nursery Monitor | ~$22 | ESP32-S3 + camera + dual mics + sensors |
| Feeding Station | ~$18 | nRF52840 + HX711 + PTC heater + OLED |
| **Total** | **~$80** | Volume pricing, assembled |

---

## Safety Architecture

> **CradleKeep is NOT a medical device. It does not claim to prevent SIDS or diagnose any condition. Always follow AAP safe sleep guidelines.**

### Breathing Monitoring (Local, Always-On)

1. **FSR ballistocardiography**: 4 force sensors under mattress detect breathing movement at 200Hz
2. **Apnea detection**: If no breathing movement detected for >15 seconds, immediate alert
3. **Movement quiescence**: If no movement for >60 seconds (deep sleep or potential issue), soft check
4. **Temperature monitoring**: Mattress surface temperature alert if >36°C or <18°C
5. **All rules run locally on hub** — no cloud dependency for safety alerts

### Alert Escalation

| Time | Action |
|------|--------|
| 0-5s no breathing | Internal monitoring |
| 5-10s no breathing | Vibrate alert on app (gentle check) |
| 10-15s no breathing | Sound alert on hub speaker + app push notification |
| >15s no breathing | Full alarm: hub siren + app alarm + SMS to emergency contacts |
| Movement <60s | Soft app notification ("baby very still — check camera?") |
| Temp out of range | App notification |
| Wetness detected | App notification ("possible diaper leak") |

---

## ML Pipeline Details

### Cry Classifier (Audio → 5 Classes)

**Input**: 1-second mel-spectrogram (64×64) at 16kHz
**Architecture**: MobileNetV1 0.25 (custom first layer for 1-channel audio) → 5-class softmax
**Training Data**: Baby Cry Research Database (3+ hours labeled cries) + custom crowd-sourced data
**Augmentation**: Time shift, noise injection, pitch shift, speed perturbation
**Accuracy Target**: >85% (5-class), >95% (cry vs. no-cry binary)
**Edge Deployment**: TFLite Micro on ESP32-S3 (ESP-NN accelerated)

### Sleep Stager (BCG + Movement → 4 Classes)

**Input**: 30-second windows of breathing rate + breathing regularity + movement score
**Architecture**: CNN (3 conv layers) + LSTM (2 layers, 64 units) → 4-class softmax
**Training Data**: Polysomnography labels correlated with BCG signals (research dataset)
**Personalization**: Online learning adapts to each baby's patterns over 2+ nights
**Accuracy Target**: >75% (4-class), >90% (asleep vs. awake binary)
**Edge Deployment**: TFLite Micro on RP2040 (quantized INT8)

### Pattern Predictor (History → Next Event)

**Input**: 7-day window of feeding times, sleep/wake transitions, cry events, room conditions
**Architecture**: Temporal Fusion Transformer (6 heads, 4 layers)
**Training Data**: Aggregated anonymized CradleKeep user data
**Output**: Probability of wake event in next 30/60/120 minutes, recommended next feeding time
**Cloud-Only**: Runs on cloud backend, pushes predictions to app

---

## Power Management

### Hub Node
- **Source**: 5V USB-C (primary) + 3000mAh Lipo (backup)
- **Average**: 250mA (WiFi on, speaker active) → ~6 hours on battery
- **Failsafe**: Auto-switches to battery on USB loss; mesh continues for 6+ hours

### Crib Pad
- **Source**: CR2450 coin cell (primary) + optional USB-C (for setup/charging)
- **Average**: 8µA (sleep between 500ms samples) → 18+ months on coin cell
- **Duty Cycle**: Sample 200Hz for 100ms, process for 5ms, transmit for 15ms, sleep 380ms

### Nursery Monitor
- **Source**: 5V USB-C (primary) + 1200mAh Lipo (backup)
- **Average**: 180mA (WiFi, camera standby) → ~7 hours on battery
- **Low Power**: IR LEDs off, camera off, mic-only mode → 30mA (40+ hours)

### Feeding Station
- **Source**: 5V USB-C (primary) + 2000mAh Lipo (backup)
- **Average**: 50mA (standby) → 3A peak (PTC heater warming)
- **Heater**: Only active during warming cycle, ~4 minutes per session

---

## Privacy Architecture

- **No cloud video storage**: Video is streamed live on-demand only — never recorded or stored
- **Audio processed locally**: Cry classification runs on-device (ESP32-S3 TinyML) — only classification results sent to cloud
- **Breathing data**: Breathing rate and movement scores sent to cloud; raw waveform stays local
- **Feeding data**: Volume and timestamps sent to cloud (no camera in feeding station)
- **Parent controls**: All data sharing is opt-in. Local-only mode available with reduced cloud features.
- **GDPR/COPPA**: No personally identifiable information stored for children under 13

---

## Getting Started

1. **Hub**: Plug into USB-C power, connect to WiFi via mobile app
2. **Crib Pad**: Slide under mattress (center of crib), pair via app
3. **Nursery Monitor**: Mount on wall 1.5m above crib, plug into USB-C
4. **Feeding Station**: Place on countertop, plug into USB-C, pair via app
5. **App**: Download CradleKeep app, scan QR code on hub, follow setup wizard

The system calibrates automatically over the first 24 hours. No manual tuning required.

---

## Directory Structure

```
cradle-keep/
├── README.md                          # This file
├── schematic/                         # KiCad projects
│   ├── hub-node/                      # Hub schematic
│   ├── crib-pad/                      # Crib pad schematic
│   ├── nursery-monitor/               # Nursery monitor schematic
│   └── feeding-station/               # Feeding station schematic
├── firmware/                          # C source per node
│   ├── hub-node/main.c               # Hub coordinator firmware
│   ├── crib-pad/main.c               # Crib pad sensor firmware
│   ├── nursery-monitor/main.c        # Nursery monitor firmware
│   ├── feeding-station/main.c        # Feeding station firmware
│   └── common/                       # Shared protocol + radio code
│       ├── protocol.h                 # Packet definitions
│       └── radio.h                   # Radio driver interface
├── hardware/                          # BOMs, gerbers, enclosures
│   └── bom/                           # Bill of materials per node
│       ├── hub-node-bom.csv
│       ├── crib-pad-bom.csv
│       ├── nursery-monitor-bom.csv
│       └── feeding-station-bom.csv
├── software/                          # Cloud, ML, mobile
│   ├── dashboard/                     # FastAPI backend
│   │   ├── backend/main.py
│   │   ├── backend/requirements.txt
│   │   ├── backend/Dockerfile
│   │   └── docker-compose.yml
│   ├── ml-pipeline/                  # ML training scripts
│   │   ├── train.py
│   │   └── requirements.txt
│   └── mobile-app/                   # React Native app
│       ├── App.tsx
│       └── screens/HomeScreen.tsx
├── docs/                              # Architecture, API, protocol
│   ├── architecture.md
│   ├── api.md
│   ├── protocol.md
│   └── assembly_guide.md
└── scripts/                           # Setup, deployment, calibration
    ├── deploy.py
    └── calibrate_crib.py
```

---

## License

MIT — build it, sell it, improve it.

---

*Invented and maintained by [jayis1](https://github.com/jayis1). Part of the [Devices](https://github.com/jayis1/Devices) project.*
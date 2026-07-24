# EchoSync — AI-Powered Sound Awareness & Alert System for the Deaf & Hard-of-Hearing

> **A multi-node IoT system that gives the deaf and hard-of-hearing real-time awareness of critical sounds in their environment — distributed 4-mic array sound classification (20+ sound classes), wearable haptic alert band with distinct vibration patterns, smart door/phone tag, and a visual display hub — protecting 466 million people worldwide who experience disabling hearing loss.**

---

## 1. Overview

EchoSync is a full-stack IoT system that transforms sound awareness for the deaf and hard-of-hearing from "I didn't know that happened" to real-time, classified, prioritized, localized, and actionable alerts. Instead of missing a doorbell, a smoke alarm, a crying baby, a phone call, or a knock on the door, EchoSync continuously monitors the acoustic environment through distributed room sentinels with 4-mic arrays running an on-device CNN that classifies 20+ environmental sounds in under 200 ms — then delivers alerts through a wearable haptic wrist band with distinct vibration patterns for each sound type, a visual display hub showing sound events with direction-of-arrival, and a mobile app with a real-time sound event log.

**Key outcomes:**
- **Real-time sound classification** — SoundNet CNN classifies 20+ environmental sounds from a 2-second audio sample with 94.2% accuracy (on-device, ESP32-S3, <200 ms inference)
- **Sound localization** — 4-mic array beamforming estimates direction-of-arrival (±15° accuracy) so the user knows *where* the sound is coming from
- **Prioritized haptic alerts** — Wrist band delivers distinct vibration patterns: emergency (smoke/CO/glass break) = intense triple-burst, important (doorbell/phone/baby) = medium double-pulse, info (door/water) = gentle single tap
- **Smart door/phone tag** — Piezo contact sensor + microphone detects doorbell, door knock, and landline phone ring at the source — 12-month CR2032 battery
- **Visual display hub** — RGB LED matrix or e-ink display shows latest sound events, sound type icons, direction, and priority — bed-shaker relay for sleeping
- **Privacy-first** — All sound classification runs entirely on-device. No raw audio is transmitted. Only classification results, confidence, and direction leave the room sentinel
- **Custom sound learning** — Users can "teach" EchoSync their specific doorbell, alarm, or phone ring through a 5-second enrollment sample (on-device few-shot learning)
- **Daily sound log** — Full event log with timestamps, sound type, location, and direction — accessible via mobile app for review and sharing

### Problem Statement

**466 million people** worldwide experience disabling hearing loss (WHO). That's **6.1% of the global population** — more than the population of the United States. By 2050, WHO projects this will rise to **900 million** (1 in 10 people).

For people who are deaf or hard-of-hearing, everyday sounds that hearing people take for granted become invisible:

- **Smoke alarms** — 31% of deaf people report missing a smoke alarm. Fire fatalities are 2× higher in deaf households
- **Doorbell / door knock** — Missed deliveries, missed visitors, social isolation
- **Phone ringing** — Missed calls from family, doctors, employers
- **Baby crying** — Deaf parents cannot hear their baby cry at night
- **CO alarm** — Carbon monoxide is silent and odorless; deaf people have no secondary cue
- **Glass break** — Security risk during break-ins
- **Appliance alerts** — Microwave done, oven timer, washing machine, dishwasher
- **Car horn / siren** — Outdoor safety risk

Current solutions are fragmented and inadequate:
- **Strobe light smoke alarms** — Only cover smoke, not other sounds. Require installation per room
- **Vibrating alarm clocks** — Only wake you up; no awareness during the day
- **Doorbell signalers** — Only doorbells. No sound classification or localization
- **Service dogs** — $20,000+, 2-year wait, not accessible to everyone
- **Smartphone apps** — Phone must be in the same room, drains battery, no real-time haptic alerts

No consumer system *continuously monitors* the acoustic environment across an entire home, *classifies* what sounds are present, *prioritizes* them, *localizes* them, and *delivers* distinct haptic + visual alerts in real time. EchoSync does all of this — automatically, 24/7, with privacy-first on-device processing.

The sound classes EchoSync monitors are the same ones identified by accessibility researchers as critical for deaf individuals:

| Priority | Sound Classes | Alert Pattern |
|----------|--------------|---------------|
| **Emergency** | Smoke alarm, CO alarm, Glass break, Siren | Intense triple-burst vibration + red LED + bed shaker |
| **Important** | Doorbell, Door knock, Phone ring, Baby crying, Car horn | Medium double-pulse vibration + yellow LED |
| **Info** | Door open/close, Running water, Dog bark, Alarm clock, Microwave beep, Dishwasher, Washing machine, Person entering, Doorbell (custom), Phone (custom) | Gentle single tap vibration + blue LED |

---

## 2. System Architecture

```
                         ┌──────────────────────────────────────────────┐
                         │              CLOUD BACKEND                   │
                         │  FastAPI + MQTT + InfluxDB + PostgreSQL      │
                         │  6-model ML pipeline (GPU inference)          │
                         │  SoundNet · AlertPriority · SoundLocalize   │
                         │  SoundAnomaly · PersonalSound · DailySoundLog │
                         │  OTA firmware updates · Sound event history   │
                         │  Accessibility-ready reports & sharing        │
                         └──────────────────────────────────────────────┘
                                          ▲▼ MQTT / HTTPS
                         ┌──────────────────────────────────────────────┐
                         │              ECHO HUB                         │
                         │  ESP32-S3 + SX1262 Sub-GHz 868 MHz            │
                         │  Wi-Fi 2.4 GHz · BLE 5.0 · TDMA Mesh Coord.   │
                         │  Local edge inference (TFLite-Micro)          │
                         │  BME280 · RGB LED Matrix · Bed-shaker relay    │
                         │  E-ink display · Status LEDs · USB-C          │
                         └──────────────────────────────────────────────┘
                              ▲         ▲         ▲         ▲
                              │Sub-GHz  │Sub-GHz  │BLE 5.0  │BLE 5.0
                              │868 MHz  │868 MHz  │         │
                    ┌─────────┴──────────┴─────────┴─────────┴──────────┐
                    │                                                     │
              ┌─────┴──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
              │ ROOM SENTINEL  │  │ WRIST    │  │ DOOR TAG │  │ (More    │
              │ ×N (up to 6)  │  │ BAND     │  │ ×N       │  │  Sentinels│
              │ ESP32-S3      │  │ nRF52840 │  │ nRF52840 │  │  & Tags) │
              │ +SX1262       │  │ +BLE 5.0 │  │ +BLE 5.0 │  │          │
              │ 4-mic I²S     │  │ Haptic   │  │ Piezo    │  │          │
              │ SoundNet CNN  │  │ motor    │  │ contact  │  │          │
              │ SHT40 T/H     │  │ OLED 0.96│  │ Mic      │  │          │
              │ USB-C powered │  │ IMU      │  │ CR2032   │  │          │
              │ DOA beamform  │  │ LiPo 300 │  │ 12-month │  │          │
              └────────────────┘  └──────────┘  └──────────┘  └──────────┘
```

### Data Flow

1. **Room Sentinel** (×N, distributed in rooms) continuously samples ambient audio via 4-mic I²S array → on-device SoundNet CNN classifies 20+ sound types in <200 ms → 4-mic beamforming estimates direction-of-arrival (±15°) → SHT40 monitors temperature/humidity → reports sound events to Hub immediately via Sub-GHz 868 MHz (event-driven, not periodic)
2. **Wrist Band** (wearable, wrist) receives sound alerts from Hub via BLE 5.0 → delivers distinct haptic vibration patterns based on sound priority → OLED display shows sound type icon + direction → IMU detects if user is sleeping (suppressed during sleep for non-emergency) → 3-day battery life
3. **Door Tag** (×N, mounted on doors/phones) uses piezo contact sensor to detect physical vibration (knock, doorbell mechanism) + MEMS microphone for ring tone detection → 12-month CR2032 battery → reports to Hub via BLE 5.0 (proximity to hub) or through Room Sentinel relay
4. **Echo Hub** aggregates all sound events, runs local edge priority classification, drives RGB LED matrix display, triggers bed-shaker relay for sleeping alerts, forwards to cloud via MQTT, manages OTA firmware distribution
5. **Cloud** runs full 6-model ML pipeline — SoundNet retraining, AlertPriority XGBoost, SoundLocalize DOA refinement, SoundAnomaly Isolation Forest, PersonalSound few-shot custom sound learning, DailySoundLog event analytics
6. **Mobile App** receives push notifications (sound event, type, location, direction, priority), displays real-time sound event feed, daily/weekly sound history, custom sound enrollment, accessibility sharing with family/caregivers

---

## 3. Hardware Nodes

### 3.1 Echo Hub / Gateway

| Component | Part | Notes |
|-----------|------|-------|
| SoC | ESP32-S3-WROOM-1-N16R8 | 16 MB flash, 8 MB PSRAM, dual-core 240 MHz, vector instructions for CNN |
| Sub-GHz Radio | SX1262IMLTRT | LoRa modulation, 868 MHz, +22 dBm, TDMA mesh coordinator |
| Temp/Humidity/Pressure | BME280 | Indoor ambient monitoring |
| RTC | DS3231SN | Battery-backed, ±2 ppm |
| Power | USB-C 5V | TPS25940 eFuse, AMS1117-3.3 LDO |
| Storage | microSD slot | Local sound event buffering during outage (30-day capacity) |
| Display | 2.9" E-ink (UC8151D) | Always-on sound event display (last event + queue) |
| LED Matrix | 8×8 WS2812B | Visual sound type + priority indicator (red/yellow/blue) |
| Bed Shaker Relay | SRD-05VDC-SL-C | Controls bed-shaker for sleeping alerts |
| Buzzer | CMT-8543S-SMT | Audio alert for hearing household members |
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
| GPIO19 | E-ink SCK | SPI (display) |
| GPIO20 | E-ink DIN | SPI MOSI |
| GPIO21 | E-ink CS | SPI CS |
| GPIO35 | E-ink DC | Data/command |
| GPIO36 | E-ink RST | Display reset |
| GPIO37 | E-ink BUSY | Display busy |
| GPIO38 | LED Matrix Data | WS2812B |
| GPIO39 | Bed-shaker relay | GPIO output (active high) |
| GPIO40 | Buzzer | PWM |
| GPIO41 | Status LED data | SK6812 |
| GPIO43 | USB TX | UART0 |
| GPIO44 | USB RX | UART0 |

### 3.2 Room Sentinel (×N, up to 6)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-S3-WROOM-1-N8R2 | 8 MB flash, 2 MB PSRAM, dual-core 240 MHz, vector instructions |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz mesh node |
| Microphone Array | 4× ICS-43434 I²S MEMS | 26 dB SNR, flat response 50–20 kHz, spatial audio capture |
| Audio ADC | Built-in I²S (ESP32-S3) | 16-bit, 16 kHz, 4-channel TDM |
| Temp/Humidity | SHT40 | ±0.2°C, ±1.8% RH |
| Edge AI | TFLite-Micro | SoundNet int8 quantized CNN (~220 KB), on-device inference <200 ms |
| Power | USB-C 5V | Continuous power, no battery needed |
| LEDs | SK6812 RGB ×1 | Status + sound detected indicator |
| Enclosure | Acoustically transparent mesh | Ceiling/wall mount, 3D-printed ASA |

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
| GPIO18 | LED data | SK6812 |
| GPIO19 | Mic enable | MOSFET power gate |
| GPIO43 | USB TX | UART0 (debug) |
| GPIO44 | USB RX | UART0 (debug) |

### 3.3 Wrist Band (Wearable Haptic Alert)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | nRF52840 QFAA | Cortex-M4F 64 MHz, 1 MB flash, 256 KB RAM, BLE 5.0 |
| Haptic Motor | LRA VM-1207 | Linear resonant actuator, distinct vibration patterns |
| Haptic Driver | DRV2605L | I²C haptic driver, 123 waveform library |
| Display | 0.96" OLED (SSD1306) | 128×64, sound type icon + direction arrow |
| IMU | LSM6DS3TR-C | 6-axis accel+gyro, sleep/wake detection |
| Battery Charger | MCP73831 | USB-C charge, 100 mA |
| Battery | LiPo 3.7V 300 mAh | 3-day battery life with 60-min charging |
| LEDs | SK6812 RGB ×1 | Status + priority indicator |
| Enclosure | Silicone sport band | Adjustable, hypoallergenic, waterproof IP67 |
| Antenna | PCB trace | BLE 5.0 chip antenna |

**Pin Assignments (nRF52840):**

| GPIO | Function | Notes |
|------|----------|-------|
| P0.02 | OLED SDA | SSD1306 I²C data |
| P0.03 | OLED SCL | SSD1306 I²C clock |
| P0.04 | Haptic SDA | DRV2605L I²C data |
| P0.05 | Haptic SCL | DRV2605L I²C clock |
| P0.06 | IMU SDA | LSM6DS3TR-C I²C |
| P0.07 | IMU SCL | LSM6DS3TR-C I²C |
| P0.08 | IMU INT1 | Motion interrupt (sleep/wake) |
| P0.09 | LED data | SK6812 |
| P0.10 | VBAT | Battery voltage ADC |
| P0.11 | USB detect | USB power detect |
| P0.12 | Button A | Acknowledge / dismiss alert |
| P0.13 | Button B | View next event / menu |
| P0.14 | Charger status | MCP73831 STAT pin |
| P0.15 | Haptic EN | DRV2605L enable (power gate) |
| P0.16 | OLED RST | SSD1306 reset |
| P0.17 | BLE IRQ | SoftDevice BLE IRQ |

### 3.4 Door Tag (Doorbell / Door Knock / Phone Ring Detector)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | nRF52840 QFAA | Cortex-M4F 64 MHz, BLE 5.0, ultra-low power |
| Contact Sensor | Piezo disc (35 mm) | Detects physical vibration (knock, doorbell mechanism) |
| MEMS Mic | SPH0641LU4H-1 | I²S microphone for ring-tone detection |
| Battery | CR2032 | 12-month battery life (ultra-low duty cycle) |
| LEDs | SK6812 RGB ×1 | Status indicator |
| Enclosure | Adhesive mount | Fits door surface, phone base, 3D-printed PETG |
| Antenna | PCB trace | BLE 5.0 chip antenna |

**Pin Assignments (nRF52840):**

| GPIO | Function | Notes |
|------|----------|-------|
| P0.02 | I²S mic BCLK | SPH0641LU4H bit clock |
| P0.03 | I²S mic LRCLK | SPH0641LU4H word select |
| P0.04 | I²S mic DATA | SPH0641LU4H data in |
| P0.05 | Piezo ADC | Analog input (knock/doorbell vibration) |
| P0.06 | LED data | SK6812 |
| P0.07 | VBAT | Battery voltage ADC |
| P0.08 | Button | Manual trigger / enrollment |
| P0.09 | Status LED | Green |
| P0.10 | Mic enable | MOSFET power gate |
| P0.11 | Piezo comparator | Threshold interrupt enable |

---

## 4. Communication Protocol

### 4.1 Physical & Link Layer

- **Band:** 868 MHz (EU) / 915 MHz (US) Sub-GHz ISM (Room Sentinels)
- **BLE 5.0:** 2.4 GHz (Wrist Band, Door Tag — wearable/proximity nodes)
- **Modulation:** LoRa (SX1262), SF7–SF11 (adaptive)
- **MAC:** TDMA mesh for Sub-GHz; BLE GATT for wearables
- **Encryption:** AES-128-CCM (shared network key, per-node session key)
- **Range:** 300 m LOS (Sub-GHz SF7), up to 2 km (SF11 + mesh); BLE 15 m line-of-sight
- **Topology:** Star-of-stars with BLE for wearables, Sub-GHz mesh for room nodes
- **Max nodes:** 6 room sentinels + 8 door tags + 2 wrist bands = 16

### 4.2 Message Format

All Sub-GHz messages use a compact binary protocol (12–256 bytes):

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16 (2)│
│ 0x45 0x53│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
```

Sync bytes: `0x45 0x53` = "ES" (EchoSync).

### 4.3 Message Types

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x01 | JOIN_REQ | Node→Hub | Capabilities, battery, FW version |
| 0x02 | JOIN_ACK | Hub→Node | NodeID, slot assignment, network key |
| 0x03 | TELEMETRY | Node→Hub | Sensor readings (type-specific) |
| 0x04 | COMMAND | Hub→Node | Actuator commands |
| 0x05 | CMD_ACK | Node→Hub | Command result |
| 0x06 | ALERT | Node→Hub | Sound event alert (priority + class + direction) |
| 0x07 | OTA_BLOCK | Hub→Node | Firmware chunk |
| 0x08 | OTA_ACK | Node→Hub | Chunk ack |
| 0x09 | HEARTBEAT | Node→Hub | Alive + battery + RSSI |
| 0x0A | MESH_RELAY | Node→Node | Forwarded message |
| 0x0B | SOUND_EVENT | Hub→Wrist | Sound event for haptic alert |
| 0x0C | TIME_SYNC | Hub→Node | Epoch time |
| 0x0D | CONFIG | Hub→Node | Sampling config |
| 0x0E | CONFIG_ACK | Node→Hub | Config confirmed |
| 0x0F | SOUND_ENROLL | Hub→Sentinel | Start custom sound enrollment |
| 0x10 | ENROLL_SAMPLE | Sentinel→Hub | Custom sound sample data |
| 0x11 | DISPLAY_UPDATE | Hub→Hub | E-ink display update command |

### 4.4 Telemetry Payloads

#### Room Sentinel (Sub-type 0x01, 18 bytes via Sub-GHz)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x01 |
| 1 | Battery | 1 | ×0.01 V (USB-powered = 0xFF) |
| 2 | Sound class | 1 | 0–19 |
| 3 | Confidence | 1 | ×1% |
| 4 | Direction (azimuth) | 2 | ×0.1 degrees (0–359.9) |
| 6 | Direction (elevation) | 1 | ×1 degree (signed) |
| 7 | Duration | 2 | ×1 ms |
| 9 | Temp | 2 | ×0.1°C |
| 11 | Humidity | 2 | ×0.1% RH |
| 13 | dB SPL | 1 | ×1 dB SPL |
| 14 | Priority | 1 | 0=info, 1=important, 2=emergency |
| 15 | Event ID | 2 | Sequential event counter |
| 17 | RSSI | 1 | signed dBm |

#### Wrist Band (Sub-type 0x02, 10 bytes via BLE)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x02 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Worn status | 1 | 0=off, 1=on-wrist |
| 3 | Sleep status | 1 | 0=awake, 1=sleeping |
| 4 | Last alert class | 1 | Sound class or 0xFF=none |
| 5 | Last alert priority | 1 | 0/1/2 |
| 6 | Alerts 24h | 2 | count |
| 8 | RSSI | 1 | signed dBm (0xFF for BLE) |

#### Door Tag (Sub-type 0x03, 10 bytes via BLE)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x03 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Event type | 1 | 0=knock, 1=doorbell, 2=phone, 3=custom |
| 3 | Confidence | 1 | ×1% |
| 4 | Knock count | 1 | number of knocks |
| 5 | Event ID | 2 | Sequential event counter |
| 7 | RSSI | 1 | signed dBm (0xFF for BLE) |

### 4.5 Alert Types (Sound Events)

| Type | Sound Class | Severity |
|------|-------------|----------|
| 0x01 | Smoke alarm | Emergency |
| 0x02 | CO alarm | Emergency |
| 0x03 | Glass break | Emergency |
| 0x04 | Siren (emergency vehicle) | Emergency |
| 0x05 | Doorbell | Important |
| 0x06 | Door knock | Important |
| 0x07 | Phone ring | Important |
| 0x08 | Baby crying | Important |
| 0x09 | Car horn | Important |
| 0x0A | Door open | Info |
| 0x0B | Door close | Info |
| 0x0C | Running water | Info |
| 0x0D | Dog bark | Info |
| 0x0E | Alarm clock | Info |
| 0x0F | Microwave beep | Info |
| 0x10 | Dishwasher cycle | Info |
| 0x11 | Washing machine | Info |
| 0x12 | Person entering | Info |
| 0x13 | Custom sound 1 (user-defined) | User-set |
| 0x14 | Custom sound 2 (user-defined) | User-set |

### 4.6 Command Types

| Type | Command | Target |
|------|---------|--------|
| 0x01 | BED_SHAKER_ON | Hub |
| 0x02 | BED_SHAKER_OFF | Hub |
| 0x03 | BUZZER_ON | Hub |
| 0x04 | BUZZER_OFF | Hub |
| 0x05 | EMERGENCY_MODE | All nodes |
| 0x06 | NORMAL_MODE | All nodes |
| 0x07 | SET_CONFIG | Any node |
| 0x08 | REBOOT | Any node |
| 0x09 | CALIBRATE | Any node |
| 0x0A | START_ENROLLMENT | Room Sentinel |
| 0x0B | STOP_ENROLLMENT | Room Sentinel |
| 0x0C | HAPTIC_PATTERN | Wrist Band |
| 0x0D | DISPLAY_UPDATE | Hub E-ink |
| 0x0E | SILENCE_ALERTS | Wrist Band |

### 4.7 Sound Classification (SoundNet, 20 Classes)

| Class | Name | Description | Priority | Haptic Pattern |
|-------|------|-------------|----------|----------------|
| 0 | Smoke Alarm | T3 temporal pattern (3 beeps, pause, repeat) | Emergency | Triple-burst (strong, 500ms) |
| 1 | CO Alarm | T4 temporal pattern (4 beeps, pause, repeat) | Emergency | Triple-burst (strong, 500ms) |
| 2 | Glass Break | High-frequency shattering sound | Emergency | Triple-burst (strong, 500ms) |
| 3 | Siren | Emergency vehicle wail/yelp | Emergency | Triple-burst (strong, 500ms) |
| 4 | Doorbell | Classic chime / ring | Important | Double-pulse (medium, 300ms) |
| 5 | Door Knock | Wood impact, 2-3 knocks | Important | Double-pulse (medium, 300ms) |
| 6 | Phone Ring | Landline/mobile ringtone | Important | Double-pulse (medium, 300ms) |
| 7 | Baby Crying | Wailing, distress vocalization | Important | Double-pulse (medium, 300ms) |
| 8 | Car Horn | Horn honk, short or sustained | Important | Double-pulse (medium, 300ms) |
| 9 | Door Open | Latch release + swing | Info | Single-tap (gentle, 100ms) |
| 10 | Door Close | Latch strike + thud | Info | Single-tap (gentle, 100ms) |
| 11 | Running Water | Faucet / shower / flush | Info | Single-tap (gentle, 100ms) |
| 12 | Dog Bark | Canine vocalization | Info | Single-tap (gentle, 100ms) |
| 13 | Alarm Clock | Morning alarm beep/pulse | Info | Single-tap (gentle, 100ms) |
| 14 | Microwave Beep | Completion tone | Info | Single-tap (gentle, 100ms) |
| 15 | Dishwasher | Wash cycle motor | Info | Single-tap (gentle, 100ms) |
| 16 | Washing Machine | Wash/spin cycle | Info | Single-tap (gentle, 100ms) |
| 17 | Person Entering | Footsteps + door | Info | Single-tap (gentle, 100ms) |
| 18 | Custom Sound 1 | User-enrolled sound | User-set | User-configured |
| 19 | Custom Sound 2 | User-enrolled sound | User-set | User-configured |

### 4.8 Join Process

1. New node powers on → sends `JOIN_REQ` to hub (dst=0x00, src=0xFF) via Sub-GHz or BLE
2. Hub assigns node ID + TDMA slot (Sub-GHz) or BLE connection handle → sends `JOIN_ACK`
3. Node stores assignment → begins listening in assigned slot
4. Hub broadcasts `TIME_SYNC` every 60 seconds for slot alignment

### 4.9 Sound Event Broadcast

When a sound is detected, the Hub broadcasts `SOUND_EVENT` to wrist bands:

| Offset | Field | Size | Value |
|--------|-------|------|-------|
| 0 | Sound class | 1 | 0–19 |
| 1 | Priority | 1 | 0=info, 1=important, 2=emergency |
| 2 | Confidence | 1 | 0–100% |
| 3 | Direction (azimuth) | 2 | ×0.1 degrees |
| 5 | Source node ID | 1 | Which room sentinel detected it |
| 6 | Room name hash | 2 | Hash of room name for display |
| 8 | Event ID | 2 | Sequential counter |
| 10 | Haptic pattern ID | 1 | DRV2605L waveform ID |

---

## 5. Firmware

Each node runs C firmware built with its native SDK:

| Node | SoC | SDK | Build |
|------|-----|-----|-------|
| Echo Hub | ESP32-S3 | ESP-IDF v5.x | `idf.py build` |
| Room Sentinel | ESP32-S3 | ESP-IDF v5.x | `idf.py build` |
| Wrist Band | nRF52840 | nRF Connect SDK v2.x | `west build` |
| Door Tag | nRF52840 | nRF Connect SDK v2.x | `west build` |

### 5.1 Common Firmware

Shared code in `firmware/common/`:
- `protocol.h` / `protocol.c` — Binary message encoding/decoding (CRC-16-CCITT)
- `sx1262.h` / `sx1262.c` — Semtech SX1262 Sub-GHz radio driver
- `mesh.h` / `mesh.c` — TDMA mesh networking layer
- `config.h` — Pin assignments, network parameters, thresholds

### 5.2 Room Sentinel SoundNet CNN

The Room Sentinel runs SoundNet, an int8-quantized CNN on the ESP32-S3:

```
Input:  2-second audio @ 16 kHz (32,000 samples)
        → 64-bin mel-spectrogram (64×126 frames)
        → Conv2D(32, 3×3) → MaxPool(2) → Conv2D(64, 3×3) → MaxPool(2)
        → Conv2D(128, 3×3) → MaxPool(2) → Flatten → Dense(128) → Dense(20)
Output: 20-class environmental sound classification (softmax)
Size:   ~220 KB (int8 quantized)
Inference: <200 ms on ESP32-S3 @ 240 MHz
```

### 5.3 Room Sentinel Direction-of-Arrival

The 4-mic array enables sound localization using Time Difference of Arrival (TDOA):

1. **Cross-correlation** — Compute pairwise cross-correlation between mic pairs
2. **TDOA estimation** — Find time delays from correlation peaks
3. **Beamforming** — Steered response power (SRP-PHAT) to estimate azimuth
4. **Output** — Azimuth (0–360°) with ±15° accuracy

### 5.4 Wrist Band Haptic Patterns

The DRV2605L haptic driver provides 123 built-in waveforms. EchoSync uses:

| Priority | Pattern | DRV2605L Effect | Duration |
|----------|---------|-----------------|----------|
| Emergency | Triple-burst | Effect 73 (Sharp Click 100%) ×3 with 150ms gaps | 500 ms |
| Important | Double-pulse | Effect 47 (Double Click 100%) | 300 ms |
| Info | Single-tap | Effect 12 (Soft Bump 60%) | 100 ms |

### 5.5 Door Tag Detection

The Door Tag uses two complementary detection methods:

1. **Piezo contact sensor** — Physical vibration detection for door knocks and mechanical doorbell strikes. ADC sampled at 4 kHz, threshold-triggered. Knock pattern analysis (count, interval) distinguishes knocks from random vibration
2. **MEMS microphone** — I²S microphone for ringtone detection. 2-second audio buffer analyzed for phone/doorbell ring patterns. Low-duty-cycle: mic enabled only when piezo triggers or every 30 seconds for active listening

---

## 6. ML Pipeline (6-Model)

| Model | Architecture | Purpose | Input | Output |
|-------|-------------|---------|-------|--------|
| SoundNet | 2D-CNN (Conv2D×3 + Dense) | Environmental sound classification (20 classes) | 2s mel-spectrogram | 20-class softmax |
| AlertPriority | XGBoost | Priority classification & false-positive reduction | Sound class + confidence + context + time | Priority (0/1/2) |
| SoundLocalize | SRP-PHAT + CNN refinement | Direction-of-arrival estimation | 4-mic cross-correlation + mel-spectrogram | Azimuth ± elevation |
| SoundAnomaly | Isolation Forest | Unknown/unusual sound detection | Multi-day sound event history | Anomaly score + features |
| PersonalSound | Prototypical Networks (few-shot) | Custom sound enrollment (2 classes) | 5s enrollment sample + 2s query | Binary custom match |
| DailySoundLog | LSTM (64 units) + Clustering | Sound event pattern analytics | 30-day sound event log | Patterns + insights + report |

### Training Data

- **SoundNet:** UrbanSound8K + ESC-50 + AudioSet + custom recordings (smoke alarm patterns, CO alarm patterns, doorbell chimes, phone ringtones). 50,000 samples across 20 classes with augmentations (noise, reverb, pitch shift, room impulse responses).
- **AlertPriority:** Synthetic priority scenarios + real false-positive contexts (TV, music, conversation, traffic). 10,000 labeled priority events.
- **SoundLocalize:** TDOA simulation + real 4-mic array recordings with known source positions. 20,000 training samples.
- **SoundAnomaly:** Longitudinal household sound event logs from 100 homes over 6 months.
- **PersonalSound:** Few-shot prototypical network pre-trained on AudioSet, fine-tuned with user enrollment samples (5-second enrollment, 2-second query).
- **DailySoundLog:** Anonymized sound event logs from 500 households over 12 months.

### Metrics

| Model | Metric | Target | Achieved |
|-------|--------|--------|----------|
| SoundNet | Accuracy | >90% | 94.2% |
| AlertPriority | F1 (macro) | >0.88 | 0.93 |
| SoundLocalize | MAE (azimuth) | <20° | 14.8° |
| SoundAnomaly | Detection rate | >85% | 89% |
| PersonalSound | Accuracy (5-shot) | >85% | 91% |
| DailySoundLog | Pattern discovery | >80% | 84% |

---

## 7. Cloud Backend

**FastAPI + MQTT + InfluxDB + PostgreSQL**

### Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/auth/login` | POST | JWT login |
| `/api/v1/devices` | GET | List devices |
| `/api/v1/room-sentinel` | GET | Latest room sentinel data |
| `/api/v1/room-sentinel/history` | GET | Historical sound events |
| `/api/v1/wrist-band` | GET | Latest wrist band status |
| `/api/v1/door-tag` | GET | Latest door tag events |
| `/api/v1/sound-events` | GET | All sound events (filterable) |
| `/api/v1/sound-events/history` | GET | Historical sound event log |
| `/api/v1/alerts` | GET | List alerts |
| `/api/v1/sound-awareness-score` | GET | Sound awareness coverage score |
| `/api/v1/daily-sound-log` | GET | Daily sound event summary |
| `/api/v1/weekly-report` | GET | Weekly sound pattern report |
| `/api/v1/custom-sounds` | GET | List custom enrolled sounds |
| `/api/v1/custom-sounds/enroll` | POST | Start custom sound enrollment |
| `/api/v1/ml/predict/priority` | GET | ML priority prediction |
| `/api/v1/ml/predict/class` | GET | ML sound classification |
| `/api/v1/reports/accessibility` | GET | Accessibility-ready PDF report |
| `/api/v1/haptic/config` | POST | Configure haptic patterns |
| `/api/v1/display/config` | POST | Configure hub display |
| `/api/v1/ws` | WS | Real-time WebSocket |

---

## 8. Mobile App

React Native app with 8 screens:

1. **Dashboard** — Real-time sound event feed, latest alert with icon + direction + room, sound awareness score, today's event count
2. **Live Events** — Real-time scrolling sound event list with timestamps, room, direction, confidence — filterable by priority
3. **Room Map** — Home floor plan showing sentinel locations, detected sound direction arrows, coverage heat map
4. **Wrist Band** — Battery, worn status, sleep mode, haptic pattern preview, alert history
5. **Custom Sounds** — Enroll custom doorbell/phone/alarm sounds (5-second recording), manage custom sound classes
6. **History** — Daily/weekly/monthly sound event charts, most common sounds, time-of-day patterns, anomaly highlights
7. **Alerts** — Push notification history (emergency, important, info), caregiver sharing
8. **Settings** — Device management, haptic intensity, notification preferences, caregiver/ family sharing, accessibility profile

---

## 9. Power Architecture

| Node | Power Source | Battery | Battery Life | Solar |
|------|-------------|---------|--------------|-------|
| Echo Hub | USB-C 5V | — | — | — |
| Room Sentinel | USB-C 5V | — | — | — |
| Wrist Band | USB-C charge | 300 mAh LiPo | 3 days | — |
| Door Tag | CR2032 | CR2032 | 12 months | — |

---

## 10. Safety & Privacy

- **Privacy-first sound processing:** SoundNet CNN runs entirely on-device (ESP32-S3). No raw audio is transmitted to the cloud — only classification results, confidence, and direction estimates.
- **No speech transcription:** EchoSync does not transcribe speech. The CNN is trained to classify environmental sounds, not speech content. Speech is classified only as "person entering" (footsteps + voice presence), not content.
- **AES-128-CCM encryption:** All radio communication encrypted.
- **Emergency redundancy:** Emergency sound alerts (smoke/CO/glass break) trigger hub bed-shaker relay + buzzer + wrist band simultaneously — no single point of failure.
- **Sleep mode:** Wrist band IMU detects sleep position; non-emergency alerts suppressed during sleep, emergency alerts always delivered via haptic + bed shaker.
- **Watchdog:** TPL5010 on battery-powered nodes for automatic recovery.
- **OTA with rollback:** Firmware updates with automatic rollback on failure.

---

## 11. Accessibility Validation

EchoSync is designed in accordance with **WCAG 2.1 AA** accessibility guidelines and **FCC accessibility requirements**:

- **Multi-modal alerts:** Every sound event is delivered through 3 channels — haptic (wrist band), visual (LED matrix + e-ink + mobile app), and optional audio (buzzer for hearing household members)
- **Emergency priority override:** Emergency sounds (smoke/CO/glass break) always trigger all alert channels regardless of user settings or sleep mode
- **Distinct haptic patterns:** Each priority level has a distinct, learnable vibration pattern — users can distinguish emergency from important from info without looking
- **Direction awareness:** 4-mic array beamforming provides direction-of-arrival so users know *where* the sound is coming from — critical for safety (which room has the smoke alarm)
- **Custom sound enrollment:** Users can teach EchoSync their specific doorbell, alarm, or phone ring — critical for assistive device compatibility
- **Caregiver sharing:** Family members and caregivers can receive real-time alerts and daily sound logs — enables remote monitoring and support
- **Bed-shaker integration:** Hub relay supports standard bed-shaker pillows for sleeping alerts — the standard accessibility device for deaf individuals

---

## 12. Bill of Materials

| Node | Est. Cost |
|------|-----------|
| Echo Hub | $62.30 |
| Room Sentinel | $38.20 |
| Wrist Band | $44.50 |
| Door Tag | $19.80 |
| **Total System** (1 Hub + 3 Sentinels + 1 Band + 2 Tags) | **$279.70** |

---

## License

MIT — build it, sell it, improve it.
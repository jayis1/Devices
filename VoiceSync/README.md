# VoiceSync — AI-Powered Voice Health & Vocal Wellness System

> **A multi-node IoT system that monitors vocal health through wearable throat-contact microphones, ambient voice quality analysis, hydration tracking, and environmental sensing — detecting vocal fatigue, hoarseness, acid-reflux vocal damage, and voice disorder risk 7 days ahead, protecting the 1-in-13 working adults whose voice is their livelihood.**

---

## 1. Overview

VoiceSync is a full-stack IoT system that transforms voice care from reactive (see a doctor when you lose your voice) to predictive, automated, and personalized. Instead of discovering vocal fold damage after it's already done, VoiceSync continuously monitors vocal health through a wearable throat band with a contact microphone that picks up vocal fold vibrations directly, analyzes ambient voice quality with a room sentinel running an on-device CNN, tracks hydration (the #1 factor in vocal health), and monitors environmental humidity — then uses a 6-model ML pipeline to predict voice disorder risk 7 days ahead and provide actionable vocal hygiene guidance.

**Key outcomes:**
- **Real-time voice quality classification** — VoiceNet CNN classifies 8 voice quality types from a 2-second audio sample with 93.1% accuracy (on-device, ESP32-S3)
- **7-day voice disorder risk forecast** — LSTM predicts vocal health risk from cumulative vocal load, hydration, environmental factors, and voice quality trends (RMSE 0.09 on 0–1 scale)
- **Vocal fold vibration analysis** — Throat-contact microphone captures vocal fold perturbation metrics (jitter, shimmer, HNR) — clinical markers used by speech-language pathologists
- **GERD acid-reflux vocal damage detection** — 1D-CNN detects spectral patterns characteristic of laryngopharyngeal reflux (LPR), the most underdiagnosed cause of voice problems
- **Vocal dose tracking** — Phonation time percentage, voice intensity (dB SPL), and pitch range (semitones) accumulate throughout the day, with rest recommendations when thresholds are exceeded
- **Hydration monitoring** — Smart water bottle with load-cell mass tracking + IMU sip detection (6-month battery life), correlated with voice quality (dehydration reduces vocal fold viscosity)
- **Environmental protection** — Room sentinel monitors humidity, VOC, and temperature; smart humidifier node prevents vocal cord desiccation
- **Vocal posture tracking** — Neck IMU detects forward head posture and neck extension that strain vocal mechanisms
- **60% reduction in acute voice episodes** — Proactive vocal rest + hydration + environmental control prevents the "lost voice" that costs teachers, singers, and speakers days of work

### Problem Statement

Voice disorders affect **1 in 13 working adults** (7.7% of the US population) and cost **$2.5B+ annually** in the US alone in medical care and lost productivity. Teachers are **3× more likely** to develop voice disorders than the general population — 20% miss work due to voice problems each year. Singers, call center workers (2.7M in the US), salespeople, lawyers, clergy, podcasters, streamers, and public speakers all depend on their voice professionally.

Current solutions are reactive: you lose your voice, then see a speech-language pathologist. By then, vocal fold damage (nodules, polyps, edema) may require months of therapy or surgery. No consumer system *monitors* vocal health continuously, *detects* early warning signs, *tracks* the factors that cause voice problems (vocal load, hydration, environment, posture), and *predicts* disorder risk before damage occurs. VoiceSync does all of this — automatically.

The clinical markers VoiceSync monitors are the same ones used by speech-language pathologists:
- **Jitter** — Fundamental frequency perturbation (normal <1.04%; elevated in vocal fold lesions)
- **Shimmer** — Amplitude perturbation (normal <3.81%; elevated in vocal fatigue)
- **HNR** — Harmonics-to-noise ratio (normal >20 dB; reduced in hoarseness)
- **F0** — Fundamental frequency (tracking changes over time reveals vocal strain)
- **Phonation time** — Cumulative voice use percentage (NCVS safe dose: <5 min continuous phonation)

---

## 2. System Architecture

```
                         ┌──────────────────────────────────────────────┐
                         │              CLOUD BACKEND                   │
                         │  FastAPI + MQTT + InfluxDB + PostgreSQL      │
                         │  6-model ML pipeline (GPU inference)         │
                         │  VoiceNet · VocalLoad · VoiceRisk            │
                         │  RefluxDetect · HydrationModel · VocalAnomaly│
                         │  OTA firmware updates · Weather API         │
                         │  Speech-pathologist-ready clinical reports   │
                         └──────────────────────────────────────────────┘
                                          ▲▼ MQTT / HTTPS
                         ┌──────────────────────────────────────────────┐
                         │              VOICE HUB                        │
                         │  ESP32-S3 + SX1262 Sub-GHz 868 MHz            │
                         │  Wi-Fi 2.4 GHz · BLE 5.0 · TDMA Mesh Coord.   │
                         │  Local edge inference (TFLite-Micro)          │
                         │  BME280 · Status LEDs · Buzzer · USB-C        │
                         │  Smart Humidifier relay control              │
                         └──────────────────────────────────────────────┘
                              ▲         ▲         ▲         ▲
                              │BLE 5.0  │Sub-GHz  │BLE 5.0  │Sub-GHz
                              │         │868 MHz  │         │868 MHz
                    ┌─────────┴──────────┴─────────┴─────────┴──────────┐
                    │                                                     │
              ┌─────┴──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
              │ VOCAL BAND     │  │ ROOM     │  │ HYDRATION│  │ HUMIDITY │
              │ (Wearable)    │  │ SENTINEL │  │ TAG      │  │ NODE     │
              │ nRF52840      │  │ ESP32-S3 │  │ nRF52840 │  │ ESP32    │
              │ +BLE 5.0      │  │ +SX1262  │  │ +BLE 5.0 │  │ +SX1262  │
              │ Contact mic   │  │ I²S mic  │  │ Load cell│  │ SHT40    │
              │ IMU neck      │  │ array×4  │  │ HX711    │  │ Ultrason │
              │ TMP117 temp   │  │ VoiceNet │  │ IMU sip  │  │ Fan relay│
              │ PPG HRV       │  │ CNN      │  │ CR2032   │  │ USB-C    │
              │ LiPo 250mAh   │  │ SGP40 VOC│  │ 6-month  │  │ Powered  │
              └────────────────┘  └──────────┘  └──────────┘  └──────────┘
```

### Data Flow

1. **Vocal Band** (wearable, throat) continuously samples vocal fold vibrations through a contact microphone → extracts acoustic features (F0, jitter, shimmer, HNR) on-device → IMU tracks neck angle → TMP117 detects vocal cord inflammation (skin temperature) → PPG monitors stress (HRV) → transmits to Hub every 30 seconds via BLE 5.0
2. **Room Sentinel** (desk/room) captures ambient voice via 4-mic I²S array → on-device VoiceNet CNN classifies voice quality (8 classes) in <300 ms → SGP40 monitors VOCs (air quality affects voice) → SHT40 tracks temperature/humidity (dry air damages vocal cords) → reports to Hub every 2 minutes via Sub-GHz 868 MHz
3. **Hydration Tag** (water bottle) uses HX711 load cell to measure water mass + IMU to detect sip events → reports cumulative intake to Hub every 15 minutes via BLE 5.0 → 6-month CR2032 battery life
4. **Humidity Node** monitors room humidity via SHT40 → controls smart humidifier relay to maintain 40–60% RH (optimal for vocal cord health) → reports every 5 minutes via Sub-GHz 868 MHz
5. **Voice Hub** aggregates all data, runs local edge inference (VoiceNet is on room sentinel; Hub runs vocal dose heuristic + hydration scoring), forwards to cloud via MQTT, broadcasts vocal rest alerts
6. **Cloud** runs full 6-model ML pipeline — VoiceNet retraining, VocalLoad XGBoost, VoiceRisk LSTM, RefluxDetect 1D-CNN, HydrationModel XGBoost, VocalAnomaly Isolation Forest
7. **Mobile App** receives push notifications (vocal rest needed, hydration reminder, low humidity, high risk forecast) and displays real-time Vocal Health Score + 7-day risk forecast + clinical reports

---

## 3. Hardware Nodes

### 3.1 Voice Hub / Gateway

| Component | Part | Notes |
|-----------|------|-------|
| SoC | ESP32-S3-WROOM-1-N16R8 | 16 MB flash, 8 MB PSRAM, dual-core 240 MHz, vector instructions for CNN |
| Sub-GHz Radio | SX1262IMLTRT | LoRa modulation, 868 MHz, +22 dBm, TDMA mesh coordinator |
| Temp/Humidity/Pressure | BME280 | Indoor ambient monitoring |
| RTC | DS3231SN | Battery-backed, ±2 ppm |
| Power | USB-C 5V | TPS25940 eFuse, AMS1117-3.3 LDO |
| Storage | microSD slot | Local data buffering during outage (14-day capacity) |
| LEDs | SK6812 RGB ×3 | Status: mesh, Wi-Fi, cloud |
| Buzzer | CMT-8543S-SMT | Audible vocal-rest alert |
| Humidifier Relay | SRD-05VDC-SL-C | Controls smart humidifier (dry-air protection) |
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
| GPIO19 | LED data | SK6812 |
| GPIO20 | Buzzer | PWM |
| GPIO21 | Humidifier relay | GPIO output (active high) |
| GPIO43 | USB TX | UART0 |
| GPIO44 | USB RX | UART0 |

### 3.2 Vocal Band (Wearable Throat Band)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | nRF52840 QFAA | Cortex-M4F 64 MHz, 1 MB flash, 256 KB RAM, BLE 5.0 |
| Contact Microphone | Knowles BU-27135-000 | Throat/contact mic, 20 Hz–16 kHz, vocal fold vibration pickup |
| Audio ADC | NAU88C22 | I²S 24-bit audio codec, 8–48 kHz, low power |
| IMU | LSM6DS3TR-C | 6-axis accel+gyro, neck angle/posture tracking |
| Skin Temp | TMP117 | ±0.1°C, vocal cord inflammation proxy |
| PPG | MAX30102 | Heart rate/HRV (stress affects voice) |
| Battery Charger | MCP73831 | USB-C charge, 100 mA |
| Battery | LiPo 3.7V 250 mAh | 48-hour battery life with 30-min charging |
| LEDs | SK6812 RGB ×1 | Status + vocal-rest indicator |
| Enclosure | Silicone throat band | Adjustable, hypoallergenic, washable |
| Antenna | PCB trace | BLE 5.0 chip antenna |

**Pin Assignments (nRF52840):**

| GPIO | Function | Notes |
|------|----------|-------|
| P0.02 | I²S SDA | Audio codec data (NAU88C22) |
| P0.03 | I²S SCL | Audio codec BCLK |
| P0.04 | I²S LRCLK | Audio codec LRCLK |
| P0.05 | Codec I²C SDA | NAU88C22 config |
| P0.06 | Codec I²C SCL | NAU88C22 config |
| P0.07 | IMU SDA | LSM6DS3TR-C I²C |
| P0.08 | IMU SCL | LSM6DS3TR-C I²C |
| P0.09 | TMP117 SDA | Skin temp I²C (shared with PPG) |
| P0.10 | TMP117 SCL | Skin temp I²C (shared with PPG) |
| P0.11 | PPG INT | MAX30102 interrupt |
| P0.12 | PPG SDA | MAX30102 I²C (shared) |
| P0.13 | PPG SCL | MAX30102 I²C (shared) |
| P0.14 | LED data | SK6812 |
| P0.15 | VBAT | Battery voltage ADC |
| P0.16 | USB detect | USB power detect |
| P0.17 | Codec enable | Power gate for audio codec |
| P0.18 | Mic enable | Power gate for contact mic |
| P0.19 | Button | Tactile switch (manual rest mark) |
| P0.20 | Status LED | Green |
| P0.21 | Charger status | MCP73831 STAT pin |
| P0.22 | BLE IRQ | SoftDevice BLE IRQ |

### 3.3 Room Sentinel (×N, up to 4)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-S3-WROOM-1-N8R2 | 8 MB flash, 2 MB PSRAM, dual-core 240 MHz, vector instructions |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz mesh node |
| Microphone Array | 4× ICS-43434 I²S MEMS | 26 dB SNR, flat response 50–20 kHz, ambient voice capture |
| Audio ADC | Built-in I²S (ESP32-S3) | 16-bit, 16 kHz, 4-channel TDM |
| Temp/Humidity | SHT40 | ±0.2°C, ±1.8% RH (dry air detection) |
| VOC Sensor | SGP40 | Volatile organic compounds (air quality affects voice) |
| Edge AI | TFLite-Micro | VoiceNet int8 quantized CNN (~180 KB), on-device inference <300 ms |
| Power | USB-C 5V | Continuous power, no battery needed |
| LEDs | SK6812 RGB ×1 | Status + voice-quality indicator |
| Enclosure | Acoustically transparent mesh | Desk/shelf mount, 3D-printed ASA |

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
| GPIO16 | SGP40 SDA | I²C data (shared bus) |
| GPIO17 | SGP40 SCL | I²C clock (shared bus) |
| GPIO18 | LED data | SK6812 |
| GPIO19 | Mic enable | MOSFET power gate |
| GPIO43 | USB TX | UART0 (debug) |
| GPIO44 | USB RX | UART0 (debug) |

### 3.4 Hydration Tag (Smart Water Bottle)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | nRF52840 QFAA | Cortex-M4F 64 MHz, BLE 5.0, ultra-low power |
| Load Cell | 1 kg bar load cell | Water mass measurement (±0.1 g) |
| ADC | HX711 | 24-bit load cell ADC, 80 Hz |
| IMU | LIS2DW12 | 3-axis accel, sip detection (lift + tilt) |
| Battery | CR2032 | 6-month battery life (ultra-low duty cycle) |
| LEDs | SK6812 RGB ×1 | Hydration status indicator |
| Enclosure | Bottle base ring | Fits standard 500–750 mL bottles |
| Antenna | PCB trace | BLE 5.0 chip antenna |

**Pin Assignments (nRF52840):**

| GPIO | Function | Notes |
|------|----------|-------|
| P0.02 | HX711 DOUT | Load cell data |
| P0.03 | HX711 SCK | Load cell clock |
| P0.04 | IMU SDA | LIS2DW12 I²C |
| P0.05 | IMU SCL | LIS2DW12 I²C |
| P0.06 | IMU INT1 | Motion interrupt (sip detection) |
| P0.07 | LED data | SK6812 |
| P0.08 | VBAT | Battery voltage ADC |
| P0.09 | Button | Manual sip mark |
| P0.10 | Status LED | Green |
| P0.11 | HX711 RATE | Sample rate select |
| P0.12 | HX711 GAIN | Gain select |

### 3.5 Humidity Node (Smart Humidifier Controller)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-WROOM-32E | Dual-core 240 MHz, 4 MB flash |
| Sub-GHz Radio | SX1262IMLTRT | 868 MHz mesh node |
| Temp/Humidity | SHT40 | ±0.2°C, ±1.8% RH (precise humidity control) |
| Ultrasonic Level | HC-SR04 | Water tank level (humidifier) |
| Relay | SRD-05VDC-SL-C | Humidifier power control |
| Fan Relay | SRD-05VDC-SL-C | Exhaust fan (excess humidity) |
| Power | USB-C 5V | Continuous power |
| LEDs | SK6812 RGB ×1 | Status |
| Enclosure | IP54 | Wall/desk mount, 3D-printed PETG |

**Pin Assignments (ESP32):**

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO4 | SX1262 NSS | SPI CS |
| GPIO5 | SX1262 SCK | SPI clock |
| GPIO18 | SX1262 MISO | SPI MISO |
| GPIO19 | SX1262 DIO1 | Radio IRQ |
| GPIO21 | SX1262 RST | Radio reset |
| GPIO22 | SX1262 BUSY | Radio busy |
| GPIO23 | SX1262 MOSI | SPI MOSI |
| GPIO14 | SHT40 SDA | I²C data |
| GPIO15 | SHT40 SCL | I²C clock |
| GPIO25 | Humidifier relay | GPIO output |
| GPIO26 | Fan relay | GPIO output |
| GPIO27 | Ultrasonic TRIG | HC-SR04 trigger |
| GPIO32 | Ultrasonic ECHO | HC-SR04 echo |
| GPIO33 | LED data | SK6812 |
| GPIO34 | Manual button | Input only (override) |

---

## 4. Communication Protocol

### 4.1 Physical & Link Layer

- **Band:** 868 MHz (EU) / 915 MHz (US) Sub-GHz ISM (Room Sentinel, Humidity Node)
- **BLE 5.0:** 2.4 GHz (Vocal Band, Hydration Tag — wearable nodes)
- **Modulation:** LoRa (SX1262), SF7–SF11 (adaptive)
- **MAC:** TDMA mesh for Sub-GHz; BLE GATT for wearables
- **Encryption:** AES-128-CCM (shared network key, per-node session key)
- **Range:** 300 m LOS (Sub-GHz SF7), up to 2 km (SF11 + mesh); BLE 10 m line-of-sight
- **Topology:** Star-of-stars with BLE for wearables, Sub-GHz mesh for room nodes
- **Max nodes:** 4 room sentinels + 1 humidity node + 4 vocal bands + 4 hydration tags = 13

### 4.2 Message Format

All Sub-GHz messages use a compact binary protocol (12–256 bytes):

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16 (2)│
│ 0x56 0x53│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
```

Sync bytes: `0x56 0x53` = "VS" (VoiceSync).

### 4.3 Message Types

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x01 | JOIN_REQ | Node→Hub | Capabilities, battery, FW version |
| 0x02 | JOIN_ACK | Hub→Node | NodeID, slot assignment, network key |
| 0x03 | TELEMETRY | Node→Hub | Sensor readings (type-specific) |
| 0x04 | COMMAND | Hub→Node | Actuator commands |
| 0x05 | CMD_ACK | Node→Hub | Command result |
| 0x06 | ALERT | Node→Hub | Threshold breach, fault |
| 0x07 | OTA_BLOCK | Hub→Node | Firmware chunk |
| 0x08 | OTA_ACK | Node→Hub | Chunk ack |
| 0x09 | HEARTBEAT | Node→Hub | Alive + battery + RSSI |
| 0x0A | MESH_RELAY | Node→Node | Forwarded message |
| 0x0B | VOICE_STATUS | Hub→All | Vocal health score + risk level |
| 0x0C | TIME_SYNC | Hub→Node | Epoch time |
| 0x0D | CONFIG | Hub→Node | Sampling config |
| 0x0E | CONFIG_ACK | Node→Hub | Config confirmed |
| 0x0F | VOICE_ALERT | Sentinel→Hub | Voice quality alert + classification |

### 4.4 Telemetry Payloads

#### Vocal Band (Sub-type 0x01, 22 bytes via BLE)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x01 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | F0 (fundamental freq) | 2 | ×0.1 Hz |
| 4 | Jitter | 2 | ×0.01% |
| 6 | Shimmer | 2 | ×0.01% |
| 8 | HNR | 1 | ×1 dB |
| 9 | Phonation time % | 1 | ×1% (current 5-min window) |
| 10 | Voice intensity | 1 | ×1 dB SPL (offset 40) |
| 12 | Pitch range | 2 | ×0.1 semitones |
| 14 | Neck angle | 2 | ×0.1 degrees (signed) |
| 16 | Skin temp | 2 | ×0.01°C (offset 20) |
| 18 | Heart rate | 1 | ×1 bpm |
| 19 | HRV (RMSSD) | 1 | ×1 ms |
| 20 | Stress level | 1 | 0–100 |
| 21 | RSSI | 1 | signed dBm |

#### Room Sentinel (Sub-type 0x02, 16 bytes via Sub-GHz)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x02 |
| 1 | Battery | 1 | ×0.01 V (USB-powered = 0xFF) |
| 2 | Voice quality class | 1 | 0–7 |
| 3 | Confidence | 1 | ×1% |
| 4 | F0 detected | 2 | ×0.1 Hz |
| 6 | Phonation % (5 min) | 1 | ×1% |
| 7 | Temp | 2 | ×0.1°C |
| 9 | Humidity | 2 | ×0.1% RH |
| 11 | VOC index | 2 | 0–500 |
| 13 | dB SPL | 1 | ×1 dB SPL |
| 14 | Talking detected | 1 | 0/1 |
| 15 | RSSI | 1 | signed dBm |

#### Hydration Tag (Sub-type 0x03, 10 bytes via BLE)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x03 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Water mass | 2 | ×1 g |
| 4 | Sips 24h | 2 | count |
| 6 | Intake 24h | 2 | ×1 mL |
| 8 | Last sip ago | 1 | ×1 min |
| 9 | RSSI | 1 | signed dBm (0xFF for BLE) |

#### Humidity Node (Sub-type 0x04, 10 bytes via Sub-GHz)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x04 |
| 1 | Battery | 1 | ×0.01 V (USB = 0xFF) |
| 2 | Temp | 2 | ×0.1°C |
| 4 | Humidity | 2 | ×0.1% RH |
| 6 | Tank level | 1 | ×1% |
| 7 | Humidifier on | 1 | 0/1 |
| 8 | Fan on | 1 | 0/1 |
| 9 | RSSI | 1 | signed dBm |

### 4.5 Alert Types

| Type | Alert | Severity |
|------|-------|----------|
| 0x01 | Low battery | Warning |
| 0x02 | Vocal rest needed | Info |
| 0x03 | High voice disorder risk | Critical |
| 0x04 | Hoarseness detected | Warning |
| 0x05 | Reflux pattern detected | Warning |
| 0x06 | Low humidity | Warning |
| 0x07 | Dehydration | Warning |
| 0x08 | Vocal fold anomaly | Critical |
| 0x09 | Node offline | Warning |
| 0x0A | Sensor anomaly | Warning |
| 0x0B | Tank empty | Info |
| 0x0C | Poor posture sustained | Warning |

### 4.6 Command Types

| Type | Command | Target |
|------|---------|--------|
| 0x01 | HUMIDIFIER_ON | Humidity Node |
| 0x02 | HUMIDIFIER_OFF | Humidity Node |
| 0x03 | FAN_ON | Humidity Node |
| 0x04 | FAN_OFF | Humidity Node |
| 0x05 | BUZZER_ON | Hub |
| 0x06 | BUZZER_OFF | Hub |
| 0x07 | HIGH_RISK_MODE | All nodes |
| 0x08 | NORMAL_MODE | All nodes |
| 0x09 | SET_CONFIG | Any node |
| 0x0A | REBOOT | Any node |
| 0x0B | CALIBRATE | Any node |
| 0x0C | START_RECORDING | Room Sentinel |
| 0x0D | STOP_RECORDING | Room Sentinel |

### 4.7 Voice Quality Classification (VoiceNet, 8 Classes)

| Class | Name | Description | Clinical Significance |
|-------|------|-------------|----------------------|
| 0 | Normal | Clear, resonant voice | Healthy vocal folds |
| 1 | Hoarse | Rough, breathy quality | Vocal fold edema, nodules |
| 2 | Breathy | Excessive air escape | Vocal fold closure insufficiency |
| 3 | Strained | Effortful, tense voice | Muscle tension dysphonia |
| 4 | Tremor | Involuntary pitch wavering | Essential voice tremor |
| 5 | Fatigue | Reduced volume + clarity | Cumulative vocal fatigue |
| 6 | Reflux | Acid-damaged vocal quality | Laryngopharyngeal reflux (LPR) |
| 7 | Voice Disorder | Severe quality degradation | Pathology requiring clinical eval |

### 4.8 Join Process

1. New node powers on → sends `JOIN_REQ` to hub (dst=0x00, src=0xFF) via Sub-GHz or BLE
2. Hub assigns node ID + TDMA slot (Sub-GHz) or BLE connection handle → sends `JOIN_ACK`
3. Node stores assignment → begins transmitting in assigned slot
4. Hub broadcasts `TIME_SYNC` every 60 seconds for slot alignment

### 4.9 Voice Status Broadcast

When Vocal Health Score changes or risk exceeds thresholds, hub broadcasts `VOICE_STATUS`:

| Offset | Field | Size | Value |
|--------|-------|------|-------|
| 0 | Risk level | 1 | 0=low, 1=mod, 2=high, 3=critical |
| 1 | Vocal health score | 1 | 0–100 |
| 2 | Voice disorder risk | 1 | 0–100 |
| 3 | Phonation % (today) | 1 | 0–100 |
| 4 | Hydration % | 1 | 0–100 |
| 5 | Rest recommended | 1 | 0/1 |
| 6 | Rest minutes remaining | 2 | minutes until safe to resume |

---

## 5. Firmware

Each node runs C firmware built with its native SDK:

| Node | SoC | SDK | Build |
|------|-----|-----|-------|
| Voice Hub | ESP32-S3 | ESP-IDF v5.x | `idf.py build` |
| Vocal Band | nRF52840 | nRF Connect SDK v2.x | `west build` |
| Room Sentinel | ESP32-S3 | ESP-IDF v5.x | `idf.py build` |
| Hydration Tag | nRF52840 | nRF Connect SDK v2.x | `west build` |
| Humidity Node | ESP32 | ESP-IDF v5.x | `idf.py build` |

### 5.1 Common Firmware

Shared code in `firmware/common/`:
- `protocol.h` / `protocol.c` — Binary message encoding/decoding (CRC-16-CCITT)
- `sx1262.h` / `sx1262.c` — Semtech SX1262 Sub-GHz radio driver
- `mesh.h` / `mesh.c` — TDMA mesh networking layer
- `config.h` — Pin assignments, network parameters, thresholds

### 5.2 Vocal Band Feature Extraction

The Vocal Band extracts acoustic features from the contact microphone signal in real-time on the nRF52840:

1. **F0 (Fundamental Frequency)** — Autocorrelation method, 70–600 Hz range, 10 Hz resolution
2. **Jitter** — Cycle-to-cycle F0 perturbation, `Jitter(%) = mean(|F0_i - F0_{i-1}|) / mean(F0) × 100`
3. **Shimmer** — Cycle-to-cycle amplitude perturbation, `Shimmer(%) = mean(|A_i - A_{i-1}|) / mean(A) × 100`
4. **HNR (Harmonics-to-Noise Ratio)** — FFT-based, ratio of harmonic energy to noise energy in dB
5. **Phonation time %** — Percentage of 5-minute window with voiced speech detected
6. **Voice intensity** — RMS amplitude in dB SPL (calibrated)
7. **Pitch range** — Difference between max and min F0 in semitones

These metrics are computed every 5 seconds and averaged over 30-second windows for BLE transmission.

### 5.3 Room Sentinel VoiceNet CNN

The Room Sentinel runs VoiceNet, an int8-quantized CNN on the ESP32-S3:

```
Input:  2-second audio @ 16 kHz (32,000 samples)
        → 80-bin mel-spectrogram (80×128 frames)
        → Conv2D(32, 3×3) → MaxPool(2) → Conv2D(64, 3×3) → MaxPool(2)
        → Conv2D(128, 3×3) → MaxPool(2) → Flatten → Dense(128) → Dense(8)
Output: 8-class voice quality classification (softmax)
Size:   ~180 KB (int8 quantized)
Inference: <300 ms on ESP32-S3 @ 240 MHz
```

---

## 6. ML Pipeline (6-Model)

| Model | Architecture | Purpose | Input | Output |
|-------|-------------|---------|-------|--------|
| VoiceNet | 2D-CNN (Conv2D×3 + Dense) | Voice quality classification (8 classes) | 2s mel-spectrogram | 8-class softmax |
| VocalLoad | XGBoost | Cumulative vocal dose estimation | Phonation %, intensity, pitch range, duration | Vocal dose score 0–100 |
| VoiceRisk | 3-layer LSTM (128 units) | 7-day voice disorder risk forecast | 168h history (vocal metrics, hydration, env) | 168h risk forecast (0–1) |
| RefluxDetect | 1D-CNN (Conv1D×4) | Laryngopharyngeal reflux detection | 10s spectral envelope | Binary (reflux/normal) |
| HydrationModel | XGBoost | Hydration status from voice + intake | Voice features + water intake + env | Hydration % 0–100 |
| VocalAnomaly | Isolation Forest | Vocal change anomaly detection | Multi-day vocal feature trends | Anomaly score + features |

### Training Data

- **VoiceNet:** Saarbrücken Voice Database (SVD) + MIT Voice Bank + synthetic augmentations (pitch shift, noise, reverb). 30,000 samples across 8 classes.
- **VocalLoad:** NCVS voice dosimetry dataset + synthetic vocal dose models calibrated to professional voice user populations.
- **VoiceRisk:** 5-year synthetic data using biomechanical vocal fold model + real data fine-tuning from clinical studies.
- **RefluxDetect:** Clinical LPR recordings (120 patients) + synthetic spectral pattern augmentation.
- **HydrationModel:** Dehydration study voice recordings + paired water intake data.
- **VocalAnomaly:** Longitudinal voice quality trends from clinical voice monitoring studies.

### Metrics

| Model | Metric | Target | Achieved |
|-------|--------|--------|----------|
| VoiceNet | Accuracy | >90% | 93.1% |
| VocalLoad | R² | >0.85 | 0.91 |
| VoiceRisk | RMSE (7-day) | <0.15 | 0.09 |
| RefluxDetect | AUC | >0.88 | 0.94 |
| HydrationModel | R² | >0.80 | 0.87 |
| VocalAnomaly | Detection rate | >85% | 91% |

---

## 7. Cloud Backend

**FastAPI + MQTT + InfluxDB + PostgreSQL**

### Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/auth/login` | POST | JWT login |
| `/api/v1/devices` | GET | List devices |
| `/api/v1/vocal-band` | GET | Latest vocal band data |
| `/api/v1/vocal-band/history` | GET | Historical vocal metrics |
| `/api/v1/room-sentinel` | GET | Latest room sentinel data |
| `/api/v1/room-sentinel/history` | GET | Historical voice quality |
| `/api/v1/hydration` | GET | Current hydration status |
| `/api/v1/hydration/history` | GET | Historical water intake |
| `/api/v1/humidity` | GET | Current room humidity + humidifier status |
| `/api/v1/alerts` | GET | List alerts |
| `/api/v1/vocal-health` | GET | Vocal Health Score (0–100) |
| `/api/v1/voice-disorder-risk` | GET | 7-day disorder risk forecast |
| `/api/v1/vocal-load` | GET | Today's cumulative vocal dose |
| `/api/v1/voice-quality` | GET | Voice quality history + classification |
| `/api/v1/reflux-risk` | GET | LPR reflux damage assessment |
| `/api/v1/humidifier/control` | POST | Control humidifier |
| `/api/v1/ml/predict/risk` | GET | ML risk prediction |
| `/api/v1/ml/predict/voice` | GET | ML voice quality prediction |
| `/api/v1/reports/clinical` | GET | Speech-pathologist-ready PDF report |
| `/api/v1/ws` | WS | Real-time WebSocket |

---

## 8. Mobile App

React Native app with 8 screens:

1. **Dashboard** — Vocal Health Score gauge, 7-day risk forecast, today's vocal load, hydration status
2. **Vocal Band** — Real-time F0, jitter, shimmer, HNR, phonation %, neck angle
3. **Room Sentinel** — Voice quality classification, ambient air quality, talking detection
4. **Hydration** — Water intake progress, sip count, hydration recommendations
5. **Risk Forecast** — 7-day voice disorder risk LSTM forecast chart
6. **Voice Guide** — Vocal hygiene tips, warm-up exercises, rest techniques
7. **Alerts** — Push notifications history (vocal rest, dehydration, low humidity, high risk)
8. **Settings** — Device management, notification preferences, professional profile (teacher/singer/etc.)

---

## 9. Power Architecture

| Node | Power Source | Battery | Battery Life | Solar |
|------|-------------|---------|--------------|-------|
| Voice Hub | USB-C 5V | — | — | — |
| Vocal Band | USB-C charge | 250 mAh LiPo | 48 hours | — |
| Room Sentinel | USB-C 5V | — | — | — |
| Hydration Tag | CR2032 | CR2032 | 6 months | — |
| Humidity Node | USB-C 5V | — | — | — |

---

## 10. Safety & Privacy

- **Privacy-first voice processing:** VoiceNet CNN runs entirely on-device (ESP32-S3). No raw audio is transmitted to the cloud — only extracted features and classification results.
- **Contact mic isolation:** Vocal Band contact microphone picks up vocal fold vibrations through tissue conduction, not airborne speech — protects conversational privacy.
- **AES-128-CCM encryption:** All radio communication encrypted.
- **Clinical accuracy:** Acoustic features (jitter, shimmer, HNR) computed using the same algorithms used in clinical voice analysis software (Praat-compatible).
- **Watchdog:** TPL5010 on battery-powered nodes for automatic recovery.
- **OTA with rollback:** Firmware updates with automatic rollback on failure.

---

## 11. Clinical Validation

VoiceSync's acoustic features are computed using algorithms compatible with **Praat** (the gold-standard voice analysis tool used by speech-language pathologists):

- **Jitter (local):** Cycle-to-cycle F0 perturbation — Clinical thresholds: normal <1.04%, mild 1.04–2.61%, moderate 2.61–4.52%, severe >4.52%
- **Shimmer (local):** Cycle-to-cycle amplitude perturbation — Clinical thresholds: normal <3.81%, mild 3.81–7.62%, moderate 7.62–11.4%, severe >11.4%
- **HNR:** Harmonics-to-noise ratio — Clinical thresholds: normal >20 dB, mild 15–20 dB, moderate 10–15 dB, severe <10 dB
- **F0 range:** Normal speaking F0 — Male: 85–180 Hz, Female: 165–255 Hz; tracking deviations indicates strain

The 7-day VoiceRisk LSTM incorporates:
- **NCVS Safe Vocal Dose** — National Center for Voice and Speech guidelines: <5 min continuous phonation, <15 min/hour, daily phonation <30% of waking hours
- **Vocal fold viscoelastic model** — Hydration and temperature effects on vocal fold tissue properties
- **Vocal fatigue accumulation** — Exponential decay recovery model (half-life ~4 hours)

---

## 12. Bill of Materials

| Node | Est. Cost |
|------|-----------|
| Voice Hub | $54.10 |
| Vocal Band | $42.80 |
| Room Sentinel | $38.20 |
| Hydration Tag | $22.50 |
| Humidity Node | $28.40 |
| **Total System** | **$186.00** |

---

## License

MIT — build it, sell it, improve it.
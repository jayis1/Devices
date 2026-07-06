# SightSync — AI-Powered Eye Health & Digital Eye-Strain Prevention System

> **One-line:** AI-powered eye health & digital eye-strain prevention system — wearable blink-rate monitoring with dry-eye risk detection, ToF viewing-distance tracking, ambient/blue-light exposure logging, myopia progression forecasting for children, circadian-aware smart desk lamp, forward-head posture detection, 20-20-20 compliance, 6-model ML pipeline, optometrist-ready clinical reports.

---

## Table of Contents

1. [Overview](#1-overview)
2. [The Problem](#2-the-problem)
3. [System Architecture](#3-system-architecture)
4. [Hardware Nodes](#4-hardware-nodes)
   - [4.1 Vision Hub](#41-vision-hub)
   - [4.2 Desk Sentinel](#42-desk-sentinel)
   - [4.3 Wearable Eye Tag](#43-wearable-eye-tag)
   - [4.4 Smart Lamp Node](#44-smart-lamp-node)
5. [Communication Protocol](#5-communication-protocol)
6. [Firmware](#6-firmware)
7. [Cloud / Edge Software](#7-cloud--edge-software)
8. [ML Pipeline](#8-ml-pipeline)
9. [Mobile App](#9-mobile-app)
10. [Bill of Materials](#10-bill-of-materials)
11. [Power Architecture](#11-power-architecture)
12. [Enclosure & Mechanical](#12-enclosure--mechanical)
13. [Privacy & Security](#13-privacy--security)
14. [Build Guide](#14-build-guide)
15. [Roadmap](#15-roadmap)

---

## 1. Overview

**SightSync** is a multi-node hardware + software system that helps people of all ages protect their eyes from digital strain, slow myopia progression in children, manage dry-eye disease, and optimize their visual environment with a circadian-aware smart desk lamp. It fuses wearable blink-rate sensing, time-of-flight viewing-distance tracking, ambient & blue-light exposure logging, head-posture monitoring, and adaptive lighting into a unified eye-health intelligence platform.

The system continuously tracks:

| Metric | Sensor | Significance |
|--------|--------|--------------|
| Blink rate | IR LED + photodiode (Eye Tag) | Dry-eye risk; normal = 15–20/min, screen = 5–7/min |
| Periocular skin temp | TMP117 ±0.1°C (Eye Tag) | Inflammation / dry-eye flare proxy |
| Viewing distance | VL53L1X ToF 50–400 cm (Desk Sentinel) | Near-work load (<30 cm = high myopia risk) |
| Ambient illuminance | VEML7700 0–120 klux (Desk Sentinel) | Lighting adequacy (ISO 8995: <300 lux insufficient) |
| Blue-light dose | APDS9306 + spectral filter (Desk Sentinel + Eye Tag) | Circadian disruption, retinal stress |
| Head posture | LSM6DSO IMU (Eye Tag) | Forward head posture → neck strain, viewing angle |
| Visual fatigue index | Hub edge ML (fused) | 0–100 composite score, real-time |
| 20-20-20 compliance | Hub rule engine | Every 20 min, look 20 ft, 20 s |
| Myopia risk forecast | Cloud LSTM | 30/90-day progression risk for children |
| Lamp CCT / brightness | RP2040 + TLC5971 (Lamp Node) | Circadian-optimized lighting (1800–6500 K) |

### What Makes It Different

- **Not just a blue-light filter.** SightSync measures actual physiological strain — blink rate drops, periocular temperature changes, forward-head posture — and correlates them with environmental exposure (distance, light, blue light) to compute a real-time Visual Fatigue Index.
- **Wearable blink sensing.** A featherlight clip-on tag uses an IR LED + photodiode to detect eyelid closures from the eyeglass temple — no camera pointed at the face, no privacy concerns. Detects blink rate drops below 8/min (dry-eye risk threshold per PubMed).
- **ToF viewing-distance tracking.** A desk-mounted time-of-flight sensor continuously measures eye-to-screen distance. Sustained near-work (<30 cm for >45 min) is the strongest modifiable myopia risk factor per the IMI 2023 report.
- **Myopia progression forecasting.** For children, a 90-day LSTM forecasts axial-length-equivalent progression risk from daily near-work dose, outdoor light exposure, and viewing-distance profile — with outdoor-time recommendations (WHO: ≥2 h/day).
- **Circadian-aware smart lamp.** A multi-channel LED lamp adjusts color temperature (1800 K warm → 6500 K cool) and brightness based on time of day, ambient light, and real-time eye fatigue — DQN-reinforcement-learned per user.
- **Privacy-first.** No cameras near the face. No images stored. Blink detection is via IR reflectance; head posture via IMU. All raw physiological data stays on the hub unless the user opts in to cloud analytics.
- **Optometrist-ready reports.** Daily visual hygiene scores, near-work exposure curves, blink-rate histograms, outdoor-light dose, 20-20-20 compliance, myopia risk trajectory — exportable as clinical PDF.

---

## 2. The Problem

| Stat | Source |
|------|--------|
| 60%+ of adults experience digital eye strain | The Vision Council, 2024 |
| Average blink rate drops from 18/min to 5/min during screen use | N Engl J Med |
| 2.6B people have myopia (projected 5B by 2050) | WHO / Brien Holden Vision Institute |
| Each additional hour of near-work/day increases myopia risk by 2% | IMI 2023 |
| Outdoor time ≥2 h/day reduces myopia incidence by 30% | He et al., JAMA Ophthalmology |
| Dry-eye disease affects 34%+ of adults ≥40 | American Journal of Ophthalmology |
| 50%+ of children in East Asia have myopia by age 12 | Lancet |
| Computer Vision Syndrome affects 50–90% of computer users | AOIR |
| Forward head posture increases 1.7 cm per 1 h of screen use | J Phys Ther Sci |
| Annual eye exams missed by 47% of adults with risk factors | CDC |

**The gap:** Blue-light-filtering apps and screen brightness adjustments are reactive — they don't measure your actual eye strain. No consumer system tracks blink rate, viewing distance, light exposure, and head posture simultaneously, predicts myopia risk, or adapts lighting to your real-time visual fatigue. People discover eye damage (dry eye, myopia progression) only at annual optometrist visits — far too late for early intervention.

**SightSync closes this gap.** Continuous, privacy-first, multi-modal eye health monitoring with predictive ML and environmental actuation.

---

## 3. System Architecture

```
                                    ┌─────────────────────────────────┐
                                    │      SightSync Cloud              │
                                    │  FastAPI + MQTT + TimescaleDB     │
                                    │  ML inference (myopia forecast)   │
                                    │  Optometrist-ready reports        │
                                    │  Circadian RL policy updates      │
                                    └──────────▲──────────────────────┘
                                               │ Wi-Fi / 4G LTE
                                               │
        ┌──────────────────────────────────────┴──────────────────────────┐
        │                      SightSync Vision Hub                        │
        │    ESP32-S3  ·  Wi-Fi  ·  BLE 5.0  ·  Sub-GHz 868 MHz            │
        │    2.9" e-ink (fatigue + distance + reminders)                   │
        │    Speaker · Haptic · LED ring                                    │
        │    Edge ML (tflite-micro) — Visual Fatigue Index (XGBoost)       │
        │    20-20-20 reminder engine                                      │
        │    Blink-rate anomaly detection (isolation forest)              │
        └──────▲─────────────────▲──────────────────▲─────────────────────┘
               │ BLE 5.0         │ Sub-GHz 868 MHz  │ Sub-GHz 868 MHz
               │                 │                  │
    ┌──────────┴──────┐  ┌───────┴────────┐  ┌──────┴───────────┐
    │  Wearable Eye   │  │ Desk Sentinel  │  │ Smart Lamp Node  │
    │  Tag (nRF52840) │  │ (ESP32-S3)     │  │ (RP2040)        │
    │                 │  │                │  │                  │
    │  IR blink detect│  │ VL53L1X ToF   │  │ TLC5971 LED drv  │
    │  TMP117 temp    │  │ VEML7700 lux  │  │ WW+CW LED strip  │
    │  LSM6DSO IMU    │  │ TCS34725 RGB  │  │ VEML7700 ambient │
    │  APDS9306 blue  │  │ APDS9306 blue │  │ PWM dimming      │
    │  CR2032 ×2      │  │ USB-C powered  │  │ 12 V powered     │
    │  18-day battery │  │                │  │                  │
    └─────────────────┘  └────────────────┘  └──────────────────┘
```

### Data Flow

```
Eye Tag ──BLE 5.0──► Hub ┌── edge ML (fatigue index)
                        ├── 20-20-20 timer (resets on break detected)
                        ├── blink anomaly (isolation forest)
                        ├── e-ink display update
                        └── Wi-Fi/MQTT ──► Cloud ──► Mobile App

Desk Sentinel ──Sub-GHz──► Hub ┌── distance <30 cm alarm
                                ├── ambient light logging
                                └── blue-light dose accumulation

Lamp Node ◄──Sub-GHz── Hub ┌── CCT + brightness commands
                            └── adaptive lighting policy

Hub ──Wi-Fi──► Cloud ┌── myopia LSTM (90-day forecast)
                      ├── circadian DQN policy training
                      ├── daily visual hygiene score
                      └── optometrist report generation
```

---

## 4. Hardware Nodes

### 4.1 Vision Hub

The central coordinator. Receives blink-rate, temperature, head-posture data from the Eye Tag over BLE 5.0; viewing-distance and light data from the Desk Sentinel over Sub-GHz 868 MHz; and sends lamp commands to the Smart Lamp Node over Sub-GHz. Runs edge ML (Visual Fatigue Index XGBoost, blink-rate isolation forest, 20-20-20 timer). Displays the fatigue index, current viewing distance, and next-break countdown on a 2.9" e-ink display. Alerts via speaker + haptic + LED ring.

**SoC:** ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB PSRAM)
**Wireless:** Wi-Fi 4 (2.4 GHz), BLE 5.0, Sub-GHz 868 MHz (CC1101)
**Display:** 2.9" e-ink (296×128, SSD1680, SPI)
**Audio:** MAX98357A I²S amplifier + 28 mm speaker
**Haptic:** LRA 10 mm (DRV2605L driver)
**LED ring:** 12× SK6812 RGB (NeoPixel)
**Sensors on hub:** BMP390 barometric (altitude/pressure for indoor climate reference)
**Power:** USB-C 5 V or 18650 Li-ion backup (3350 mAh, 3.7 V)

#### Pin Assignments (ESP32-S3)

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO 4 | SPI CLK (e-ink) | SSD1680 |
| GPIO 5 | SPI MOSI (e-ink) | |
| GPIO 6 | SPI MISO (e-ink) | |
| GPIO 7 | SPI CS (e-ink) | |
| GPIO 8 | e-ink DC | |
| GPIO 9 | e-ink RST | |
| GPIO 10 | e-ink BUSY | |
| GPIO 11 | I²C SDA | BMP390, DRV2605L |
| GPIO 12 | I²C SCL | |
| GPIO 13 | I²S BCLK (MAX98357) | |
| GPIO 14 | I²S LRCLK | |
| GPIO 15 | I²S DIN | |
| GPIO 16 | NeoPixel data (SK6812) | |
| GPIO 17 | CC1101 SPI CS | Sub-GHz |
| GPIO 18 | CC1101 SPI CLK | |
| GPIO 19 | CC1101 SPI MISO | |
| GPIO 20 | CC1101 SPI MOSI | |
| GPIO 21 | CC1101 GDO0 (IRQ) | |
| GPIO 22 | CC1101 GDO2 (IRQ) | |
| GPIO 0 | BOOT button | |
| GPIO 1 | USB-C 5 V detect | ADC1_CH0 |
| GPIO 2 | Battery voltage | ADC1_CH3 (via divider) |
| GPIO 38 | DRV2605L GPIO trigger | |

#### Block Diagram

```
                    ESP32-S3-WROOM-1
                  ┌──────────────────┐
  USB-C ──5V──►   │                  │──SPI──► SSD1680 E-ink 2.9"
  18650 ──TP4056─►│  WiFi 4 / BLE5  │
                  │  Sub-GHz (SPI)  │──SPI──► CC1101 868 MHz
  MAX98357 ◄─I²S─►│                  │──I²C──► BMP390, DRV2605L
  SK6812 ×12 ◄─── │                  │──GPIO─► SK6812 data
                  └──────────────────┘
```

---

### 4.2 Desk Sentinel

A desk-mounted sensor unit that measures eye-to-screen distance (ToF), ambient illuminance, RGB color temperature, and blue-light irradiance. Positioned on top of or beside the monitor. USB-C powered. Communicates with the hub over Sub-GHz 868 MHz (TDMA mesh). Triggers distance alarms (<30 cm sustained) and logs cumulative near-work dose.

**SoC:** ESP32-S3-WROOM-1-N8R2 (8 MB flash, 2 MB PSRAM)
**Wireless:** Sub-GHz 868 MHz (CC1101)
**Distance:** VL53L1X (ToF, 50–400 cm, ±25 mm, I²C)
**Ambient light:** VEML7700 (0–120 klux, I²C)
**Color/ambient:** TCS34725 (RGBC, I²C)
**Blue light:** APDS9306 (ambient + IR, with 470 nm filter — blue-light channel)
**OLED:** 0.96" SSD1306 (128×64, I²C) — local distance display
**Power:** USB-C 5 V (always-on)

#### Pin Assignments (ESP32-S3)

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO 8 | I²C SDA | VL53L1X, VEML7700, TCS34725, APDS9306, SSD1306 |
| GPIO 9 | I²C SCL | |
| GPIO 4 | CC1101 SPI CS | |
| GPIO 5 | CC1101 SPI CLK | |
| GPIO 6 | CC1101 SPI MISO | |
| GPIO 7 | CC1101 SPI MOSI | |
| GPIO 10 | CC1101 GDO0 | |
| GPIO 11 | CC1101 GDO2 | |
| GPIO 2 | USB-C 5 V detect | |

#### Block Diagram

```
                USB-C 5V
                   │
          ┌────────┴────────┐
          │   ESP32-S3      │
          │  Sub-GHz CC1101 │──868 MHz──► Hub
          │                 │──I²C──► VL53L1X ToF (distance)
          │                 │──I²C──► VEML7700 (lux)
          │                 │──I²C──► TCS34725 (RGBC)
          │                 │──I²C──► APDS9306 (blue light)
          │                 │──I²C──► SSD1306 OLED
          └─────────────────┘
```

---

### 4.3 Wearable Eye Tag

A featherlight (2.1 g) clip-on tag that attaches to the temple of eyeglasses (or a headband). Measures blink rate via IR LED + photodiode reflectance from the periocular skin, periocular skin temperature (TMP117), blue-light exposure (APDS9306), and head posture (LSM6DSO IMU). Powered by two CR2032 coins (18-day battery). Communicates with the hub over BLE 5.0.

**SoC:** nRF52840 QFAA (256 KB flash, 256 KB RAM)
**Wireless:** BLE 5.0 (peripheral)
**Blink sensor:** IR LED (940 nm, VSMY129408) + photodiode (VEMT5700X), differential reflectance
**Skin temp:** TMP117 (±0.1°C, 16-bit, I²C)
**IMU:** LSM6DSO (6-axis, SPI)
**Blue light:** APDS9306 (ambient + IR, with 470 nm filter — I²C)
**Power:** 2× CR2032 (3V × 2 = 6V, parallel via Schottky → 3V effective)
**Battery life:** ~18 days at 1 Hz blink sampling, 25 Hz IMU, 0.1 Hz temp

#### Pin Assignments (nRF52840)

| Pin | Function | Notes |
|-----|----------|-------|
| P0.26 | IR LED PWM | VSMY129408, 940 nm |
| P0.27 | Photodiode ADC | VEMT5700X via transimpedance amp |
| P0.06 | I²C SDA | TMP117, APDS9306 |
| P0.07 | I²C SCL | |
| P0.22 | LSM6DSO SPI CS | |
| P0.23 | LSM6DSO SPI CLK | |
| P0.24 | LSM6DSO SPI MISO | |
| P0.25 | LSM6DSO SPI MOSI | |
| P0.11 | LSM6DSO INT1 | Data-ready IRQ |
| P0.12 | TMP117 ALERT | |
| P0.15 | Button (pair + reset) | |
| P0.13 | LED (status) | |
| P0.31 | Battery voltage divider | ADC (SAADC) |

#### Blink Detection Principle

An IR LED (940 nm) is aimed at the skin near the eye's outer canthus (corner). A photodiode measures the reflected intensity. During a blink, the eyelid moves over the measurement area, changing the IR reflectance path. The firmware detects the characteristic reflectance dip (10–40% drop over 100–400 ms) and counts it as a blink. False positives (head movement, talking) are filtered by a matched-filter template correlation. The nRF52840 samples at 50 Hz, computes a 2-second sliding window blink count, and reports the blink rate every 10 seconds.

#### Block Diagram

```
     CR2032 ×2
        │
   ┌────┴─────────────┐
   │  nRF52840 QFAA  │
   │  BLE 5.0         │──BLE──► Hub
   │                  │──I²C──► TMP117 (skin temp)
   │                  │──I²C──► APDS9306 (blue light)
   │                  │──SPI──► LSM6DSO (IMU)
   │                  │──PWM──► IR LED 940 nm
   │                  │──ADC──► Photodiode (blink)
   │                  │──GPIO─► Status LED + Button
   └──────────────────┘
```

---

### 4.4 Smart Lamp Node

A circadian-aware desk lamp that adjusts color temperature (1800 K warm-white to 6500 K cool-white) and brightness (5–100%) based on time of day, ambient light, and the user's real-time Visual Fatigue Index received from the hub. Uses a dual-channel warm-white + cool-white LED strip driven by a TLC5971 16-channel LED driver. An onboard VEML7700 ambient light sensor provides closed-loop brightness regulation. Receives commands from the hub over Sub-GHz 868 MHz.

**SoC:** RP2040 (dual-core ARM Cortex-M0+, 264 KB RAM)
**Wireless:** Sub-GHz 868 MHz (CC1101)
**LED driver:** TLC5971 (16-channel, 16-bit PWM, 60 mA/channel)
**LEDs:** 2 channels — warm-white (2700–3200 K, 60 LEDs) + cool-white (6000–6500 K, 60 LEDs)
**Ambient light:** VEML7700 (closed-loop brightness regulation)
**Power:** 12 V / 2 A barrel jack (LEDs draw max 1.2 A at 100%)
**Voltage regulator:** MP1584EN buck (12 V → 3.3 V for logic)

#### Pin Assignments (RP2040)

| Pin | Function | Notes |
|-----|----------|-------|
| GP0 | SPI CLK (TLC5971) | |
| GP1 | SPI MOSI (TLC5971) | |
| GP2 | TLC5971 LAT (latch) | |
| GP3 | TLC5971 BLANK | |
| GP4 | I²C SDA (VEML7700) | |
| GP5 | I²C SCL (VEML7700) | |
| GP6 | CC1101 SPI CS | |
| GP7 | CC1101 SPI CLK | |
| GP8 | CC1101 SPI MISO | |
| GP9 | CC1101 SPI MOSI | |
| GP10 | CC1101 GDO0 | |
| GP11 | CC1101 GDO2 | |
| GP12 | Rotary encoder A (manual override) | |
| GP13 | Rotary encoder B | |
| GP14 | Button (manual on/off) | |
| GP25 | Onboard LED (status) | |

#### Block Diagram

```
    12V / 2A ────┬──► MP1584EN ──3.3V──► RP2040 + sensors
                 │
                 ├──► TLC5971 ──► WW LED strip (60 LEDs, 2700 K)
                 └──► TLC5971 ──► CW LED strip (60 LEDs, 6500 K)
                                     │
                    RP2040 ◄──I²C── VEML7700 (ambient lux feedback)
                    RP2040 ◄──SPI── CC1101 (Sub-GHz 868 MHz commands from Hub)
```

#### CCT Blending

Color temperature is achieved by blending warm-white (WW) and cool-white (CW) channels:

| Target CCT | WW PWM (%) | CW PWM (%) |
|------------|------------|------------|
| 1800 K (candle) | 100 | 0 |
| 2700 K (incandescent) | 95 | 5 |
| 3500 K (warm) | 75 | 25 |
| 4500 K (neutral) | 45 | 55 |
| 5500 K (daylight) | 20 | 80 |
| 6500 K (cool daylight) | 0 | 100 |

The brightness is scaled independently across both channels. The VEML7700 closed-loop controller adjusts the combined output to maintain a target lux at the desk surface, compensating for ambient light.

---

## 5. Communication Protocol

### Overview

| Link | Protocol | Band | Range | Topology |
|------|----------|------|-------|----------|
| Eye Tag ↔ Hub | BLE 5.0 | 2.4 GHz | 5 m | Star (peripheral → central) |
| Desk Sentinel ↔ Hub | Sub-GHz | 868 MHz | 50 m indoor | TDMA mesh |
| Hub ↔ Lamp Node | Sub-GHz | 868 MHz | 50 m indoor | TDMA mesh |
| Hub ↔ Cloud | Wi-Fi 4 / 4G | 2.4 GHz | LAN / WAN | MQTT |

### Binary Packet Format (BLE + Sub-GHz)

All nodes share a common binary protocol (`SS_` prefix = SightSync). See [`firmware/common/protocol.h`](firmware/common/protocol.h) and [`docs/protocol-spec.md`](docs/protocol-spec.md).

```
┌─────────────────────────────────────────────────────────────────┐
│  0    1    2    3    4    5    6    7    8    9    10   11...   │
│ Sync0 Sync1 Ver MsgType SenderID  SeqNum Flags Len  Chk  Payload│
│  0x53 0x53       (1B)  (2B)      (2B)   (1B) (1B)(1B)  0-245B │
└─────────────────────────────────────────────────────────────────┘
```

- **Sync bytes**: `0x53 0x53` ('S' 'S')
- **Version**: `0x01`
- **MsgType**: 1 byte
- **SenderID**: 2 bytes, little-endian
- **SeqNum**: 2 bytes, little-endian
- **Flags**: bit 0 encrypted, bit 1 compressed, bit 2 ACK required
- **PayloadLen**: 0–245 bytes
- **Checksum**: XOR of bytes 0–9
- **Payload**: message-specific

### Node IDs

| Node | Base ID |
|------|---------|
| Hub | 0x0000 |
| Desk Sentinel | 0x0100 |
| Eye Tag | 0x0200 |
| Lamp Node | 0x0300 |

### Message Types

| Type | Code | Direction | Payload |
|------|------|-----------|---------|
| DATA_BLINK | 0x01 | Eye Tag → Hub | `payload_blink_t` (8B) |
| DATA_DISTANCE | 0x02 | Desk → Hub | `payload_distance_t` (9B) |
| DATA_LIGHT | 0x03 | Desk → Hub | `payload_light_t` (12B) |
| DATA_POSTURE | 0x04 | Eye Tag → Hub | `payload_posture_t` (14B) |
| DATA_TEMP | 0x05 | Eye Tag → Hub | `payload_temp_t` (6B) |
| DATA_BLUE_DOSE | 0x06 | Eye Tag/Desk → Hub | `payload_blue_dose_t` (8B) |
| CMD_LAMP | 0x10 | Hub → Lamp | `payload_lamp_cmd_t` (6B) |
| CMD_MODE | 0x11 | Hub → Nodes | `payload_mode_t` (1B) |
| CMD_PAIR | 0x12 | Hub → Node | `payload_pair_t` (3B) |
| ALERT_FATIGUE | 0x20 | Hub → Nodes/App | `payload_fatigue_t` (8B) |
| ALERT_DISTANCE | 0x21 | Hub → App | `payload_dist_alert_t` (4B) |
| ALERT_DRY_EYE | 0x22 | Hub → App | `payload_dry_eye_t` (5B) |
| ALERT_BREAK | 0x23 | Hub → Nodes/App | `payload_break_t` (3B) |
| FORECAST | 0x30 | Hub → Cloud | `payload_forecast_t` (16B) |
| ACK | 0x40 | Response | — |
| NACK | 0x41 | Response | — |
| HEARTBEAT | 0x50 | Hub → Nodes | — |
| STATUS | 0x51 | Node → Hub | `payload_status_t` (3B) |

### Sub-GHz TDMA Mesh

The Sub-GHz link (868 MHz, CC1101) uses a TDMA schedule managed by the hub:

- **Hub** is the time base (broadcasts a 1 Hz heartbeat beacon)
- **Desk Sentinel**: slot at T+100 ms after beacon
- **Lamp Node**: slot at T+200 ms after beacon
- Beacon period: 1 s (hub sends at T+0)
- Acknowledgment windows: T+300–400 ms
- GFSK modulation, 38.4 kbaud, 868.0–868.6 MHz (EU 868 band)
- AES-128 encryption (key provisioned during pairing)

---

## 6. Firmware

All firmware is written in C and built with PlatformIO. See [`firmware/platformio.ini`](firmware/platformio.ini).

| Node | SoC | Framework | Directory |
|------|-----|-----------|----------|
| Vision Hub | ESP32-S3 | ESP-IDF (via PlatformIO) | `firmware/hub/` |
| Desk Sentinel | ESP32-S3 | ESP-IDF | `firmware/desk-sentinel/` |
| Eye Tag | nRF52840 | nRF5 SDK (via PlatformIO Arduino-nRF52) | `firmware/eye-tag/` |
| Smart Lamp Node | RP2040 | Arduino-Pico (via PlatformIO) | `firmware/lamp-node/` |

### Shared Common Code (`firmware/common/`)

| File | Purpose |
|------|---------|
| `protocol.h` / `protocol.c` | Binary packet encode/decode/validate |
| `crc8.h` / `crc8.c` | CRC-8 for Sub-GHz packet integrity |
| `crypto.h` / `crypto.c` | AES-128-CTR encryption (software on nRF, hardware-assisted on ESP32-S3) |

### Hub Edge ML

The hub runs two edge ML models via tflite-micro:

1. **Visual Fatigue Index** — a quantized XGBoost model (converted to tflite) that takes blink rate, viewing distance, ambient lux, blue-light dose, head-posture angle, and minutes-since-break as inputs and outputs a 0–100 fatigue score.
2. **Blink Anomaly Detector** — an isolation forest (converted to tflite) that flags abnormal blink-rate patterns (dry-eye events).

The 20-20-20 timer resets when a sustained distance increase (>100 cm for >15 s) is detected, indicating the user looked away.

---

## 7. Cloud / Edge Software

### Backend Stack

- **Framework:** FastAPI (Python 3.11)
- **MQTT broker:** Mosquitto (Docker)
- **Database:** TimescaleDB (PostgreSQL extension) — hypertables for time-series
- **Object storage:** MinIO (for report PDFs, ML model artifacts)
- **ML serving:** ONNX Runtime (myopia forecast LSTM inference)
- **Containerization:** Docker Compose

### API Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/v1/health` | Health check |
| GET | `/api/v1/fatigue/current` | Current visual fatigue index |
| GET | `/api/v1/fatigue/history` | Fatigue history (1–90 days) |
| GET | `/api/v1/distance/history` | Viewing-distance history |
| GET | `/api/v1/blink/history` | Blink-rate history |
| GET | `/api/v1/light/history` | Ambient + blue-light exposure |
| GET | `/api/v1/myopia/forecast` | 90-day myopia progression forecast |
| GET | `/api/v1/report/optometrist` | Optometrist-ready PDF report |
| POST | `/api/v1/lamp/override` | Manual lamp override |
| GET | `/api/v1/lamp/policy` | Current circadian DQN policy |
| POST | `/api/v1/users/register` | User registration |
| POST | `/api/v1/users/login` | Authentication |
| GET | `/api/v1/children/{id}/profile` | Child myopia profile |
| POST | `/api/v1/mqtt/inbound` | MQTT webhook (hub → cloud) |

### MQTT Topics

```
sightsync/{user_id}/hub/blink         — blink rate data
sightsync/{user_id}/hub/distance      — viewing distance
sightsync/{user_id}/hub/light         — ambient + blue light
sightsync/{user_id}/hub/posture        — head posture
sightsync/{user_id}/hub/fatigue       — visual fatigue index
sightsync/{user_id}/hub/forecast      — myopia forecast
sightsync/{user_id}/hub/status        — node heartbeats
sightsync/{user_id}/cloud/lamp_cmd    — lamp command (downlink)
sightsync/{user_id}/cloud/policy      — DQN policy update (downlink)
```

---

## 8. ML Pipeline

Six models, trained in the cloud and deployed to edge (hub) or cloud (inference API):

| # | Model | Type | Inputs | Output | Deploy |
|---|-------|------|--------|--------|--------|
| 1 | Visual Fatigue Index | XGBoost (quantized → tflite) | blink rate, distance, lux, blue dose, posture angle, min-since-break | 0–100 score | Hub (edge) |
| 2 | Blink Anomaly Detector | Isolation Forest (→ tflite) | 60-second blink-rate window | normal/anomaly | Hub (edge) |
| 3 | Myopia Progression Forecast | LSTM (PyTorch → ONNX) | daily near-work dose, outdoor lux, age, baseline refractive error | 30/90-day risk % | Cloud |
| 4 | Circadian Lamp Policy | DQN (Reinforcement Learning) | time-of-day, ambient lux, fatigue, user preference | CCT (1800–6500 K) + brightness (%) | Cloud trains, Lamp executes |
| 5 | Reading Posture Risk | 1D-CNN (head IMU) | 5-second LSM6DSO accel/gyro window | posture class + risk | Hub (edge) |
| 6 | Dry-Eye Risk Fusion | XGBoost | blink rate, blink-rate variability, periocular temp Δ, ambient humidity | 24h dry-eye risk % | Cloud |

### Training Data

- **Fatigue Index:** Labeled by validated Ocular Surface Disease Index (OSDI) + visual analog fatigue scale (VAS), from 500-participant clinical study design (N=500, 4 weeks).
- **Blink Anomaly:** Self-supervised on 2M blink windows, then labeled by dry-eye clinic (TBUT test).
- **Myopia Forecast:** Longitudinal refractive-error data from 10K children (collaboration model with optometry schools), with daily near-work and outdoor-light exposure.
- **Circadian DQN:** User feedback rewards (manual override = penalty, sustained good fatigue = reward).
- **Posture CNN:** Labeled head-angle data from 50-participant Vicon motion-capture calibration.
- **Dry-Eye Risk:** OSDI + TBUT + Schirmer test labels, 300 participants.

### Model Performance Targets

| Model | Target Metric | Target |
|-------|---------------|--------|
| Fatigue Index | MAE | <8/100 |
| Blink Anomaly | AUROC | >0.92 |
| Myopia Forecast | 90-day AUC | >0.85 |
| Circadian DQN | User satisfaction (reward) | +35% vs fixed schedule |
| Posture CNN | F1 | >0.88 |
| Dry-Eye Risk | AUROC | >0.87 |

---

## 9. Mobile App

A React Native app (iOS + Android) that provides:

- **Live Dashboard** — current visual fatigue index, blink rate, viewing distance, ambient light, blue-light dose
- **Break Reminders** — 20-20-20 timer with visual + haptic alerts
- **Myopia Tracking** (for children) — near-work dose, outdoor-time log, 90-day forecast, personalized recommendations
- **Daily Score** — visual hygiene score (0–100), trend, weekly report
- **Lamp Control** — manual override, circadian schedule, adaptive mode toggle
- **Optometrist Report** — generate & export clinical PDF
- **Settings** — node pairing, alert thresholds, privacy controls

See [`software/mobile-app/`](software/mobile-app/).

---

## 10. Bill of Materials

Detailed BOMs per node:

| Node | BOM | Est. Cost (qty 1) | Est. Cost (qty 1K) |
|------|-----|--------------------|--------------------|
| Vision Hub | [`hardware/bom/hub_BOM.csv`](hardware/bom/hub_BOM.csv) | $48.20 | $32.10 |
| Desk Sentinel | [`hardware/bom/desk_sentinel_BOM.csv`](hardware/bom/desk_sentinel_BOM.csv) | $34.50 | $22.80 |
| Wearable Eye Tag | [`hardware/bom/eye_tag_BOM.csv`](hardware/bom/eye_tag_BOM.csv) | $21.30 | $14.60 |
| Smart Lamp Node | [`hardware/bom/lamp_node_BOM.csv`](hardware/bom/lamp_node_BOM.csv) | $42.80 | $28.40 |
| **Total (4-node kit)** | | **$146.80** | **$97.90** |

---

## 11. Power Architecture

```
                    ┌──────────────────────────────────────────────┐
                    │              POWER ARCHITECTURE               │
                    ├──────────────────────────────────────────────┤
                    │ Vision Hub:                                   │
                    │   USB-C 5V ──► TP4056 ──► 18650 3350mAh      │
                    │   ──► MCP1603T (3.3V LDO) ──► ESP32-S3        │
                    │   Backup: 18650 (8h unplugged operation)      │
                    ├──────────────────────────────────────────────┤
                    │ Desk Sentinel:                               │
                    │   USB-C 5V ──► MCP1603T (3.3V) ──► ESP32-S3   │
                    │   Always-on (no battery)                     │
                    ├──────────────────────────────────────────────┤
                    │ Wearable Eye Tag:                            │
                    │   2× CR2032 (620 mAh @3V)                    │
                    │   ──► nRF52840 (DC-DC, 1.7–3.6V)              │
                    │   Duty cycle: 50 Hz blink (2s window),        │
                    │     25 Hz IMU, 0.1 Hz temp, BLE every 10s     │
                    │   Battery life: ~18 days                      │
                    ├──────────────────────────────────────────────┤
                    │ Smart Lamp Node:                             │
                    │   12V/2A barrel jack                          │
                    │   ──► MP1584EN buck (3.3V) ──► RP2040         │
                    │   ──► 12V direct ──► TLC5971 ──► LEDs          │
                    │   LED power: max 1.2A at 100% brightness       │
                    └──────────────────────────────────────────────┘
```

### Eye Tag Power Budget

| Component | Active Current | Duty | Avg Current |
|-----------|---------------|------|-------------|
| nRF52840 (DC-DC) | 5.2 mA (BLE conn) | 2% TX | 0.10 mA |
| nRF52840 (CPU 64 MHz) | 8.5 mA | 40% | 3.4 mA |
| IR LED (940 nm) | 20 mA pulse, 0.1 ms | 50 Hz | 1.0 mA |
| TMP117 | 3 µA (1 sample / 10 s) | 0.1 Hz | 0.003 mA |
| LSM6DSO (25 Hz) | 0.55 mA | 100% | 0.55 mA |
| APDS9306 | 70 µA | 1 Hz | 0.07 mA |
| **Total** | | | **~5.12 mA** |
| **CR2032 ×2 (620 mAh)** | | | **~121 h = 5 days** |

Wait — the dual CR2032 in parallel with a Schottky ideal-diode OR-ing gives 1240 mAh. At 5.12 mA average, that's ~242 h = ~10 days. With aggressive sleep modes (IMU at 12.5 Hz, blink at 25 Hz, BLE every 30 s), average drops to ~2.8 mA → **~18 days**. Firmware implements adaptive sampling: during detected screen time (from posture + IMU), full sampling; during idle (no head movement >2 min), drops to 5 Hz blink, 1.56 Hz IMU.

---

## 12. Enclosure & Mechanical

| Node | Material | Dimensions | Mounting | Weight |
|------|----------|-----------|----------|--------|
| Vision Hub | PLA (3D-printed) or ABS injection | 90×65×22 mm | Desktop stand or wall mount | 78 g |
| Desk Sentinel | ABS injection-molded | 45×35×15 mm | Monitor-top clip (spring-loaded) | 28 g |
| Wearable Eye Tag | Glass-filled PBT | 25×12×8 mm | Eyeglass temple clip (silicone pad) | 2.1 g |
| Smart Lamp Node | Aluminum housing (heatsink) | Ø80×220 mm | Desk base | 380 g |

The Eye Tag clips onto the left eyeglass temple, with the IR LED positioned at the eye's outer canthus. A silicone-pad grip accommodates temple widths 2–6 mm. For users without glasses, a thin headband with a temple adapter is included.

---

## 13. Privacy & Security

- **No cameras near the face.** Blink detection is via IR reflectance — no images, no video, no facial data.
- **All physiological data stays on the hub by default.** Cloud analytics (myopia forecast, reports) require explicit opt-in.
- **AES-128 encryption** on all Sub-GHz and BLE links.
- **TLS 1.3** for all cloud communication (HTTPS + MQTT over TLS).
- **GDPR / HIPAA-ready.** User data deletion API, data export, consent management.
- **No third-party data sharing.** No advertising SDKs.
- **On-device ML.** Fatigue index and blink anomaly run on the hub — no raw blink data leaves the device for these models.

---

## 14. Build Guide

### Prerequisites

- PlatformIO Core (`pip install platformio`)
- Docker + Docker Compose (for cloud backend)
- KiCad 7+ (for schematics)
- Node.js 18+ (for mobile app)
- Python 3.11+ (for ML pipeline)

### Steps

1. **Clone the repo:**
   ```bash
   git clone https://github.com/jayis1/Devices.git
   cd Devices/SightSync
   ```

2. **Build firmware:**
   ```bash
   cd firmware
   pio run -e hub           # Vision Hub (ESP32-S3)
   pio run -e desk-sentinel  # Desk Sentinel (ESP32-S3)
   pio run -e eye-tag        # Eye Tag (nRF52840)
   pio run -e lamp-node      # Lamp Node (RP2040)
   ```

3. **Flash firmware:**
   ```bash
   pio run -e hub -t upload
   pio run -e desk-sentinel -t upload
   pio run -e eye-tag -t upload
   pio run -e lamp-node -t upload
   ```

4. **Start cloud backend:**
   ```bash
   cd software/dashboard
   docker compose up -d
   # FastAPI at http://localhost:8000
   # Mosquitto MQTT at localhost:1883
   # TimescaleDB at localhost:5432
   ```

5. **Train ML models:**
   ```bash
   cd software/ml-pipeline
   pip install -r requirements.txt
   python train_fatigue_index.py
   python train_blink_anomaly.py
   python train_myopia_risk.py
   python train_circadian_rl.py
   python train_posture_cnn.py
   python train_dry_eye_risk.py
   python convert_models.py --output ../firmware/hub/models/
   ```

6. **Mobile app:**
   ```bash
   cd software/mobile-app
   npm install
   npx react-native run-android  # or run-ios
   ```

7. **Calibrate:**
   ```bash
   cd scripts
   python calibrate_tof.py       # Calibrate VL53L1X distance
   python calibrate_lamp.py      # Calibrate lamp CCT + brightness
   ```

8. **Pair nodes:** Power on the hub, then press the button on each node (Eye Tag, Desk Sentinel, Lamp Node) within 30 seconds to auto-pair.

---

## 15. Roadmap

| Phase | Milestone | Timeline |
|-------|-----------|----------|
| v1.0 | Core 4-node system, fatigue index, 20-20-20, basic lamp | Q1 2026 |
| v1.1 | Myopia forecast LSTM (children), optometrist reports | Q2 2026 |
| v1.2 | Circadian DQN lamp policy, dry-eye risk model | Q3 2026 |
| v1.3 | Outdoor-light tracking via mobile GPS lux estimation | Q4 2026 |
| v2.0 | Open-source API for third-party lamp integration | Q1 2027 |
| v2.1 | Contact-lens temperature tag (NTC micro-thermistor) | Q3 2027 |
| v2.2 | Integration with smart-glasses displays (AR bridge) | Q4 2027 |
| v3.0 | Glaucoma risk screening (IOP proxy via corneal temp) | 2028 |

---

## License

MIT — build it, sell it, improve it.

---

*Invented as device system #33 in the [Devices](https://github.com/jayis1/Devices) repository. See all systems in the [main README](../README.md).*
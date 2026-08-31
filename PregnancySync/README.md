# PregnancySync

**AI-powered prenatal maternal-fetal health and daily pregnancy support system** — a coordinated home + wearable platform that helps expectant families monitor fetal movement, hypertensive risk, sleep position, hydration, proteinuria, and early warning signs of complications without turning pregnancy into a full-time job.

## What It Solves

Pregnancy is joyful, but it is also full of uncertainty and preventable risk:

- **Reduced fetal movement** is often noticed late or inconsistently.
- **Preeclampsia warning signs** can emerge between clinic visits.
- **Supine sleep late in pregnancy** can worsen comfort and raise concern.
- **Protein, glucose, ketones, and hydration changes** are easy to miss at home.
- **Partners and caregivers** often want practical visibility without becoming invasive.

**PregnancySync** creates a practical prenatal observability layer. It combines an abdominal belly band, an automated blood-pressure cuff, a urine strip reader dock, and an under-mattress sleep pad under one hub. The stack includes embedded firmware in C, a deterministic Sub-GHz home mesh, a FastAPI + MQTT backend, a React Native mobile app, and an ML pipeline for reduced-movement, hypertensive-risk, and sleep-position forecasting.

---

## System Architecture

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                    PregnancySync Cloud / Edge Intelligence                 │
│ FastAPI + MQTT + SQLite/PostgreSQL + ML artifact registry + alert engine  │
│                                                                             │
│ Models                                                                      │
│ • KickGuard Temporal CNN surrogate  - reduced fetal movement risk           │
│ • PressureWatch XGBoost surrogate   - preeclampsia / hypertension risk      │
│ • StripSense classifier             - urine strip trend scoring             │
│ • SleepSide sequence model          - supine sleep burden + comfort score   │
│ • CarePlan policy engine            - coaching, escalation, partner routing │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │ MQTT over TLS / HTTPS
                                │
                   ┌────────────┴──────────────────────┐
                   │ PregnancySync Hub Gateway         │
                   │ CM4 + RP2040 + SX1262 + LTE Cat-1│
                   │ edge inference, alerts, OTA       │
                   └───────┬─────────────┬─────────────┘
                           │             │
             Sub-GHz 868 MHz TDMA mesh   │ BLE / Wi-Fi commissioning
                           │             │
     ┌─────────────────────┼─────────────┼──────────────────────┬──────────────────┐
     │                     │             │                      │                  │
┌────┴────────────┐ ┌──────┴──────────┐ ┌┴───────────────────┐ ┌┴───────────────┐ ┌┴─────────────┐
│ Belly Band      │ │ Smart BP Cuff   │ │ Strip Reader Dock  │ │ Sleep Pad      │ │ Mobile App   │
│ movement + EHG  │ │ BP + edema risk │ │ protein/glucose    │ │ BCG + posture  │ │ family view  │
│ posture + temp  │ │ pulse waveform  │ │ ketones + SG       │ │ left-side coach│ │ and reports  │
└─────────────────┘ └─────────────────┘ └────────────────────┘ └────────────────┘ └──────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **PregnancySync Hub Gateway** | Raspberry Pi CM4 + RP2040 + SX1262 + Quectel EG915U | Household coordinator, local rules, OTA, MQTT/FastAPI bridge, clinician export | 12V/3A adapter + LiFePO4 UPS HAT | Ethernet, Wi‑Fi, BLE 5.0, LTE Cat-1, Sub-GHz 868 MHz |
| **Belly Band** | ESP32-S3-WROOM-1 + ADS1292R + BMI270 | Fetal movement trend sensing, uterine activity proxy, maternal posture, skin temp, haptic check-in prompts | 1200 mAh LiPo | BLE 5.0 for setup, Sub-GHz 868 MHz |
| **Smart BP Cuff** | nRF52840 + MPX5050DP + DRV8837 | Oscillometric BP, pulse waveform quality, cuff compliance, edema questionnaire trigger | 2×18650 or 5V USB-C | BLE 5.0, Sub-GHz 868 MHz |
| **Strip Reader Dock** | RP2040 + AS7341 + HX711 | Urine strip colorimetry for protein/glucose/ketones/leukocytes + bottle weight / hydration support | USB-C 5V | Wi‑Fi via ESP-AT coprocessor or Sub-GHz daughtercard |
| **Sleep Pad** | STM32WL55JC + NAU7802 | Under-mattress BCG respiration, turn events, left-side adherence, comfort trend | 4×AA or wall supply | Integrated Sub-GHz 868 MHz |

---

## Daily User Experience

1. The belly band checks in twice a day with a 10-minute guided fetal movement session.
2. KickGuard notices movement counts dropping below the user’s learned baseline and requests a second session after hydration and a position change.
3. The BP cuff captures a clean morning reading and PressureWatch combines systolic/diastolic, trend slope, strip protein, and sleep burden.
4. The strip dock detects trace protein plus rising specific gravity and suggests hydration, rest, and a repeat reading.
5. The sleep pad tracks overnight supine time and vibrates the belly band if late-pregnancy side-sleep guidance is enabled.
6. If risk rises above threshold, the hub escalates from coaching → partner alert → clinician export packet.

---

## Node 1 — PregnancySync Hub Gateway

### Core hardware

- **Compute:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for watchdog, radio timing, battery monitoring, and fail-safe notifications
- **Long-range radio:** Semtech SX1262 on SPI
- **Cellular backup:** Quectel EG915U Cat-1 module over USB/UART for off-grid alert delivery
- **UPS:** 4-cell LiFePO4 HAT with INA219 current monitor and DS3231 RTC
- **UX:** 5 inch touch display, RGB status pillar, piezo buzzer, QR commissioning workflow

### Responsibilities

- Maintains the pregnancy profile, trimester stage, caregiver graph, and node registry
- Runs FastAPI backend, local MQTT bridge, clinician report generation, and OTA catalogs
- Executes edge risk scoring when the internet is down
- Stores recent telemetry, risk trends, symptom check-ins, and export packets
- Performs escalation policy and quiet-hours logic

### Hub interface map

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 I2C0 | INA219, RTC, LED expander |
| RP2040 UART0 | CM4 supervisor heartbeat |
| CM4 USB2 | EG915U LTE modem |
| CM4 DSI | 5 inch touchscreen |
| CM4 Ethernet | router uplink |

---

## Node 2 — Belly Band

Worn for guided check-ins, movement trend capture, comfort coaching, and haptic prompts.

### Hardware architecture

- **MCU:** ESP32-S3-WROOM-1-N16R8
- **Biopotential AFE:** ADS1292R for abdominal electromyography / maternal respiration proxy electrodes
- **Motion:** BMI270 low-noise IMU for kick bursts, turns, and posture changes
- **Temperature:** TMP117 skin temperature sensor
- **Haptics:** DRV2605L + linear resonant actuator
- **Battery management:** BQ24074 charger + MAX17048 fuel gauge
- **Long-range radio:** SX1262 on SPI

### Pin assignment

| Signal | ESP32-S3 pin | Peripheral |
|--------|--------------|------------|
| I2C_SDA | GPIO8 | TMP117 / BMI270 / MAX17048 / DRV2605L |
| I2C_SCL | GPIO9 | TMP117 / BMI270 / MAX17048 / DRV2605L |
| SPI_MOSI | GPIO11 | SX1262 |
| SPI_MISO | GPIO13 | SX1262 |
| SPI_SCK | GPIO12 | SX1262 |
| SX1262_NSS | GPIO10 | SX1262 |
| SX1262_BUSY | GPIO14 | SX1262 |
| ADS1292R_DRDY | GPIO4 | ADS1292R |
| ADS1292R_CS | GPIO5 | ADS1292R |
| BUTTON | GPIO0 | user check-in button |
| HAPTIC_INT | GPIO6 | DRV2605L notify |

### Power architecture

3.7V LiPo → BQ24074 power-path charger → 3V3 system rail. ADS1292R analog domain is isolated through ferrite + LDO for noise control. Belt runs 18–24 hours between charges with two guided sessions and overnight standby.

---

## Node 3 — Smart BP Cuff

### Hardware

- **MCU:** nRF52840
- **Pressure transducer:** MPX5050DP for cuff pressure capture
- **Pump/valves:** miniature diaphragm pump + two MOSFET-switched solenoid valves
- **Waveform front end:** MCP6002 anti-alias analog chain into SAADC
- **Motion artifact sensor:** LIS2DW12 IMU
- **Battery:** dual 18650 pack or medical-grade USB-C adapter
- **Long-range relay:** SX1262 for high-priority alerts when phone is absent

### Clinical workflow support

- Scheduled morning/evening BP sessions
- Waveform quality scoring to reject motion-corrupted readings
- Symptom prompts when BP is elevated: headache, swelling, visual changes, RUQ pain
- Trend export for clinician review

---

## Node 4 — Strip Reader Dock

### Hardware

- **MCU:** RP2040
- **Spectral sensor:** AS7341 10-channel visible/NIR sensor with controlled LED illumination
- **Strip lighting:** 365 nm UV, 470 nm blue, 525 nm green, 630 nm red, 850 nm IR LEDs
- **Weight sensing:** HX711 + 5 kg load cell for hydration bottle / sample tray verification
- **Local comms:** ESP32-C3 AT coprocessor over UART for Wi‑Fi commissioning and OTA
- **UX:** 2.13 inch e-ink prompt display, buzzer, status LED

### Uses

- Reads urine dipsticks for protein, glucose, ketones, leukocytes, nitrites, blood, and specific gravity
- Normalizes for ambient light and strip lot calibration
- Associates strip results with symptoms and BP trend
- Optionally tracks hydration bottle refill and morning weight pattern

---

## Node 5 — Sleep Pad

Slides under the mattress near thorax/abdomen zone.

### Hardware

- **MCU/radio:** STM32WL55JC
- **Ballistocardiography strips:** 4× piezo film channels into NAU7802 instrumentation ADC path
- **Posture sensing:** 8-zone force strip matrix for left/right/supine trend
- **Ambient bed climate:** SHT41
- **Vibration alert output:** low-profile bed shaker or band wake signal
- **Power:** 4×AA LiFeS2 or 5V wall adapter

### Why it matters

Sleep position, rest quality, respiratory effort trend, and nighttime restlessness often change before a user notices they are overexerting. PregnancySync converts those signals into gentle coaching instead of alarm fatigue.

---

## Communications Protocol

PregnancySync uses an **868 MHz deterministic TDMA mesh** for reliable bedroom-to-bathroom coverage with predictable battery life.

### Frame layout

| Byte(s) | Field |
|---------|-------|
| 0 | preamble |
| 1 | version |
| 2 | message type |
| 3 | source node ID |
| 4 | destination node ID |
| 5 | flags |
| 6-7 | payload length |
| 8..N | payload |
| N+1..N+2 | CRC16 |

### Message types

- `0x01` heartbeat
- `0x10` belly band session summary
- `0x11` BP session summary
- `0x12` strip reader result
- `0x13` sleep pad overnight summary
- `0x20` risk score update
- `0x30` user prompt / intervention
- `0x40` OTA fragment
- `0x7F` fault report

---

## Firmware Layout

```text
firmware/
├── common/
│   ├── protocol.h
│   └── protocol.c
├── hub_gateway/main.c
├── belly_band/main.c
├── bp_cuff/main.c
├── strip_reader_dock/main.c
└── sleep_pad/main.c
```

Each firmware target is standard C with portable hardware-abstraction stubs so the state machines compile on desktop CI before being bound to vendor HALs.

---

## Cloud / Edge Software

`software/dashboard/` contains a FastAPI service with:

- SQLite storage for local-first operation
- REST endpoints for telemetry ingest, overview, and alerts
- deterministic inference helpers for edge fallback
- MQTT integration hooks for broker bridging
- clinician export endpoints for summary packets

---

## ML Pipeline

`software/ml-pipeline/` includes training scripts for:

- reduced fetal movement risk (`train_kickguard.py`)
- hypertensive / preeclampsia risk (`train_pressurewatch.py`)
- sleep-position burden model (`train_sleepside.py`)
- synthetic cohort generation for repeatable testing (`generate_synthetic_dataset.py`)

These scripts use deterministic synthetic cohorts so the repo remains runnable without proprietary clinical data while preserving realistic feature engineering.

---

## Mobile App

`software/mobile-app/` contains a React Native starter app with:

- pregnancy week dashboard
- daily checklist and symptom check-in
- partner / caregiver risk feed
- clinician-export history screen
- node battery / connectivity panels

---

## BOMs

Detailed CSV BOMs for every node live in `hardware/bom/`.

---

## Safety, ethics, and limitations

- PregnancySync is a **decision-support and coaching system**, not a replacement for obstetric care.
- Risk outputs must never suppress urgent care when users report decreased fetal movement, severe headache, bleeding, or severe pain.
- All alerts should be clinically reviewed before commercialization.
- The system stores only summarized acoustic/biopotential features by default; raw signals remain opt-in.

---

## Repository Layout

```text
PregnancySync/
├── README.md
├── schematic/
├── firmware/
├── hardware/bom/
├── software/dashboard/
├── software/ml-pipeline/
├── software/mobile-app/
├── docs/
└── scripts/
```

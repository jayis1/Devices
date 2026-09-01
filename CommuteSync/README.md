# CommuteSync

**AI-powered daily commuting orchestration, safety, and exposure-management system** — a multi-node platform that prevents forgotten essentials, reduces lateness, tracks bag and bike security, measures route-level pollution and vibration stress, and gives commuters one coordinated system instead of five disconnected apps.

## What It Solves

Millions of people lose time and money every week to the same failures:

- leaving home without a bag, badge, laptop, keys, charger, or medication;
- missing safe transfer windows because one delay cascades through the whole trip;
- arriving stressed after high-noise, high-pollution, high-vibration routes;
- parking a bike or scooter with weak theft visibility;
- forgetting where critical work items were last seen.

**CommuteSync** builds a practical hardware+software commute layer around the doorway, the bag, the vehicle, the destination desk, and a hub that coordinates everything. It blends deterministic rules with ML forecasts so the system can act before a commuter is late, unsafe, overexposed, or separated from essentials.

---

## System Overview

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                    CommuteSync Cloud / Edge Intelligence                   │
│ FastAPI + MQTT + SQLite/PostgreSQL + route planner + artifact registry    │
│                                                                             │
│ Models                                                                      │
│ • ReadyCheck      - forgotten-item and departure-readiness risk            │
│ • DelayGraph      - missed-transfer / lateness forecasting                 │
│ • ExposureScore   - PM2.5 / VOC / vibration route burden scoring          │
│ • TheftWatch      - tamper + separation anomaly detection                  │
│ • HabitPolicy     - low-friction intervention timing engine                │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │ HTTPS / MQTT over TLS
                                │
                   ┌────────────┴──────────────────────┐
                   │ CommuteSync Hub Gateway           │
                   │ CM4 + RP2040 + SX1262 + LTE Cat-1│
                   │ local rules, OTA, reports, cache │
                   └───────┬─────────────┬─────────────┘
                           │             │
             868 MHz TDMA home/office    │ Wi-Fi / LTE backhaul
             mesh + BLE/UWB proximity    │
                           │             │
    ┌──────────────────────┼─────────────┼───────────────────────┬───────────────────┐
    │                      │             │                       │                   │
┌───┴───────────┐   ┌──────┴──────┐ ┌────┴─────────────┐ ┌───────┴──────────┐ ┌──────┴───────┐
│ Entry Dock    │   │ Bag Tag     │ │ Mobility Beacon │ │ Desk Dock         │ │ Mobile App    │
│ UWB + NFC +   │   │ bag attach  │ │ bike/car/scoot  │ │ arrival + dock    │ │ commute plan  │
│ load cells    │   │ tamper + IMU│ │ GPS + air + IMU │ │ item recall       │ │ alerts        │
└───────────────┘   └─────────────┘ └─────────────────┘ └───────────────────┘ └───────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **CommuteSync Hub Gateway** | Raspberry Pi CM4 + RP2040 + SX1262 + Quectel EG915U | Home/office coordinator, OTA, local rules, MQTT/FastAPI bridge, route caching, report generation | 12V/3A adapter + LiFePO4 UPS | Ethernet, Wi‑Fi, BLE 5.0, LTE Cat-1, 868 MHz |
| **Entry Dock** | ESP32-S3 + DW3000 + PN532 + HX711 | Doorway readiness check, bag/key/badge presence, departure confirmation, haptic/audio cueing | USB-C 5V | BLE 5.0, UWB, 868 MHz |
| **Bag Tag** | nRF52840 + DW3000 + LIS2DW12 + ATECC608B | Bag proximity, tamper, motion, anti-loss, handoff confirmation | CR2450 or 300 mAh LiPo | BLE 5.0, UWB, 868 MHz |
| **Mobility Beacon** | ESP32-C6 + u-blox M10 + ICM-42688-P + BME688 + SPS30 | Vehicle/bike telemetry, route timing, crash/tamper detection, pollution and vibration scoring | vehicle 12V or 2-cell Li-ion | Wi‑Fi 6, BLE 5.3, 868 MHz |
| **Desk Dock** | RP2040 + ESP32-C3 + HX711 + DW3000 | Arrival confirmation, item dock status, end-of-trip recall, charger/presence node | USB-C 5V | Wi‑Fi, BLE 5.0, UWB, 868 MHz |

---

## Daily User Experience

1. The commuter sets required items per trip template: workday, gym, school drop-off, airport, bike commute.
2. The Entry Dock confirms badge, bag, keys, and laptop sleeve presence using UWB ranging, NFC taps, and tray/hook load cells.
3. If something is missing, the dock speaks a short prompt and flashes only the relevant icon to avoid alarm fatigue.
4. The Mobility Beacon tracks live route speed, transit delay propagation, vibration exposure, and air quality burden.
5. If the bag separates from the commuter or the parked bike is tampered with, TheftWatch escalates from push alert to LTE fallback.
6. The Desk Dock logs arrival, remembers last-seen item state, and learns which interventions actually prevent lateness.

---

## Node 1 — CommuteSync Hub Gateway

### Core hardware

- **Compute:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for watchdog, SX1262 slot timing, LED/buzzer fail-safe control
- **Long-range radio:** Semtech SX1262 on SPI
- **Cellular backup:** Quectel EG915U Cat-1 for outage-safe alerts
- **UPS:** 4-cell LiFePO4 HAT with INA219 and DS3231 RTC
- **Local UX:** 4.3 inch touch dashboard, RGB column, piezo buzzer, QR onboarding

### Responsibilities

- Maintains commuter profiles, trip templates, item registry, and node certificates
- Runs FastAPI backend, MQTT broker bridge, OTA catalog, and export jobs
- Caches transit schedules and local route policies for offline operation
- Executes edge readiness and theft scoring when WAN is down
- Generates weekly commute burden reports and route recommendations

### Hub interface map

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 I2C0 | INA219, DS3231, LED expander |
| RP2040 UART0 | CM4 supervisor heartbeat |
| CM4 USB2 | EG915U LTE modem |
| CM4 Ethernet | router uplink |
| CM4 DSI | local dashboard display |

---

## Node 2 — Entry Dock

Doorway appliance for pre-departure readiness.

### Hardware architecture

- **MCU:** ESP32-S3-WROOM-1-N16R8
- **UWB:** Qorvo DW3000 for 10–30 cm proximity confirmation to bag and badge-tag anchors
- **NFC/RFID:** PN532 for employee badge, transit card, or wallet tap verification
- **Load sensing:** 2× HX711 channels for key tray and bag hook load cells
- **Distance sensing:** VL53L1X for hook occupancy and handoff timing
- **Audio/haptics:** MAX98357A speaker amp + vibration puck
- **Radio:** SX1262 for deterministic home mesh

### Pin assignment

| Signal | ESP32-S3 pin | Peripheral |
|--------|--------------|------------|
| I2C_SDA | GPIO8 | VL53L1X, status IO |
| I2C_SCL | GPIO9 | VL53L1X, status IO |
| SPI_MOSI | GPIO11 | SX1262 / DW3000 shared bus |
| SPI_MISO | GPIO13 | SX1262 / DW3000 shared bus |
| SPI_SCK | GPIO12 | SX1262 / DW3000 shared bus |
| SX1262_NSS | GPIO10 | SX1262 |
| DW3000_CS | GPIO5 | DW3000 |
| PN532_UART_TX | GPIO17 | PN532 |
| PN532_UART_RX | GPIO18 | PN532 |
| HX711_A_DT | GPIO3 | key tray load cell |
| HX711_A_SCK | GPIO4 | key tray load cell |
| HX711_B_DT | GPIO6 | bag hook load cell |
| HX711_B_SCK | GPIO7 | bag hook load cell |

### Power architecture

USB-C 5V input → buck to 3V3 digital rail. Speaker amp and vibration puck run off 5V switched rail. Always-on idle is below 160 mA with short active peaks during ranging and speech prompts.

---

## Node 3 — Bag Tag

Attaches to backpack, laptop sleeve, purse, or lunch carrier.

### Hardware

- **MCU:** nRF52840
- **UWB:** DW3000 for precise separation and handoff confirmation
- **IMU:** LIS2DW12 for motion, drop, and tamper events
- **Secure identity:** ATECC608B for signed handshakes
- **Alerting:** coin vibration motor + RGB LED
- **Power:** CR2450 for low-duty mode or 300 mAh LiPo for high-frequency commuters
- **Radio bridge:** small 868 MHz transceiver daughterboard

### Functions

- detects separation from doorway and desk anchors;
- logs last-seen coordinates passed from the Mobility Beacon;
- triggers tamper alerts when bag is opened or moved during parked state;
- advertises item class and battery state to the hub.

---

## Node 4 — Mobility Beacon

A rugged node for bike handlebars, scooter stems, or vehicle dashboards.

### Hardware

- **MCU:** ESP32-C6-WROOM-1
- **GNSS:** u-blox MAX-M10S
- **IMU:** ICM-42688-P for shock, crash, and road vibration
- **Air quality:** Sensirion SPS30 PM1/PM2.5/PM10 + Bosch BME688 VOC/temp/humidity
- **Power:** automotive 12V input or dual-cell Li-ion pack with USB-C charging
- **Expansion:** CAN transceiver footprint or wheel-speed reed input
- **Radio:** SX1262 fallback for local coordination when Wi‑Fi/cellular are absent

### Why it matters

Commute quality is not just travel time. Fine particulates, repeated vibration, and unsafe route geometry add up over months. The Mobility Beacon lets CommuteSync optimize for **arrival + safety + exposure**, not just ETA.

---

## Node 5 — Desk Dock

Destination-side memory and arrival node.

### Hardware

- **MCU pair:** RP2040 + ESP32-C3 coprocessor
- **UWB:** DW3000 arrival anchor for bag/laptop confirmation
- **Load cell:** HX711 + tray cell for item dock presence
- **NFC:** optional PN532 for badge check-in reuse
- **Display:** 2.13 inch e-ink status panel for “all set / missing item / leaving soon” guidance
- **Power:** USB-C 5V

### Uses

- confirms trip completion and suppresses stale “missing bag” alarms;
- records which items were present on arrival versus departure;
- warns before leaving work if a charger, badge, or laptop is still docked;
- learns commute-specific item sets over time.

---

## Communications Protocol

CommuteSync uses a hybrid protocol:

- **868 MHz TDMA mesh** for deterministic home/office coordination;
- **BLE 5.x** for commissioning and phone-side low-power interactions;
- **UWB** for exact doorway/desk handoff checks;
- **Wi‑Fi/LTE + MQTT** for cloud sync, route data, and alerts.

### 868 MHz frame layout

| Byte(s) | Field |
|---------|-------|
| 0 | preamble |
| 1 | protocol version |
| 2 | message type |
| 3 | source node ID |
| 4 | destination node ID |
| 5 | flags |
| 6-7 | payload length |
| 8..N | payload |
| N+1..N+2 | CRC16 |

### Message types

- `0x01` heartbeat
- `0x10` readiness snapshot
- `0x11` bag tamper event
- `0x12` route sample
- `0x13` arrival confirmation
- `0x20` model score update
- `0x30` intervention request
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
├── entry_dock/main.c
├── bag_tag/main.c
├── mobility_beacon/main.c
└── desk_dock/main.c
```

All firmware is plain C and organized around deterministic state machines so it can be ported to Zephyr, ESP-IDF, Pico SDK, or vendor HALs.

---

## Cloud Software

`software/dashboard/` contains a FastAPI service that:

- stores telemetry in SQLite by default;
- exposes readiness, theft, route, and exposure APIs;
- computes interpretable heuristic scores from recent telemetry;
- returns actions such as “badge missing”, “take lower-PM route”, or “bike tamper likely”.

Key endpoints:

- `GET /api/v1/health`
- `GET /api/v1/overview`
- `GET /api/v1/nodes`
- `POST /api/v1/telemetry/entry`
- `POST /api/v1/telemetry/bag`
- `POST /api/v1/telemetry/mobility`
- `POST /api/v1/telemetry/desk`
- `POST /api/v1/actions/checkin`

---

## ML Pipeline

`software/ml-pipeline/` includes reproducible synthetic-data and artifact-generation scripts for:

- **ReadyCheck** — missing-item and departure-readiness risk
- **DelayGraph** — lateness and missed-transfer risk
- **ExposureScore** — commute burden from PM2.5, VOC, vibration, and duration

Artifacts are JSON files so the edge/backend can load them without heavyweight dependencies.

---

## Mobile App

The React Native stub in `software/mobile-app/` includes:

- today’s commute readiness card,
- live bag/bike status,
- route comparison with exposure deltas,
- destination leave-behind checklist,
- weekly “minutes saved / alerts prevented / exposure reduced” summaries.

---

## BOMs

Detailed build BOMs live in `hardware/bom/` for every node with manufacturer part suggestions and quantities.

---

## Documentation

- `docs/architecture.md` — end-to-end architecture and power/data flow
- `docs/api.md` — REST contract and payload examples
- `docs/protocol.md` — packet formats, timing slots, pairing, and OTA framing

---

## Build Notes

- 868 MHz should be adjusted for region-specific ISM band constraints.
- UWB anchors should be spaced to reduce metal-shadow multipath near doors.
- SPS30 intake path on the Mobility Beacon needs splash protection but free airflow.
- Desk/Entry load cells should be mechanically preloaded for repeatable thresholds.

---

## Repo Structure

```text
CommuteSync/
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

CommuteSync is designed so someone can build a first prototype with off-the-shelf dev boards, then collapse the design into custom PCBs once the workflows are validated.

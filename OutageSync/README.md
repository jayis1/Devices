# OutageSync

**AI-powered home outage resilience and backup orchestration system** — a whole-home platform that detects grid instability before a blackout, protects refrigerators and medical loads, coordinates battery/inverter/generator resources, forecasts outage duration, and guides families through safe, low-stress operation during utility failures.

## What It Solves

Grid outages are no longer rare edge cases. Heat waves, storms, wildfire shutoffs, overloaded transformers, and aging distribution infrastructure leave millions of households dealing with the same problems:

- **People do not know how long an outage will last.** They either overreact and waste fuel or underreact and lose food, medication, internet, and comfort.
- **Critical loads compete blindly for limited backup power.** Refrigerators, CPAP machines, modems, sump pumps, medical coolers, and phone chargers all matter, but most homes have no automatic load-priority logic.
- **Generator use is risky.** Fuel runs out, maintenance gets skipped, carbon monoxide becomes dangerous, and people backfeed circuits unsafely.
- **Food and temperature-sensitive medicine quietly become unsafe.** A freezer may look cold while door openings and internal warming have already reduced safe hold time.
- **Families need a calm operating picture.** During blackouts they need a single dashboard that says what is safe, what should stay off, when to run the generator, and which room is best for comfort.

**OutageSync** turns outage preparedness into a coordinated hardware/software system. It watches utility quality at the panel, tracks runtime and priority of individual critical loads, measures refrigerator/freezer and medicine temperatures directly, monitors generator/fuel/CO safety, and uses ML models to recommend the best backup strategy for the next 15 minutes, 2 hours, and 12 hours.

---

## System Architecture

```text
┌────────────────────────────────────────────────────────────────────────────────┐
│                            OutageSync Cloud / Edge                             │
│ FastAPI + SQLite/Postgres adapter + MQTT + model store + alerting             │
│                                                                                │
│ Models                                                                         │
│ • OutageCast        - 15 min to 24 h outage duration forecast                  │
│ • ShedPolicy        - prioritized load-shedding recommendation engine          │
│ • ColdGuard         - food / medicine thermal safety estimator                 │
│ • GenHealth         - generator failure / maintenance anomaly model            │
│ • ComfortDecay      - room comfort and habitability forecast                   │
└───────────────────────────────┬────────────────────────────────────────────────┘
                                │ MQTT over TLS / HTTPS / WebSocket
                                │
                 ┌──────────────┴─────────────────┐
                 │     OutageSync Resilience Hub   │
                 │     CM4 + RP2040 + SX1262 + LTE │
                 │     local rules + dashboard     │
                 └──────┬───────────────┬──────────┘
                        │               │
       Sub-GHz 868 MHz  │               │ BLE 5.0 / Wi-Fi setup
                        │               │
     ┌──────────────────┼───────────────┼───────────────────┬──────────────────┐
     │                  │               │                   │                  │
┌────┴─────────┐  ┌─────┴──────────┐ ┌──┴─────────────┐ ┌───┴────────────┐ ┌──┴──────────────┐
│ Panel        │  │ Cold Chain Tag │ │ Critical Outlet│ │ Fuel & Air     │ │ Mobile App /    │
│ Controller   │  │ xN             │ │ Node xM        │ │ Sentinel        │ │ caregiver view  │
│ grid quality │  │ fridge/freezer │ │ relay+meter    │ │ gen shed safety │ │ local + remote  │
│ ATS/contact  │  │ medicine temp  │ │ smart shedding │ │ CO/fuel/temp    │ │                 │
└──────────────┘  └────────────────┘ └────────────────┘ └─────────────────┘ └─────────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **OutageSync Resilience Hub** | Raspberry Pi CM4 + RP2040 + Semtech SX1262 + Quectel EG25-G | Local orchestration, MQTT bridge, dashboard, OTA, LTE fallback alerts | 12V/5A AC adapter + 2x18650 UPS HAT | Ethernet, Wi‑Fi, BLE 5.0, LTE, Sub‑GHz 868 MHz |
| **Panel Controller** | STM32G474RET6 + ADE9153A + RS-485 isolated front-end | Grid sag/swell/frequency detection, inverter/ATS coordination, branch priority control | 24V DIN rail input -> isolated 12V/5V/3V3 | Sub‑GHz 868 MHz, RS‑485/Modbus, dry contacts |
| **Cold Chain Tag** | nRF52840 + SX1262 + TMP117 + SHT45 | Fridge/freezer/medicine temperature, door-open duration, safe hold-time tracking | CR2477 or 2xAAA lithium | BLE 5.0, Sub‑GHz 868 MHz |
| **Critical Outlet Node** | ESP32-C6-MINI-1 + ATM90E26 + latching relay | Per-load metering, smart load shedding, outlet priority enforcement | 120/230VAC mains with isolated AC-DC + backup supercap | Wi‑Fi 6, BLE 5, Sub‑GHz 868 MHz |
| **Fuel & Air Sentinel** | ESP32-S3-WROOM-1 + SX1262 + SCD41 + MICS-6814 + JSN-SR04T | Generator shed CO/CO₂/NO₂ monitoring, fuel-level estimation, engine heat/vibration tracking | 12V battery / wall input | Wi‑Fi, BLE, Sub‑GHz 868 MHz |

---

## Daily User Experience

1. Utility voltage begins to sag repeatedly at 17:40 during a heat wave.
2. The **Panel Controller** detects undervoltage, THD rise, and frequency drift, and pushes a pre-outage warning to the hub.
3. The hub predicts a **67% chance of outage within 90 minutes** and pre-cools the freezer recommendation window, tops off battery-backed medical outlets, and asks the user to defer laundry and EV charging.
4. Power fails at 18:12. The hub immediately classifies the home into **battery-only preservation mode**.
5. **Critical Outlet Nodes** keep fridge, internet, CPAP, and device charging live while shedding nonessential loads.
6. **Cold Chain Tags** estimate safe hold time: freezer 31.4 h unopened, fridge 3.2 h with current door-open rate, insulin cooler 7.8 h.
7. If battery reserve falls below policy thresholds, the hub checks **Fuel & Air Sentinel** for safe CO/fuel status, then recommends or auto-enables generator start through the panel controller.
8. The mobile app shows plain-language advice: **"Keep freezer closed. Run generator in 22 minutes for 48-minute charge window. Bedroom 2 stays coolest until 02:00."**
9. When utility service returns, the system performs staged reconnection to avoid inrush overload, logs outage cost, and schedules generator maintenance if runtime exceeded service thresholds.

---

## Node 1 — OutageSync Resilience Hub

### Core hardware

- **Compute module:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for watchdog, radio TDMA timing, and safe restart sequencing
- **Sub‑GHz radio:** SX1262 at 868 MHz with +22 dBm PA path
- **Cellular fallback:** Quectel EG25-G mini-PCIe modem with GNSS for regional outage correlation
- **Local UI:** 7 inch DSI touch screen, RGB status pillar, piezo buzzer
- **Storage:** eMMC rootfs + industrial microSD event spool
- **Power:** 12V input -> 5V/6A buck for CM4/display, 3V3 buck for radio/MCU; UPS HAT with two 18650 cells

### Hub responsibilities

- Maintains the home outage policy: priorities, medical constraints, runtime goals, quiet hours, comfort rules
- Hosts FastAPI backend, WebSocket events, local dashboard, and MQTT bridge
- Schedules deterministic TDMA slots for all Sub-GHz nodes
- Runs cached inference when internet is down
- Sends LTE/SMS/email alerts if home internet is unavailable
- Stores outage timeline, branch reconnection logs, and maintenance history

### Pin / interface map

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 UART0 | CM4 heartbeat and failsafe RPC |
| RP2040 I2C0 | INA219, RTC, buzzer driver |
| CM4 USB2 | EG25-G modem |
| CM4 DSI | 7 inch touch display |
| CM4 Ethernet | router uplink |
| CM4 GPIO | tower LED, service button |

---

## Node 2 — Panel Controller

### Why it matters

Most outage systems only react after power is already gone. The panel node measures power quality directly where it matters and can coordinate **automatic transfer**, **branch priority disconnects**, and **staged restoration**.

### Electrical architecture

- **MCU:** STM32G474RET6 for fast ADC/timers and industrial control
- **Energy metering AFE:** Analog Devices ADE9153A for line voltage, current, power factor, THD, and frequency
- **Current transformers:** SCT-013-000 on mains and selected backup branches
- **Transfer/contact logic:** opto-isolated drivers for contactors, dry contacts to inverter/generator ATS, manual lockout input
- **Field bus:** isolated MAX3485 RS‑485 for inverter/BMS/ATS Modbus integration
- **Safety:** reinforced isolation, MOV surge arresters, thermal fuse, DIN-rail enclosure, clear creepage zones

### Interfaces and pins

| Signal | MCU pin | Notes |
|--------|---------|-------|
| ADE9153A SPI SCK/MISO/MOSI | PA5/PA6/PA7 | metering interface |
| ADE9153A CS | PB0 | dedicated chip select |
| RS‑485 TX/RX | PA2/PA3 | inverter telemetry |
| Contactors K1-K4 | PB8-PB11 | branch priority relays |
| Grid presence opto | PC13 | mains status interrupt |
| Service UART | PA9/PA10 | commissioning |

---

## Node 3 — Cold Chain Tag

A refrigerator is not a single thermal block. Door openings, shelf placement, and evaporator location matter. OutageSync uses small tags placed in the **fridge**, **freezer**, **garage freezer**, and **medicine cooler**.

### Hardware

- **SoC:** nRF52840 for ultra-low-power sensing and BLE commissioning
- **Long-range radio:** SX1262 for reliable communication through insulated metal compartments
- **Primary sensor:** TMP117 ±0.1°C digital temperature sensor
- **Environmental sensor:** Sensirion SHT45 for humidity and condensation risk
- **Door state:** reed switch + magnet or LIS3DH shock/tilt fallback
- **Status UX:** single RGB indicator + magnetic pairing button
- **Power:** CR2477 primary cell or 2xAAA lithium for freezer reliability

### Use cases

- Food safety hold-time estimation under real door-open behavior
- Temperature excursion logging for insulin, biologics, pumped breast milk, and vaccines
- Pre-outage overcool recommendation before predicted failures

---

## Node 4 — Critical Outlet Node

These nodes sit between backup power and devices that matter most.

### Hardware

- **SoC:** ESP32-C6-MINI-1
- **Energy meter:** ATM90E26 single-phase metering IC
- **Relay:** bistable 16 A latching relay for fail-retain state and low quiescent draw
- **Current transformer/shunt:** compact burdened CT or precision shunt depending SKU
- **Local UI:** tri-color LED, override button, piezo chirp
- **Protection:** MOV, thermal fuse, relay weld detection via feedback line, isolated AC-DC module

### Policy examples

- Keep modem/router always on while battery > 25%
- Cycle chest freezer 15 min on / 45 min off when battery reserve < 35%
- Disable entertainment outlet instantly during outage
- Keep CPAP outlet non-sheddable unless battery reserve is critical and manual confirmation occurs

---

## Node 5 — Fuel & Air Sentinel

### Hardware

- **SoC:** ESP32-S3-WROOM-1
- **Gas/air sensors:** SCD41 CO₂, MICS-6814 gas trio, optional electrochemical CO module for safety SKU
- **Fuel level:** JSN-SR04T ultrasonic sensor for diesel/gasoline day tank or propane enclosure distance check
- **Temperature/vibration:** TMP235 analog temp + IIS2DLPC accelerometer for engine run profile
- **Outputs:** buzzer, warning strobe, dry contact to inhibit generator auto-start if unsafe
- **Enclosure:** IP65 wall box with cable glands

### Event logic

- Inhibits auto-start if CO already elevated, enclosure too hot, or fuel estimate below minimum start threshold
- Detects generator actually running via vibration + temperature rise + line frequency confirmation from panel controller
- Estimates time-to-refuel and maintenance interval from runtime accumulation and vibration signature drift

---

## Communications and Protocol

OutageSync uses a **deterministic 868 MHz TDMA star/mesh hybrid** for resilient low-bandwidth control even when Wi‑Fi is congested or down.

- **Topology:** hub coordinator with repeat-capable mains nodes
- **Slot length:** 40 ms control slots, 200 ms telemetry superframe during outage events
- **Payload max:** 56 bytes
- **Protection:** CRC-16/CCITT + AES-128 CTR payload encryption + rolling session nonce
- **Addressing:** 16-bit household-scoped node IDs
- **Priority classes:** emergency shutoff > critical telemetry > configuration > bulk logs/OTA
- **Fallback commissioning:** BLE 5.0 from mobile app to each node

See [`docs/protocol_spec.md`](docs/protocol_spec.md) and [`firmware/common/protocol.h`](firmware/common/protocol.h).

---

## ML Stack

### 1. OutageCast
Forecasts probable restoration time using:
- recent voltage sag/swell/frequency instability
- weather severity and utility event features
- neighborhood historical outage duration priors
- time of day, season, and feeder stress indicators

### 2. ShedPolicy
A policy model that ranks loads by:
- medical criticality
- thermal inertia
- instantaneous and average watt draw
- backup reserve state
- occupancy and quiet hours

### 3. ColdGuard
Estimates remaining safe hold time for food and medicine using:
- internal temperature curve
- door-open count and open duration
- appliance category (fridge/freezer/cooler)
- ambient room temperature

### 4. GenHealth
Detects maintenance drift and likely starting failure from:
- vibration RMS / spectral bands
- runtime since last oil change
- fuel quality proxy / start duration
- enclosure temperature trends

### 5. ComfortDecay
Predicts which room will remain most livable during hot or cold outages using:
- room temperature and humidity history
- solar gain / outside weather
- occupancy and door/window patterns
- available fan / outlet status

---

## Safety and Power Architecture

- **Never backfeed the utility.** Panel controller is designed around transfer contacts and supervisory interlocks, not improvised cord-based methods.
- **Generator safety is primary.** CO, enclosure temperature, and fuel constraints are hard interlocks.
- **Critical outlets fail to last safe state.** Latching relay preserves configuration through brownouts.
- **Hub remains online during brief outages.** UPS ride-through keeps orchestration, logs, and cellular alerts alive.
- **Panel node uses isolated sensing and control domains.** High-voltage and SELV regions are physically separated.

---

## Firmware Layout

```text
firmware/
├── common/
│   ├── crc16.c / crc16.h
│   ├── mesh.c / mesh.h
│   └── protocol.c / protocol.h
├── hub/
├── panel-controller/
├── cold-chain-tag/
├── outlet-node/
└── fuel-sentinel/
```

Each node has buildable C source with deterministic simulation-friendly logic that can be ported to real HAL drivers.

---

## Software Stack

- **FastAPI backend** for orchestration APIs, WebSocket events, policy changes, and telemetry ingestion
- **MQTT topics** for node events, commands, acknowledgements, and edge alerts
- **SQLite by default** for portable local development, with environment variable override for Postgres
- **React Native app** for household and caregiver control
- **ML scripts** for generating and training baseline models with synthetic data

---

## Folder Structure

```text
OutageSync/
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

---

## Build / Verification Quick Start

```bash
cd software/dashboard
uvicorn main:app --reload

cd ../ml-pipeline
python train_outage_duration.py
python train_load_shed_policy.py
```

---

## Future Extensions

- bidirectional EV charger integration for vehicle-to-home resilience
- neighborhood outage mesh federation between nearby homes
- automatic comfort-room guidance using smart shades and fans
- refrigerated medication case with active Peltier holdover
- FEMA/utility export bundles for claim documentation

---

## License

MIT

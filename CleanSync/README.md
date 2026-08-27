# CleanSync

**AI-powered whole-home cleaning orchestration and hygiene system** — a multi-node home cleanliness platform that senses where dirt is building up, automatically prepares and services a robot vacuum/mop dock, verifies whether kitchens and bathrooms were actually cleaned, forecasts high-risk messes before they happen, and coordinates the right intervention at the right time.

## What It Solves

Cleaning is one of the most repetitive, least optimized parts of daily life:

- **Homes get dirty unevenly.** Entryways, kitchens, bathrooms, pet paths, and kids' zones accumulate debris much faster than guest rooms, but most people clean on rigid schedules instead of actual need.
- **Robot vacuums still need a human babysitter.** Tanks run dry, dirty-water bins fill up, brushes foul, and schedules fire at the worst possible moment.
- **"Looks clean" is often not clean.** Grease, soap residue, urine splash, toothpaste film, and food biofilm remain on high-touch surfaces after quick wipe-downs.
- **Busy households miss early warning signs.** A spill near a sink becomes a slip hazard; a damp bathroom becomes a mildew hotspot; a pet-feeding corner turns into an odor source.
- **Supplies disappear at bad times.** Mop detergent, trash liners, sanitizer cartridges, and brush heads run out when the house needs them most.

**CleanSync** turns home cleaning from a chore calendar into a responsive cyber-physical system. Distributed dirt sentinels monitor room-level buildup and occupancy patterns, a dock controller keeps the floor robot ready and safe, a handheld surface wand verifies hygiene on counters and fixtures, and the hub fuses everything into room-level cleanliness scores, route optimization, consumable planning, and proactive alerts.

---

## System Architecture

```text
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                           CleanSync Cloud / Edge Platform                           │
│ FastAPI + MQTT + SQLite/PostgreSQL + object storage + ML pipeline                  │
│                                                                                     │
│ Models                                                                              │
│ • DirtCast          - room-level dirt accumulation forecast                         │
│ • GrimeNet          - fluorescent residue / soap film / grease classifier           │
│ • RouteBandit       - robot cleaning timing and room-priority optimizer             │
│ • SlipRisk          - wet-floor / spill risk model                                  │
│ • SupplyFlow        - detergent, brush, and bag depletion forecast                  │
└────────────────────────────────────┬────────────────────────────────────────────────┘
                                     │ MQTT over TLS / HTTPS REST / WebSocket
                                     │
                       ┌─────────────┴───────────────────────┐
                       │      CleanSync Hub Gateway          │
                       │   Raspberry Pi CM4 + RP2040 + LTE   │
                       │ Local rules, edge inference cache   │
                       └───────┬──────────────────┬──────────┘
                               │                  │
              Sub-GHz 868 MHz TDMA mesh           │ BLE / Wi-Fi commissioning
                               │                  │
     ┌─────────────────────────┼──────────────┬───┴────────────────────┐
     │                         │              │                        │
┌────┴───────────┐    ┌────────┴────────┐ ┌───┴─────────────────┐ ┌────┴────────────┐
│ Dirt Sentinel  │    │ Dirt Sentinel   │ │ Robot Dock          │ │ Surface Wand     │
│ entry / hall   │ .. │ bath / kitchen  │ │ Controller          │ │ RGB + UV inspect │
│ PM + humidity  │    │ PM + mmWave     │ │ refill, drain, UV-C │ │ residue grading  │
└────────────────┘    └─────────────────┘ └─────────────────────┘ └──────────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **CleanSync Hub Gateway** | Raspberry Pi CM4 + RP2040 + SX1262 + BG95-M3 | Runs local API, MQTT bridge, schedules TDMA mesh, stores house map, dispatches clean jobs, sends LTE fallback alerts | 12 V / 4 A wall power + 2-cell 18650 UPS | Ethernet, Wi-Fi, BLE 5.0, Sub-GHz 868 MHz, LTE Cat-M1 |
| **Dirt Sentinel** | STM32WL55JC | Room-level dirt, humidity, occupancy, and spill-risk sensing | 2×AA lithium or USB-C 5 V | Sub-GHz 868 MHz |
| **Robot Dock Controller** | ESP32-S3-WROOM-1 | Clean-water refill, dirty-water extraction, detergent dosing, brush wash, UV-C sanitation, robot handshake | 24 V / 6 A dock supply | Wi-Fi, BLE 5.0, Sub-GHz 868 MHz |
| **Surface Wand** | ESP32-S3-WROOM-1 + RP2040 co-processor | Handheld fluorescence inspection of counters, sinks, toilets, tiles, handles, and cutting boards | 1×21700 Li-ion + USB-C PD charge | Wi-Fi, BLE 5.0, Sub-GHz 868 MHz |

---

## Why People Would Want It

- Kitchens and bathrooms stay clean **without guesswork**.
- The robot is actually ready when you need it, instead of dry, jammed, or full.
- High-traffic areas get cleaned more often than low-use areas.
- Parents, allergy sufferers, pet owners, and busy professionals see measurable hygiene improvement.
- Caregivers or housekeepers can prove work completion with objective before/after verification.

---

## Daily User Experience

1. Dirt sentinels notice that the mudroom and kitchen have unusually high particulate counts after a rainy day and heavy foot traffic.
2. SlipRisk flags the kitchen sink zone because humidity is high, occupancy just spiked, and the floor reflectance baseline changed after dishwashing.
3. The hub schedules a targeted robot mop run for the kitchen + hall at 10:15 AM, after everyone leaves for school/work.
4. Before dispatch, the dock controller tops up clean water, doses detergent, confirms dirty tank capacity, and sterilizes the mop roller with UV-C.
5. After dinner, a user scans the stovetop and counter using the surface wand. GrimeNet grades the burner area as **light grease residue** and suggests a degrease pass.
6. The mobile app records verified-clean zones, updates the cleaning score, and pushes tomorrow's supply forecast: *"Mop detergent will fall below 10% in 5 days."*
7. Over time, RouteBandit learns the best cleaning windows that minimize interruptions while keeping the house above the target cleanliness threshold.

---

## Node 1 - CleanSync Hub Gateway

### Core hardware

- **Main compute:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for radio timing, power supervision, and hard watchdog reset
- **Long-range radio:** Semtech SX1262 at 868 MHz with matched whip or PCB antenna
- **Cellular backup:** Quectel BG95-M3 LTE Cat-M1/NB-IoT module for outage alerts
- **User interface:** 5 inch 800×480 DSI touchscreen + RGB status pillar + piezo buzzer
- **Storage:** CM4 eMMC + industrial microSD for rolling telemetry buffer
- **Power:** 12 V input -> 5 V / 5 A buck for CM4 + USB peripherals, 3.3 V buck for RP2040/radio, 2-cell 18650 UPS HAT for ~3 h ride-through

### Major responsibilities

- Maintains room map, floor-type profiles, robot capability profile, and household quiet hours
- Aggregates TDMA mesh telemetry and relays it to MQTT topics
- Caches ML models for local scoring during internet outages
- Computes cleanliness score per room and dispatches targeted cleaning missions
- Manages OTA manifests, secure node provisioning, and local audit logs

### Hub pin / interface map

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 868 MHz radio |
| RP2040 UART0 | BG95-M3 cellular modem |
| RP2040 I2C0 | INA219 power monitor, RV-3028 RTC |
| RP2040 GPIO | tower LED, buzzer, UPS status lines |
| CM4 DSI | 5 inch touch display |
| CM4 Ethernet | RJ45 uplink |
| CM4 USB2 | service flash / export drive |
| CM4 UART | RP2040 heartbeat |

---

## Node 2 - Dirt Sentinel

A Dirt Sentinel sits in each target room or zone: entryway, kitchen, bathroom, pet area, hallway, playroom, etc.

### Sensors and functions

- **MCU/radio:** STM32WL55JC (Cortex-M4 + integrated Sub-GHz radio)
- **Airborne particulate:** PM1006K laser dust sensor for fine household dust trends
- **Environment:** Sensirion SHT41 temperature/humidity sensor
- **Occupancy and motion:** Hi-Link LD2410B 24 GHz mmWave presence sensor
- **Surface spill proxy:** Vishay VEML6035 ambient light sensor aimed at floor patch to detect reflectance change from wetness
- **Shock / tamper:** LIS2DW12 accelerometer
- **Optional VOC:** SGP40 for kitchen/pet-area odor zones
- **UX:** tri-color status LED + install button

### Mechanical placement

- Mounted 20-40 cm above floor on wall or furniture kickboard
- Floor-facing light sensor watches a 15×15 cm patch where spills or wetness are likely
- PM inlet uses labyrinth channel and replaceable dust mesh to reduce hair ingestion

### Power architecture

- **Primary:** 2×AA Li-FeS2 cells for 12-18 month operation in low-traffic rooms
- **High-traffic / mmWave-heavy rooms:** USB-C 5 V wall power recommended
- 3.3 V rail from TPS62740 low-IQ buck, sensor rails switchable by load switch

### Example outputs

- `dust_index`: normalized 0-100 room dust burden
- `traffic_score`: derived from mmWave occupancy dwell
- `wet_floor_probability`: 0-1 from reflectance + humidity + recent cleaning state
- `odor_index`: optional VOC trend for pet/garbage zones

---

## Node 3 - Robot Dock Controller

This node upgrades a vacuum/mop docking station into a self-maintaining cleaning appliance.

### Hardware architecture

- **SoC:** ESP32-S3-WROOM-1-N16R8
- **Flow sensing:** 2× YF-S201 hall flow meters for clean-water fill and dirty-water extraction confirmation
- **Level sensing:** 3× capacitive level probes (clean tank, detergent, dirty tank)
- **Pump control:** 12 V peristaltic detergent pump + 24 V diaphragm pump + 24 V drain pump
- **Valve control:** 2× latching solenoid valves for fill and purge
- **Brush sanitation:** 275 nm UV-C LED array with lid interlock and light shield
- **Safety:** INA260 current monitor, float switch redundancy, leak tray electrodes, E-stop, thermal fuse on UV rail
- **Robot link:** BLE GATT for status exchange with supported robot; IR blaster fallback for universal dock commands
- **UX:** 2.4 inch LCD, rotary encoder, RGB state bar

### What it does

- Verifies consumables before a scheduled job
- Refills robot reservoir to target volume
- Extracts dirty water after mop jobs
- Washes mop/roller using controlled pump cycles
- Sanitizes brush chamber with timed UV-C when no user is present
- Detects clogs, leaks, and tank mis-seating before they become messes

---

## Node 4 - Surface Wand

A handheld inspection + verification tool for bathrooms, kitchens, nursery surfaces, cutting boards, switches, rails, and toilet splash zones.

### Core hardware

- **Application SoC:** ESP32-S3-WROOM-1-N16R8
- **Sensor co-processor:** RP2040 for deterministic LED pulse timing and ADC capture
- **Camera:** OV5640 5 MP module with macro lens for close-up residue imagery
- **Spectral sensor:** AMS AS7341 (visible + NIR channels)
- **Fluorescence channel:** 405 nm UV-A LED + OPT4048 color/light sensor for residue fluorescence response
- **Distance:** VL53L1X ToF to lock inspection standoff at 35-50 mm
- **Battery:** 21700 Li-ion cell, BQ25895 charger, MAX17048 fuel gauge
- **UX:** 2.8 inch IPS touch display, haptic motor, trigger switch

### Inspection modes

- **Kitchen mode:** grease, protein film, cutting-board contamination, sink splash
- **Bathroom mode:** soap scum, urine splash, toothpaste film, mold-prone damp residue
- **Nursery mode:** high-chair, changing station, bottle-prep surface verification
- **Office/shared mode:** desk, keyboard surround, door handle, break-room counter

### Output categories

- clean
- dust film
- soap residue
- grease residue
- organic splash / biofilm suspicion
- moisture/mildew risk

---

## Communication and Protocol

CleanSync uses a **deterministic 868 MHz TDMA mesh** for low-latency room telemetry and robust penetration across tile, cabinetry, and bathrooms.

- **Application payload:** max 48 bytes
- **Addressing:** 16-bit node IDs, household network ID, per-boot session nonce
- **Integrity:** CRC-16/CCITT
- **Encryption:** AES-128 CTR on payload bytes
- **Commissioning:** BLE 5.0 direct pairing from the mobile app, then credentials transferred to mesh nodes
- **Update policy:** staged OTA with 10% canary rollout per node class

Topics:

- `cleansync/hub/health`
- `cleansync/dirt/<node_id>/telemetry`
- `cleansync/dock/<node_id>/state`
- `cleansync/wand/<node_id>/scan`
- `cleansync/alerts`

See [`docs/protocol_spec.md`](docs/protocol_spec.md) and [`firmware/common/protocol.h`](firmware/common/protocol.h).

---

## ML Stack

### 1. DirtCast
Forecasts 6 h / 24 h / 72 h dirt burden per room using:

- current dust index and trend
- occupancy dwell
- floor type
- weather import (rain/snow -> entryway dirt)
- pet presence
- last-cleaned timestamp

### 2. GrimeNet
Classifies handheld surface scans using:

- RGB macro crop
- fluorescence response under 405 nm excitation
- AS7341 spectral vector
- room type and surface material

### 3. RouteBandit
Learns when and where to dispatch cleaning to maximize cleanliness while minimizing interruption, noise nuisance, and unnecessary water use.

### 4. SlipRisk
Predicts near-term wet-floor hazard in kitchens and bathrooms using reflectance delta, humidity, occupancy, and recent dock-run or shower events.

### 5. SupplyFlow
Estimates days remaining for detergent, brush rollers, dust bags, and dirty-water capacity from recent job counts and measured volumes.

---

## Firmware Layout

```text
firmware/
├── common/
│   ├── crc16.c / crc16.h
│   ├── mesh.c / mesh.h
│   └── protocol.c / protocol.h
├── hub/
├── dirt-sentinel/
├── dock-controller/
└── surface-wand/
```

Each node contains a standalone `main.c` plus a `platformio.ini` showing the intended MCU family.

---

## Hardware BOMs

Detailed BOMs live in [`hardware/bom/`](hardware/bom/).

- `hub_bom.csv`
- `dirt_sentinel_bom.csv`
- `dock_controller_bom.csv`
- `surface_wand_bom.csv`

---

## Software Stack

### Backend (`software/dashboard/`)

- FastAPI REST + WebSocket API
- MQTT ingest for node telemetry
- in-memory fallback store for offline development
- cleanliness scoring, scheduling recommendations, alert generation

### ML pipeline (`software/ml-pipeline/`)

- synthetic dataset generators
- scikit-learn training scripts
- artifact export to `artifacts/`

### Mobile app (`software/mobile-app/`)

- React Native dashboard
- room cleanliness heatmap
- scan history + cleaning verification
- supply forecasts and quiet-hour scheduling

---

## Example Deployment Topology

| Space | Recommended nodes |
|------|--------------------|
| Small apartment | 1 hub, 2 dirt sentinels, 1 dock controller, 1 surface wand |
| Family home | 1 hub, 5-8 dirt sentinels, 1 dock controller, 1-2 surface wands |
| Short-term rental | 1 hub, 4 dirt sentinels, 1 dock controller, 1 surface wand with cleaner verification workflows |
| Assisted living suite | 1 hub, 3 dirt sentinels, 1 dock controller, 1 surface wand for caregiver hygiene rounds |

---

## Safety Notes

- UV-C sanitation only runs with enclosure interlock closed and occupancy clear.
- Dock pumps use float-switch and leak-electrode redundancy.
- Surface wand UV mode is low-power UV-A fluorescence inspection, not germicidal exposure.
- No raw room audio is captured anywhere in the system.
- Camera imagery from the wand remains local by default unless the user opts into cloud dataset contribution.

---

## Build / Run Quick Start

```bash
cd software/dashboard
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
uvicorn main:app --reload
```

```bash
cd software/ml-pipeline
python3 train_dirtcast.py
python3 train_grimenet.py
```

---

## Repository Layout

```text
CleanSync/
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

## Future Expansion Ideas

- under-sink graywater reclaim cartridge for mop refill
- drawer-mounted cutlery UV inspection dock
- integration with dishwasher and laundry systems
- commercial cleaning supervisor mode for offices and clinics

CleanSync is designed to make the clean house *default*, not another manual project.

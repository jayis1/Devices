# WasteSort

**AI-powered household recycling, composting, and landfill diversion system** — a multi-node home waste intelligence platform that identifies what you are throwing away, routes it to the correct stream, forecasts bin fill and odor events, verifies curb pickup, and teaches households how to reduce contamination and waste over time.

## What It Solves

Households want to recycle correctly, but the system around them is confusing:

- **Recycling rules are inconsistent** — what is recyclable in one city is contamination in another. Families guess, contaminate bins, and entire truckloads get downgraded to landfill.
- **Food scraps and wet waste smell fast** — people delay composting or overfill kitchen bins because odor and fruit flies make the experience unpleasant.
- **Overflow happens at the worst time** — bins fill faster around holidays, parties, move-outs, and school weeks, but most homes have no data on volume trends.
- **Municipal pickup is easy to miss** — if the curb bin is not placed out on time, households wait another week and end up with overflowing trash.
- **Behavior change is hard without feedback** — people do not know which products create the most waste or which habits would meaningfully reduce landfill volume.

**WasteSort** treats household waste as a measurable, optimizable system. It scans items before disposal, classifies materials using RGB + NIR + barcode fusion, lights the correct bin path, monitors fill level/odor/weight across indoor bins, verifies curb pickup on outdoor bins, predicts when each stream will be full, and coaches households toward higher diversion and lower contamination.

---

## System Architecture

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│                           WasteSort Cloud / Edge                            │
│ FastAPI + MQTT + PostgreSQL + object storage + ML pipeline                  │
│                                                                              │
│ Models                                                                       │
│ • SortNet CNN          - RGB+NIR household material classification           │
│ • ContamNet XGBoost    - contamination probability by municipality profile   │
│ • FillCast LSTM        - 1-7 day per-bin fill forecast                       │
│ • PickupPulse RF       - missed-pickup / overflow risk predictor             │
│ • HabitCoach Bandit    - personalized diversion interventions                │
└───────────────────────────────┬──────────────────────────────────────────────┘
                                │ MQTT over TLS / HTTPS
                                │
                    ┌───────────┴────────────────┐
                    │   WasteSort Hub Gateway     │
                    │   Raspberry Pi CM4 + RP2040 │
                    │   SX1262 coordinator        │
                    │   local rules cache         │
                    └──────┬───────────────┬──────┘
                           │               │
      Sub-GHz 868 MHz TDMA │               │ BLE / Wi-Fi commissioning
                           │               │
      ┌────────────────────┼───────────────┼────────────────────┐
      │                    │               │                    │
┌─────┴──────────┐  ┌──────┴─────────┐  ┌──┴──────────────┐  ┌──┴─────────────┐
│ Countertop     │  │ Bin Dock Node  │  │ Bin Dock Node   │  │ Outdoor Pickup │
│ Sorter         │  │ (Recycling)    │  │ (Compost/Trash) │  │ Beacon         │
│ camera+NIR     │  │ fill+odor+mass │  │ fill+odor+mass  │  │ curb verify    │
│ servo diverter │  │ e-paper label  │  │ UV deodorizer   │  │ e-paper agenda │
└────────────────┘  └────────────────┘  └─────────────────┘  └────────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **WasteSort Hub Gateway** | Raspberry Pi CM4 + RP2040 + SX1262 | Local rule engine, MQTT bridge, dashboard host, OTA orchestrator, edge inference cache | 12V/3A wall input + UPS HAT | Wi-Fi, Ethernet, BLE 5.0, Sub-GHz 868 MHz |
| **Countertop Sorter** | ESP32-S3-WROOM-1 + OV2640 + AS7341 | Item recognition, barcode scan, local UX, servo diverter / bin guidance | 12V DC + 5V/3.3V rails | Wi-Fi for setup, Sub-GHz 868 MHz |
| **Bin Dock Node** | STM32WL55JC | Fill level, load cell mass, lid-open events, VOC/odor sensing, deodorizer / status display | 2×18650 or 12V dock input | Integrated Sub-GHz 868 MHz |
| **Outdoor Pickup Beacon** | nRF52840 + SX1262 | Pickup-day reminder, curb placement verification, pickup event sensing, anti-tip alert | LiFePO4 32700 + small solar panel | BLE 5.0, Sub-GHz 868 MHz |

---

## Daily User Experience

1. A user approaches the countertop sorter with a yogurt cup.
2. The sorter reads barcode + image + spectral response and applies the municipality profile.
3. The chute ring glows **blue for recycle**, and the voice prompt says: **"Rinse lightly, lid on, recycle."**
4. The recycle bin dock records added mass and fill delta.
5. Overnight, FillCast updates a three-day overflow forecast and HabitCoach notices heavy single-serve plastic use.
6. The mobile app suggests a lower-waste alternative and warns that recycling will hit 92% fill before pickup.
7. On pickup morning, the outdoor beacon flashes amber until the curb bin is wheeled out.
8. After the truck empties the bin, the beacon detects lift-and-drop motion and the app marks service complete automatically.

---

## Node 1 - WasteSort Hub Gateway

### Core hardware

- **Compute module:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for watchdog, deterministic radio scheduling, and safe-power sequencing
- **Sub-GHz radio:** Semtech SX1262 at 868 MHz with +22 dBm PA path
- **Connectivity:** Gigabit Ethernet PHY, CYW43455 Wi-Fi/BLE on CM4, USB-C service port
- **Local UX:** 5 inch 800x480 DSI touchscreen, status buzzer, RGB tower LED
- **Storage:** industrial microSD for logs + eMMC rootfs on CM4
- **Power:** 12V input -> 5V/5A buck for CM4 peripherals -> 3V3 buck for radio/MCU; UPS HAT with two 18650 cells for 2 hours ride-through

### Major responsibilities

- Maintains municipal recycling rules and per-material contamination policies
- Runs FastAPI backend and MQTT broker bridge
- Schedules TDMA slots for sorter, indoor bins, and outdoor beacon
- Caches edge models for low-latency item classification and fallback operation during internet outages
- Performs OTA manifest distribution, telemetry compression, and household analytics

### Hub pin / interface map

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 I2C0 | INA219 power monitor, RTC, buzzer driver |
| RP2040 UART0 | CM4 console / heartbeat exchange |
| CM4 CSI | optional service camera |
| CM4 DSI | 5 inch touch display |
| CM4 USB2 | barcode provisioning dongle / backup export |
| CM4 Ethernet | router uplink |

---

## Node 2 - Countertop Sorter

### Hardware architecture

- **SoC:** ESP32-S3-WROOM-1-N16R8
- **Camera:** OV2640 2 MP module for visual classification and contamination inspection
- **Spectral sensor:** AMS AS7341, 8 visible + NIR channels
- **Barcode engine:** GM65 1D/2D serial scanner
- **Distance sensor:** VL53L1X ToF for object presence and chute occupancy
- **Actuation:** MG996R metal-gear servo for diverter flap; NEMA 14 stepper optional for premium rotary drum version
- **UX:** 1.69 inch ST7789 color display, WS2812B LED ring, piezo buzzer, capacitive confirm/cancel pads
- **Safety:** hall sensor on access door, current-limited servo rail, e-stop tact switch

### Mechanical behavior

- Funnel accepts hand-thrown items up to 110 x 110 x 180 mm
- Diverter chooses one of three output paths: recycle, compost, landfill
- If confidence is low, item is held and user gets a picture + question flow in the app

### Example rules

- Oily pizza box -> compost if municipal food-soiled fiber allowed, otherwise landfill
- Black plastic tray -> landfill in most optical-sorter constrained cities
- Aluminum can -> recycle, high-confidence path with deposit/refund tracking
- PLA cup -> compost only if industrial compost accepted locally; else landfill

---

## Node 3 - Bin Dock Node

A Bin Dock is placed beneath or behind each waste stream bin. Households normally use 3-4: **recycling**, **compost**, **landfill**, and optional **glass/deposit**.

### Sensors and actuators

- **MCU/radio:** STM32WL55JC (integrated Cortex-M4 + Sub-GHz)
- **Fill sensor:** VL53L1X top-mounted ToF module
- **Mass sensing:** 50 kg single-point load cell + HX711 ADC
- **Odor sensing:** Sensirion SGP41 VOC index sensor
- **Environment:** SHT31 temp/humidity for odor and spoilage context
- **Lid state:** reed switch + magnet
- **Display:** 2.13 inch tri-color e-paper for stream label + fill percent + contamination status
- **Actuation:** 5V blower + UVC-C LED chamber for compost deodorization in premium dock

### Power architecture

- 12V docking brick input for kitchens that want active deodorization
- Fallback battery mode: dual protected 18650 cells with MP2615 charger / load sharing
- 3V3 rail via TPS62162 buck, isolated analog ground for HX711 front end

---

## Node 4 - Outdoor Pickup Beacon

### Why it exists

Indoor intelligence is not enough if the outdoor cart never reaches the curb. The Pickup Beacon attaches magnetically to the large municipal cart or wheelie bin and closes the loop.

### Hardware

- **SoC:** nRF52840 for BLE commissioning and low-power motion logic
- **Sub-GHz radio:** SX1262 for long-range yard-to-home connectivity
- **Motion:** LIS2DW12 accelerometer for curb-roll and truck-lift signatures
- **Tilt safety:** BMA400 for tip-over / high-wind alert redundancy
- **Display:** 2.9 inch e-paper with next pickup date and service status
- **Position awareness:** Hall sensor in dock clip to detect curb-stand magnet
- **Power:** 32700 LiFePO4 cell + 3W solar panel + CN3791 charger
- **Enclosure:** UV-stable IP66 polycarbonate with stainless strap mount

### Event logic

- **Curb placement detected** when sustained rolling motion is followed by stable outdoor tilt angle
- **Pickup confirmed** when lift impulse + inversion + drop sequence matches truck-empty profile
- **Missed pickup alert** when cart remains at home geofence after scheduled curb deadline

---

## Network and Protocol

WasteSort uses a **deterministic 868 MHz TDMA mesh** for reliability through walls, metal bins, and outdoor placement.

- **Beacon interval:** 60 s nominal, 5 s during active sorting or pickup window
- **Payload max:** 48 bytes
- **Frame protection:** CRC-16/CCITT
- **AES-128 CTR encryption** on application payloads
- **Node addressing:** 16-bit node IDs, household-scoped network key, rotating session nonce
- **Fallback:** BLE 5.0 direct commissioning for the mobile app when installing new nodes

See [`docs/protocol_spec.md`](docs/protocol_spec.md) and [`firmware/common/protocol.h`](firmware/common/protocol.h).

---

## ML Stack

### 1. SortNet - material + object classifier
Inputs:
- RGB crop from OV2640
- AS7341 spectral vector
- barcode metadata if available
- municipality rule profile embedding

Outputs:
- stream recommendation: recycle / compost / landfill / special drop-off / deposit return
- material class: PET, HDPE, PP, aluminum, steel, paperboard, glass, food waste, e-waste, film, textile, etc.
- confidence score

### 2. ContamNet
Gradient-boosted classifier that estimates contamination risk from:
- residue visibility score
- household historical behavior
- local municipality rules
- item category / material

### 3. FillCast
LSTM forecasting per-bin fill % and mass over 1-7 days using:
- historical disposal events
- day-of-week seasonality
- holiday / event flags
- household size

### 4. PickupPulse
Random forest for missed-pickup and overflow risk using:
- fill forecast
- curb placement history
- weather and wind
- municipal pickup schedule reliability

### 5. HabitCoach
Contextual bandit that selects which intervention to show:
- refill reminder
- contamination tip
- bulk-buy recommendation
- pickup reminder timing
- compost deodorization suggestion

---

## Repository Layout

```text
WasteSort/
├── README.md
├── schematic/
│   ├── README.md
│   ├── hub/hub.sch
│   ├── countertop-sorter/countertop-sorter.sch
│   ├── bin-dock/bin-dock.sch
│   └── pickup-beacon/pickup-beacon.sch
├── firmware/
│   ├── common/
│   ├── hub/
│   ├── countertop-sorter/
│   ├── bin-dock/
│   └── pickup-beacon/
├── hardware/bom/
├── software/
│   ├── dashboard/
│   ├── ml-pipeline/
│   └── mobile-app/
├── docs/
└── scripts/
```

---

## Bill of Materials Summary

| Node | Estimated build cost |
|------|----------------------|
| Hub Gateway | $149.60 |
| Countertop Sorter | $82.20 |
| Bin Dock Node | $58.05 each |
| Outdoor Pickup Beacon | $49.85 |

A three-bin starter kit with hub + sorter + 3 docks + beacon lands around **$456-$520 BOM**, depending on touchscreen, enclosure, and whether active compost deodorization is populated.

---

## Safety and Privacy Design

- No raw household images leave the home unless the user explicitly enables cloud review for uncertain items
- Municipal rule packs are cached locally so sorting works offline
- UVC deodorization is enclosed, interlocked, and disabled when lid or service door is open
- All pickup alerts are advisory only — no actuation on municipal infrastructure
- Bin fill predictions use aggregation; personally identifiable shopping behavior is not required

---

## Build / Verification Notes

- Python backend validated with `python3 -m py_compile`
- C firmware sources validated with `gcc -fsyntax-only`
- Mobile app is a React Native scaffold with core screens and WebSocket service stubs
- Schematics are provided as KiCad-style text starter files suitable for expansion into full board projects

---

## Next Hardware Iterations

- Add **deposit-value OCR** for bottle/can refund automation
- Add **food-waste vacuum transfer** for larger households
- Add **apartment chute mode** for multifamily buildings
- Add **municipal analytics API** for anonymized contamination heatmaps

WasteSort is designed to make correct disposal the easiest disposal path.

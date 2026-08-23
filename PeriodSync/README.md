# PeriodSync

**AI-powered menstrual health, pain relief, leak prevention, and cycle forecasting system** — a privacy-first multi-node home + wearable platform that continuously models cycle phase, flow intensity, cramps, sleep disruption, hydration, and hormone test trends so people get practical support before symptoms derail the day.

## What It Solves

Millions of people manage menstruation with too little real-time information:

- **Leak anxiety is constant** during school, work, exercise, and sleep.
- **Cramping and fatigue arrive before people can prepare** with heat, hydration, or schedule adjustments.
- **Cycle irregularity is hard to track accurately** when symptoms, temperature, sleep, and hormone strips live in separate apps.
- **Heavy bleeding can hide meaningful health risk** such as iron depletion, fibroids, or endometriosis patterns.
- **Partner/caregiver/clinician context is poor** because the underlying telemetry is fragmented and inconsistent.

**PeriodSync** coordinates a bedside or bathroom hub, a skin-worn physiology patch, a leak-risk flow clip, a smart pelvic relief belt, and an optical strip reader. The system builds a day-by-day model of cycle state, symptom burden, and intervention effectiveness. It can pre-warm a relief belt before cramps peak, estimate when a product change is needed, forecast heavy-flow windows, remind for hydration and iron-rich meals, and assemble clinician-ready trend reports.

---

## System Architecture

```text
┌──────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 PeriodSync Cloud / Edge                                     │
│ FastAPI + MQTT + PostgreSQL + object storage + model registry                               │
│                                                                                              │
│ Models                                                                                       │
│ • PhaseCast TFT        - cycle phase + ovulation / period onset forecast                    │
│ • FlowGuard XGBoost    - next 2 h flow intensity / leak-risk forecast                       │
│ • CrampAhead LSTM      - 30/60/120 min cramp severity forecast                              │
│ • IronWatch GBDT       - heavy-cycle / fatigue / low-iron risk scoring                      │
│ • ReliefTune Bandit    - learns best heat / vibration / timing protocol                     │
└──────────────────────────────────────┬───────────────────────────────────────────────────────┘
                                       │ HTTPS / MQTT over TLS / WebSocket
                                       │
                         ┌─────────────┴────────────────────────┐
                         │ PeriodSync Home Hub Gateway          │
                         │ Raspberry Pi CM4 + RP2040 + SX1262  │
                         │ offline rules + OTA + local privacy  │
                         └───────┬───────────────┬──────────────┘
                                 │               │
                 868 MHz TDMA mesh + BLE 5.x     Wi-Fi provisioning / mobile app
                                 │
      ┌──────────────────────────┼──────────────────────────┬───────────────────────────┬──────────────────────┐
      │                          │                          │                           │                      │
┌─────┴────────────┐    ┌────────┴──────────┐     ┌─────────┴─────────┐       ┌─────────┴─────────┐  ┌─────┴──────────┐
│ TempPatch        │    │ FlowClip           │     │ Relief Belt       │       │ Strip Reader Dock  │  │ Mobile App     │
│ skin temp+PPG    │    │ capacitive leak    │     │ heat+vibration     │       │ LH/FSH/hCG strip   │  │ coaching+alerts│
│ HRV+motion       │    │ risk+orientation   │     │ adaptive protocol  │       │ spectral reader    │  │ reports        │
└──────────────────┘    └────────────────────┘     └───────────────────┘       └───────────────────┘  └────────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **PeriodSync Home Hub** | Raspberry Pi CM4 + RP2040 + SX1262 | Local orchestration, MQTT bridge, data retention, OTA, audio/visual prompts | 12 V / 4 A wall input + dual 18650 UPS | Wi-Fi, Ethernet, BLE 5.0, Sub-GHz 868 MHz |
| **TempPatch** | nRF52840 + MAX86141 + TMP117 | Continuous skin temp, HR/HRV, sleep fragmentation, activity, symptom button | 180 mAh LiPo | BLE 5.2, Sub-GHz 868 MHz |
| **FlowClip** | nRF5340 + AD7746 + SHTC3 + BMA400 | Pad/garment saturation trend, leak-risk estimation, posture-aware overnight protection | CR2450 or 250 mAh LiPo | BLE 5.3, Sub-GHz 868 MHz |
| **Relief Belt** | STM32WB55 + DRV2605L + heater MOSFET stage | Adaptive abdominal heat and haptic cramp relief | 2000 mAh LiPo | BLE 5.0, Sub-GHz 868 MHz |
| **Strip Reader Dock** | ESP32-S3-WROOM-1 + AS7341 + OV2640 | Reads LH/FSH/hCG/pH strips, symptom journaling, lighting-calibrated colorimetry | USB-C 5 V | Wi-Fi for setup, BLE, Sub-GHz 868 MHz |

---

## Experience Flow

1. **TempPatch** captures overnight skin temperature trend, resting HR, HRV, sleep interruptions, and a manual pain button press if cramps wake the user.
2. **FlowClip** estimates saturation progression using textile capacitance and humidity, then predicts how long remains before leak risk crosses the user’s configured threshold.
3. The **Home Hub** fuses physiology, symptom history, flow pace, and calendar data to estimate cycle phase and the next likely heavy-flow or cramp window.
4. Thirty minutes before a predicted cramp spike, the **Relief Belt** can pre-warm to a clinician-safe temperature envelope and prompt a hydration or NSAID reminder if enabled.
5. The **Strip Reader Dock** quantifies LH/FSH/hCG or pH strip intensity under calibrated LEDs and reconciles those results with the phase model.
6. The mobile app shows practical guidance: “Heavy-flow window likely 7:10–10:30 AM”, “change product within 35 min”, “heat protocol A reduced pain 28% last cycle”, or “persistent heavy cycles + fatigue pattern warrants clinician review.”

---

## Node 1 - PeriodSync Home Hub

### Hardware

- **Main compute:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for watchdog, radio timing, safe-OTA rollback, and offline fail-safe scenes
- **Mesh radio:** Semtech **SX1262** with matched 868 MHz antenna network
- **Local UX:** 5 inch capacitive display, MAX98357A I2S speaker, RGB status strip, haptic enable button
- **Timekeeping & health:** DS3231 RTC, INA219 power monitor
- **Power:** 12 V input -> 5 V buck for CM4/display -> 3.3 V buck for MCU/radio; dual-18650 UPS HAT for outages

### Responsibilities

- Maintains per-user symptom policies, quiet hours, cycle goals, and clinician-sharing preferences
- Bridges radio/BLE telemetry to MQTT and local WebSocket sessions
- Runs local inference when cloud is unavailable
- Stores 90 days of edge-resident raw summaries so sensitive data can remain home-local by default
- Handles OTA manifests and calibration coefficients for each node

### Hub interfaces

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 mesh radio |
| RP2040 I2C0 | DS3231, INA219, PCA9633 |
| RP2040 UART0 | CM4 heartbeat / watchdog |
| CM4 I2S | MAX98357A amplifier |
| CM4 DSI | 5 inch local touchscreen |
| CM4 Ethernet | router uplink / PoE splitter |

---

## Node 2 - TempPatch

A reusable skin-worn patch intended for lower abdomen, flank, or upper arm wear depending comfort.

### Electronics

- **MCU:** Nordic **nRF52840**
- **PPG:** Analog Devices **MAX86141** for heart rate and HRV trend features
- **Temperature:** TI **TMP117** (±0.1 °C)
- **Motion:** ST **LIS2DW12** low-power accelerometer
- **Haptics:** **DRV2605L** for gentle reminders without waking partners
- **Charging:** **BQ24074** single-cell LiPo charger with power path
- **Optional skin contact sensing:** resistive dual-pad contact check via ADC

### Derived features

- Overnight basal skin temperature drift
- Resting HR and vagal-recovery HRV trend
- Sleep interruption count around predicted symptom windows
- Pain-marker correlation after user button taps
- Activity-adjusted phase confidence score

### Power architecture

- 180 mAh curved LiPo
- 3.3 V buck-boost rail for sensor stability at low state-of-charge
- 5-minute summaries locally cached; raw PPG not retained unless clinical export is enabled

---

## Node 3 - FlowClip

A snap-on clip that attaches to underwear, reusable pad, or period sleep short. It never touches the body directly; it senses moisture migration and orientation near the outer absorbent layer.

### Electronics

- **MCU:** Nordic **nRF5340**
- **Capacitance front-end:** Analog Devices **AD7746** for high-resolution textile capacitance change tracking
- **Humidity:** Sensirion **SHTC3**
- **Motion/orientation:** Bosch **BMA400**
- **Indicators:** low-power RGB LED + acknowledge button
- **Power:** CR2450 holder or optional 250 mAh LiPo with **MCP73831** charger

### Logic

- Uses baseline dry calibration for each garment/product type
- Combines capacitance slope + humidity + posture to estimate remaining absorbency margin
- Detects high overnight back-sleep leak risk vs upright daytime use
- Marks “change product soon”, “urgent change”, and “possible leak event” states

---

## Node 4 - Relief Belt

A soft abdominal / lower-back belt with closed-loop heat and haptic actuation.

### Electronics

- **MCU:** ST **STM32WB55RG**
- **Haptic driver:** TI **DRV2605L** driving 2x LRA zones
- **Heating:** 2x carbon-fiber heater pads switched by AO3400A MOSFETs with PWM
- **Temperature safety:** 2x **TMP117** plus 1x NTC divider for redundant overtemp cutout
- **Battery fuel gauge:** MAX17048
- **Charging:** USB-C with **BQ25895** buck charger
- **Safety:** hardware thermal fuse in series with heater rail

### Control modes

- Warmup (38–40 °C)
- Active relief (40–43 °C bounded by skin-safe timer and sensor feedback)
- Night comfort (pulsed 37–39 °C + low haptics)
- Cooldown / lockout on any sensor disagreement or prolonged session

---

## Node 5 - Strip Reader Dock

A countertop dock that standardizes home strip measurements with controlled optical geometry.

### Electronics

- **SoC:** Espressif **ESP32-S3-WROOM-1-N8R8**
- **Spectral sensor:** AMS **AS7341** 11-channel visible/NIR sensor
- **Imager:** **OV2640** for strip localization and result archiving
- **Illumination:** 525 nm, 590 nm, 630 nm, and white LEDs with constant-current drivers
- **Presence:** slot microswitch + Hall sensor for tray detection
- **User I/O:** 2.13 inch e-paper, buzzer, capacitive touch button
- **Power:** USB-C 5 V with AP63203 3V3 buck

### Use cases

- LH surge test quantification for fertility awareness
- FSH trend support for perimenopause conversations
- hCG progression logging when desired
- Vaginal pH strip reading for infection-context journaling (not diagnosis)

---

## Communication and Protocol

PeriodSync uses **868 MHz TDMA mesh** for deterministic low-power indoor coverage and **BLE 5.x** for provisioning and direct phone sync.

- **Frame size:** 48-byte payload, 64-byte over-the-air max
- **Encryption:** AES-128 CTR with rotating per-session nonce
- **Integrity:** CRC-16/CCITT
- **Addressing:** 16-bit household-scoped node IDs
- **QoS classes:** telemetry, alert, command, ack, bulk-sync
- **Privacy mode:** raw PPG and strip image upload disabled unless opt-in

See [`docs/protocol_spec.md`](docs/protocol_spec.md) and [`firmware/common/protocol.h`](firmware/common/protocol.h).

---

## ML Stack

### 1. PhaseCast
Forecasts cycle phase confidence, period onset, and fertile window using:
- temperature trend
- HRV trend
- symptom tags
- hormone strip readings
- prior cycle variability

### 2. FlowGuard
Predicts 30/60/120 min leak risk and product change urgency using:
- FlowClip capacitance slope
- garment/product profile
- posture state
- cycle day
- prior heavy-flow patterns

### 3. CrampAhead
Sequence model that predicts near-term pain severity from:
- pain button presses
- HRV suppression
- temperature drift
- flow intensity
- relief protocol history

### 4. IronWatch
Gradient-boosted model that scores when heavy cycles + fatigue + elevated resting HR patterns suggest low-iron risk worth discussing with a clinician.

### 5. ReliefTune
Contextual bandit that learns which heat/haptic routine best reduces pain while minimizing battery drain and overheating complaints.

---

## Software Stack

### Backend
- **FastAPI** REST + WebSocket API
- **MQTT** ingestion for mesh uplink payloads
- **PostgreSQL** for users, telemetry, symptom events, model outputs
- **Object storage** for optional strip images and PDF exports
- **Prometheus-ready** health endpoint

### Mobile app
- **React Native** caregiver/user app
- live dashboard, cycle calendar, leak-risk timeline, relief controls, clinician report export

### Edge services
- local rule engine on the hub
- OTA manifest server
- on-hub encrypted cache

---

## Firmware Layout

```text
firmware/
├── common/
│   ├── crc16.c / crc16.h
│   ├── protocol.c / protocol.h
│   └── mesh.c / mesh.h
├── hub/
├── temp-patch/
├── flow-clip/
├── relief-belt/
└── strip-reader/
```

Each node publishes summarized telemetry, alert state, battery level, and calibration metadata. Shared message definitions live in `firmware/common/protocol.h`.

---

## Power Architecture Summary

| Node | Primary power | Regulation | Runtime target |
|------|---------------|-----------|----------------|
| Hub | 12 V wall adapter + 2x18650 UPS | 5 V / 3.3 V bucks | 24/7 |
| TempPatch | 180 mAh LiPo | 3.3 V buck-boost | 36 h |
| FlowClip | CR2450 or 250 mAh LiPo | direct + LDO | 30 days coin cell, 5 days LiPo |
| Relief Belt | 2000 mAh LiPo | heater rail + 3.3 V buck | 6 heated sessions/day |
| Strip Reader Dock | USB-C 5 V | 3.3 V buck | desktop continuous |

---

## Safety and Privacy

- Heat control requires dual-sensor agreement and hard thermal fuse backup.
- PeriodSync does **not** diagnose pregnancy, infertility, anemia, infection, endometriosis, or other medical conditions; it highlights patterns worth discussing with professionals.
- Strip images and raw physiology are opt-in.
- Device sharing is role-based: user, partner, caregiver, clinician export-only.
- Local-only mode is fully supported.

---

## Repository Structure

```text
PeriodSync/
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

## Build and Validation

1. Create a Python virtual environment and install dashboard / ML requirements.
2. Run the synthetic-data training scripts to generate starter models.
3. Launch the FastAPI backend locally.
4. Flash node firmware with PlatformIO or vendor SDK after adapting HAL functions.
5. Calibrate TempPatch baseline, FlowClip dry-state, and Relief Belt thermal offsets with `scripts/calibrate.py`.

---

## BOM Index

- [`hardware/bom/hub_bom.csv`](hardware/bom/hub_bom.csv)
- [`hardware/bom/temp_patch_bom.csv`](hardware/bom/temp_patch_bom.csv)
- [`hardware/bom/flow_clip_bom.csv`](hardware/bom/flow_clip_bom.csv)
- [`hardware/bom/relief_belt_bom.csv`](hardware/bom/relief_belt_bom.csv)
- [`hardware/bom/strip_reader_bom.csv`](hardware/bom/strip_reader_bom.csv)

---

## Next Build Steps

- Replace generic radio HAL stubs with target board SPI/GPIO drivers
- Add KiCad footprints and board outlines around the provided pin maps
- Collect real pilot data to retrain the starter models
- Integrate the mobile app with real auth, BLE commissioning, and report export

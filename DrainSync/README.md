# DrainSync

**AI-powered home drain health, sewer-gas detection, and backup prevention system** — a multi-node plumbing intelligence platform that continuously monitors kitchen, bathroom, laundry, and main sewer lines; predicts clogs before they happen; prevents trap dry-out and sewer-gas exposure; and automatically closes a backwater valve before a costly backup reaches the home.

## What It Solves

Everyday drain problems are expensive, unsanitary, and usually detected too late:

- **Kitchen sink grease slowly accumulates** until drainage becomes sluggish or stops entirely.
- **Shower and bathroom drains** trap hair and soap scum, causing overflow at the worst moment.
- **Basement floor drains and utility drains** can dry out and leak sewer gas into living spaces.
- **Municipal surcharges, storms, and sewer blockages** can force wastewater backward into homes.
- **Drain failures are mostly invisible** until odor, gurgling, flooding, or property damage appears.

**DrainSync** turns residential drain plumbing into an observable system. It combines acoustic flow signatures, vibration sensing, trap-water monitoring, sewer-gas sensing, line pressure, cleanout-level sensing, and motorized backwater control into a coordinated stack spanning edge firmware, a FastAPI/MQTT backend, ML forecasting, and a React Native mobile app.

---

## System Architecture

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                         DrainSync Cloud / Edge Stack                        │
│ FastAPI + MQTT + SQLite/PostgreSQL + ML artifact store + alert engine      │
│                                                                             │
│ Models                                                                      │
│ • FlowPrint 1D-CNN   - drain acoustic/vibration event classification        │
│ • ClogCast LSTM      - 1-14 day clog / slow-drain forecast                  │
│ • OdorNet XGBoost    - sewer-gas / dry-trap / biofilm source attribution    │
│ • BackupRisk TFT     - storm + line pressure + cleanout trend forecast      │
│ • PrimerPolicy Bandit- low-water trap primer scheduling                     │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │ MQTT over TLS / HTTPS
                                │
                   ┌────────────┴───────────────────────┐
                   │ DrainSync Hub Gateway              │
                   │ Raspberry Pi CM4 + RP2040 + SX1262 │
                   │ Local rules, OTA, edge inference   │
                   └───────┬──────────────┬─────────────┘
                           │              │
         Sub-GHz 868 MHz TDMA mesh        │ BLE/Wi-Fi commissioning
                           │              │
      ┌────────────────────┼──────────────┼─────────────────────┬──────────────┐
      │                    │              │                     │              │
┌─────┴──────────┐  ┌──────┴────────┐  ┌──┴──────────────┐  ┌───┴──────────┐  ┌──┴──────────────┐
│ Under-Sink     │  │ Floor Drain   │  │ Main Stack      │  │ Backwater     │  │ Mobile App /    │
│ Sentinel ×N    │  │ Guard ×M      │  │ Monitor         │  │ Actuator      │  │ Web Dashboard   │
│ flow + odor    │  │ dry trap + H2S│  │ cleanout level  │  │ motorized gate│  │ alerts + control│
│ + vibration    │  │ + primer valve│  │ + pressure      │  │ + safety logic│  │                 │
└────────────────┘  └───────────────┘  └─────────────────┘  └────────────────┘  └────────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **DrainSync Hub Gateway** | Raspberry Pi CM4 + RP2040 + SX1262 | Local coordinator, edge inference host, MQTT bridge, OTA, alarm policy | 12V/3A wall adapter + LiFePO4 UPS HAT | Ethernet, Wi‑Fi, BLE 5.0, Sub-GHz 868 MHz |
| **Under-Sink Sentinel** | ESP32-S3-WROOM-1 + SX1262 | Acoustic/vibration flow fingerprinting, humidity/leak sensing, odor trend sensing | 12V under-sink adapter or USB-C PD trigger | Wi‑Fi setup, Sub-GHz 868 MHz |
| **Floor Drain Guard** | STM32WL55JC | Trap-water depth sensing, H2S/VOC warning, automatic trap primer pulse | 4×AA LiFeS2 or 12V utility supply | Integrated Sub-GHz 868 MHz |
| **Main Stack Monitor** | nRF52840 + SX1262 | Cleanout level, differential pressure, storm backup risk, service guidance | 2×18650 + optional solar trickle | BLE 5.0, Sub-GHz 868 MHz |
| **Backwater Actuator** | STM32G474RE + SX1262 | Motorized normally-closed backwater valve control with encoder and torque protection | 24V DIN rail supply + 6Ah LiFePO4 backup | RS-485 local, Sub-GHz 868 MHz |

---

## Daily User Experience

1. DrainSync learns the normal weekday profile of the kitchen, dishwasher, shower, and laundry drains.
2. The under-sink sentinel notices a rising turbulence signature and longer drain-down after dishwashing.
3. ClogCast predicts an **83% chance of a kitchen branch clog in 5 days**.
4. The app recommends a non-caustic maintenance cycle and schedules a reminder after dinner.
5. During a storm, the main stack monitor sees pressure pulses, rising cleanout level, and municipal risk data.
6. BackupRisk moves from green to red and the backwater actuator pre-arms.
7. When reverse-flow pressure crosses threshold, the actuator closes the valve in under 1.5 s and alerts the household.
8. A basement floor drain that has gone dry gets a 300 mL primer pulse automatically, preventing sewer-gas odor.

---

## Node 1 — DrainSync Hub Gateway

### Core hardware

- **Compute:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for watchdog, radio slot timing, hard alarm path, and brownout-safe shutdown
- **Radio:** Semtech SX1262 on SPI with +22 dBm PA path
- **UPS:** 4-cell LiFePO4 HAT with INA219 rail monitor and RTC
- **Local UX:** 4.3 inch capacitive touchscreen, RGB stack LED, piezo buzzer, service USB-C

### Responsibilities

- Maintains the household drain topology map and node registry
- Runs FastAPI backend, local MQTT broker bridge, notification fanout, and OTA catalog
- Executes edge inference when the internet is unavailable
- Stores recent waveforms, event summaries, and valve state transitions
- Performs multi-node consensus before closing the backwater valve

### Hub interface map

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 I2C0 | INA219, RTC, buzzer GPIO expander |
| RP2040 UART0 | CM4 heartbeat / failover channel |
| CM4 DSI | 4.3 inch dashboard panel |
| CM4 Ethernet | router uplink |
| CM4 USB2 | commissioning dongle / service export |

---

## Node 2 — Under-Sink Sentinel

Installed beneath kitchen or bathroom sinks and near dishwashers/laundry standpipes.

### Hardware architecture

- **MCU:** ESP32-S3-WROOM-1-N16R8
- **Sub-GHz radio:** SX1262 via SPI for long-range house coverage
- **Acoustic sensing:** ICS-43434 I²S MEMS microphone mounted to enclosure acoustic port
- **Pipe vibration:** ADXL355 precision accelerometer bonded to pipe clamp PCB wing
- **Drain temperature:** DS18B20 stainless probe on branch line to separate hot-water signatures
- **Ambient RH / leak:** SHT41 + capacitive leak rope input
- **Odor proxy:** Sensirion SGP41 VOC + raw gas index
- **Current sense:** INA219 for adapter health
- **UX:** tri-color LED, tactile service button

### Pin assignment

| Signal | ESP32-S3 pin | Peripheral |
|--------|--------------|------------|
| I2S_BCLK | GPIO4 | ICS-43434 |
| I2S_WS | GPIO5 | ICS-43434 |
| I2S_DIN | GPIO6 | ICS-43434 |
| SPI_MOSI | GPIO11 | SX1262 |
| SPI_MISO | GPIO13 | SX1262 |
| SPI_SCK | GPIO12 | SX1262 |
| SX1262_NSS | GPIO10 | SX1262 |
| SX1262_BUSY | GPIO9 | SX1262 |
| I2C_SDA | GPIO17 | SHT41 / INA219 / ADXL355 |
| I2C_SCL | GPIO18 | SHT41 / INA219 / ADXL355 |
| 1WIRE | GPIO16 | DS18B20 |
| LEAK_SENSE | GPIO7 | leak rope comparator |
| LED_R/G/B | GPIO35/36/37 | status LEDs |

### Power architecture

12V input → TPS54202 5V rail → AP2112K 3V3 rail. Audio and accelerometer run from filtered analog 3V3 island. Reverse-polarity and transient protection included for appliance cabinet installs.

---

## Node 3 — Floor Drain Guard

Designed for basements, utility rooms, mechanical closets, and rarely used guest bathrooms.

### Hardware

- **MCU/radio:** STM32WL55JC (integrated Cortex-M4 + Sub-GHz LoRa/FSK)
- **Trap-water depth:** capacitive concentric ring probe with FDC1004 capacitance ADC
- **Gas detection:** SPEC Sensors 3SP-H2S-50 electrochemical sensor with LMP91000 AFE
- **VOC / humidity:** SGP40 + SHT31
- **Primer valve:** 12V normally-closed solenoid delivering controlled water pulse from primer tee
- **Flow verify:** YF-S401 hall flow sensor on primer line
- **Water-on-floor safety:** gold-finger contact ring around drain lip

### Typical behavior

- Reports trap depth every 30 minutes
- Sends immediate alert when H2S exceeds threshold or trap depth collapses
- Opens primer valve for 2–5 seconds only when policy permits and water flow is confirmed
- Locks out repeated primer cycles to avoid unnoticed plumbing faults

---

## Node 4 — Main Stack Monitor

Mounted near the cleanout or building sewer entry point.

### Hardware

- **MCU:** nRF52840 for BLE provisioning and low-power sensor fusion
- **Sub-GHz radio:** SX1262 for long-range link to hub
- **Cleanout level:** JSN-SR04T waterproof ultrasonic level sensor aimed into clear cleanout sight tube
- **Differential pressure:** Honeywell ABP2 series ±2 psi transducer across stack/room reference
- **Temperature:** TMP117 for drift compensation
- **Vibration:** LIS2DW12 for main-line slug / hammer / truck-cleaning signatures
- **Battery:** dual 18650 pack with BQ24074 charger and ideal diode path

### Why it matters

The main stack node provides the earliest in-home signal of rising wastewater, reverse pressure, or repeated surges that precede a backup. It is the node the hub trusts most during storm-related events.

---

## Node 5 — Backwater Actuator

Installed on a normally-open backwater valve or gate valve assembly in the basement service line.

### Hardware

- **MCU:** STM32G474RE
- **Radio:** SX1262 + external whip antenna
- **Motor drive:** DRV8873 H-bridge driving 24V DC gearmotor
- **Valve position:** AS5600 magnetic encoder
- **Limit / jam sensing:** dual reed limit switches + INA240 current shunt amplifier
- **Manual override:** clutch knob + lockout key switch
- **Local bus:** isolated RS-485 port for plumber/service tool
- **Emergency power:** 6Ah LiFePO4 pack for 20+ closures during outage

### Safety logic

- Default policy is **fail-as-configured** based on plumbing code and installation geometry
- Closing requires either: 
  - main stack risk above critical and reverse-flow pressure, or
  - explicit user / service command
- Re-opening requires water-level recovery or service acknowledgement
- Every actuation is logged with current, travel time, and final position confidence

---

## Communications Protocol

DrainSync uses an **868 MHz deterministic TDMA mesh** tuned for basements, cabinets, and utility spaces.

- **Frame size:** 48-byte max payload
- **CRC:** CRC-16/CCITT
- **Encryption:** AES-128 CTR
- **Addressing:** 16-bit household node ID + 32-bit boot nonce
- **QoS:** command frames require ACK within 250 ms; critical valve commands are retried 5×
- **Discovery:** BLE commissioning from mobile app to hub or directly to portable nodes

See [`docs/protocol_spec.md`](docs/protocol_spec.md) and [`firmware/common/protocol.h`](firmware/common/protocol.h).

---

## ML Stack

### FlowPrint 1D-CNN
Classifies acoustic/vibration windows into:
- normal drain
- partial clog
- air-gurgle / venting issue
- garbage disposal event
- dishwasher discharge
- washing machine pump-out
- reverse-flow surge

### ClogCast LSTM
Forecasts 1–14 day clog probability from:
- flow duration trend
- drain-down slope
- turbulence score
- temperature-dependent grease risk
- household usage cadence

### OdorNet XGBoost
Infers likely odor source from:
- H2S / VOC / humidity profile
- trap-depth history
- floor-drain inactivity
- sink temperature spikes
- municipal storm / barometric inputs

### BackupRisk TFT
Forecasts sewer backup risk using:
- main stack level trend
- differential pressure pulses
- local rainfall / flood alerts
- historical storm response
- valve travel and health status

### PrimerPolicy contextual bandit
Optimizes when to prime rarely used traps to minimize odor risk while avoiding wasted water.

---

## Software Stack

```text
software/
├── dashboard/
│   ├── main.py              # FastAPI app + telemetry ingest + overview endpoints
│   ├── models.py            # Pydantic schemas
│   ├── ml_inference.py      # Lightweight local inference helpers
│   ├── requirements.txt     # Python dependencies
│   └── test_app.py          # API smoke tests
├── ml-pipeline/
│   ├── train_flowprint.py   # synthetic flow event classifier
│   ├── train_clogcast.py    # clog probability forecaster
│   └── train_backup_risk.py # backup classifier/regressor
└── mobile-app/
    ├── App.tsx              # React Native control app stub
    ├── package.json
    └── README.md
```

---

## BOM Files

Detailed BOM CSVs live in `hardware/bom/`:

- `hub_gateway_BOM.csv`
- `under_sink_sentinel_BOM.csv`
- `floor_drain_guard_BOM.csv`
- `main_stack_monitor_BOM.csv`
- `backwater_actuator_BOM.csv`

---

## Schematics

KiCad starter projects for each node live in `schematic/` and include named nets, power rails, connectors, and block-level placement references:

- `hub_gateway.kicad_sch`
- `under_sink_sentinel.kicad_sch`
- `floor_drain_guard.kicad_sch`
- `main_stack_monitor.kicad_sch`
- `backwater_actuator.kicad_sch`

---

## Build / Deployment Workflow

1. Manufacture the node PCBs from the schematic set.
2. Flash firmware in `firmware/*`.
3. Bring up the gateway with `software/dashboard`.
4. Pair nodes with the mobile app over BLE.
5. Run `scripts/calibrate.py` to store thresholds and topology metadata.
6. Run ML training scripts to generate initial artifacts for local inference.
7. Mount the backwater actuator only after manual plumber verification.

---

## Safety and Compliance Notes

- Not a substitute for local plumbing code review or licensed installation.
- Any automatic backwater valve deployment should be reviewed against municipal code and fixture configuration.
- H2S detection is treated as a life-safety feature and should be paired with conventional indoor air alarms where required.
- Solenoid primer lines must include backflow prevention.

---

## Repository Layout

```text
DrainSync/
├── README.md
├── schematic/
├── firmware/
├── hardware/bom/
├── software/
├── docs/
└── scripts/
```

DrainSync is designed to be buildable, serviceable, and useful from day one: prevent odors, reduce surprise clogs, and stop the nightmare scenario of sewage backing into the home.

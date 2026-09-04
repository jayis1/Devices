# WellSync

AI-powered private well water safety, treatment, and supply resilience system — continuously protects households that depend on wells by watching water chemistry, pump health, pressure stability, watershed contamination risk, and point-of-use treatment performance before bad water reaches a glass, shower, or appliance.

## What It Solves

Roughly tens of millions of people rely on private wells, yet most homes still discover problems too late:

- bacteria or runoff contamination after heavy rain;
- iron, manganese, sulfide, hardness, or pH drift that slowly damages plumbing and tastes awful;
- pressure-tank or pump failures that leave a family without water;
- UV sterilizer or filter failures that go unnoticed;
- seasonal drawdown that turns a marginal well into an outage risk;
- no unified record for testing, remediation, and long-term aquifer behavior.

WellSync turns a private well into a continuously supervised water utility. It combines inline chemistry, pump telemetry, under-sink treatment verification, and watershed sensing into one coordinated system with local fail-safe automation and cloud analytics.

## System Architecture

```text
[Watershed Weather Sentinel] --868 MHz-->                     ┌─────────────────────────────────────┐
[Inline Water Quality Node] --RS-485/868-->                  │ Well Hub Gateway                     │
[Pump & Pressure Controller] --RS-485/868-->                 │ CM4 + RP2040 + SX1262 + LTE + UPS   │
[Tap Sentinel×N] --BLE/868 MHz-->                            │ edge rules + MQTT bridge + OTA      │
                                                             └───────────────┬─────────────────────┘
                                                                             │
                                                                    Wi‑Fi / LTE MQTT
                                                                             │
                                              ┌──────────────────────────────▼──────────────────────────────┐
                                              │ FastAPI cloud backend + historian + alerting + ML services │
                                              └──────────────────────────────┬──────────────────────────────┘
                                                                             │
                                                                React Native homeowner + service app
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **Well Hub Gateway** | Raspberry Pi CM4 + RP2040 + SX1262 + Quectel EG25-G | Edge coordinator, local rules engine, historian cache, LTE failover, OTA and commissioning | 12 VDC supply + 2-cell LiFePO4 backup | Wi‑Fi, BLE 5.0, Ethernet, LTE, GNSS, 868 MHz FSK/LoRa |
| **Inline Water Quality Node** | STM32L476 + ADS1220 + RS-485 transceiver | Inline pH/EC/ORP/turbidity/temp/pressure sensing at the treatment manifold | 12 VDC from mechanical room rail | RS-485 Modbus RTU, 868 MHz backup |
| **Pump & Pressure Controller** | ESP32-S3 + ADE7953 + ADS1115 + SX1262 | Pump current signature analysis, contactor drive, VFD 0-10 V control, pressure tank supervision, leak-isolated shutoff | 24 VDC panel supply | RS-485, 868 MHz, Wi‑Fi service AP |
| **Tap Sentinel** | nRF52840 + TMP117 + flow switch front end + OPT3001 + PN532 | Under-sink point-of-use verification: tap temp, filter age, UV intensity, faucet usage, consumable check-in | 2000 mAh Li-ion or USB-C | BLE 5.0, 868 MHz sleepy endpoint |
| **Watershed Weather Sentinel** | RP2040 + SX1262 + BME280 + tipping bucket + capacitive soil probes | Rainfall, barometric changes, runoff proxy, freeze depth, dry-well trend context | 6 W solar + LiFePO4 | 868 MHz TDMA mesh |

## Daily User Experience

1. The Pump Controller confirms the well pump starts cleanly, pressure rises, and current waveform matches a healthy signature.
2. The Inline Node samples pH, conductivity, ORP, turbidity, line pressure, and water temperature every few minutes.
3. The Tap Sentinel verifies the under-sink UV reactor is lit, tracks faucet usage and filter service intervals, and catches flow events after a contamination advisory.
4. The Weather Sentinel correlates rainfall, barometric collapse, and soil saturation with contamination and drawdown risk.
5. The Hub fuses all of it into four household states: **Safe**, **Watch**, **Treat**, **Do Not Drink**.
6. The app explains why, suggests corrective actions, and stores a service-ready trace for lab testing, chlorination, or pump maintenance.

## Hazard Models

WellSync is centered on four high-value risk engines:

- **Contamination Surge Risk** — runoff + turbidity + ORP collapse + pressure anomalies.
- **Pump Failure Risk** — current imbalance, long ramp time, short-cycling, thermal stress.
- **Dry Well Risk** — static pressure recovery, rainfall deficit, pump runtime inflation.
- **Treatment Integrity Risk** — UV intensity drop, expired filter, abnormal tap usage after advisory.

## Node 1 — Well Hub Gateway

### Core hardware
- Raspberry Pi CM4 Lite, 4 GB RAM
- RP2040 supervisor MCU for deterministic local alarm and radio scheduling
- SX1262 868 MHz transceiver on RP2040 SPI0
- Quectel EG25-G LTE/GNSS modem on USB 2.0
- DS3231 RTC, INA219 rail monitor, ATECC608 secure element
- 7-inch service touchscreen or headless DIN-rail enclosure option
- local buzzer, stack light, relay output for whole-house advisory beacon

### Key interfaces
| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 UART0 | CM4 supervision channel |
| CM4 USB2 | EG25-G LTE modem |
| CM4 I2C1 | RTC, secure element, INA219 |
| CM4 Ethernet | local router / installer laptop |
| CM4 BLE/Wi‑Fi | Tap Sentinel commissioning |
| CM4 GPIO | alarm stack light, mute, service output relay |

### Power architecture
- 12 V DIN rail input
- surge clamp + reverse polarity MOSFET
- 5 V/4 A rail for CM4 + LTE modem
- 3.3 V logic rail for RP2040 + sensors
- 2-cell LiFePO4 pack for 6-hour advisory + LTE backup during outage

## Node 2 — Inline Water Quality Node

### Sensing chain
- **pH**: EZO-pH carrier into ADS1220 differential front end
- **EC/TDS**: EZO-EC carrier with isolated supply section
- **ORP**: EZO-ORP carrier for oxidation-reduction drift tracking
- **Turbidity**: SEN0189 analog front end
- **Pressure**: 0-10 bar 4-20 mA transducer into 165 Ω shunt
- **Temperature**: PT1000 RTD or DS18B20 stainless probe

### Deployment point
Mount after the pressure tank and before treatment branches so the node sees raw well output and cleaning events. A second variant can be placed after filtration for delta-quality calculations.

### Pin map
| STM32L476 pin | Function |
|---------------|----------|
| PA5/PA6/PA7 | SPI1 to ADS1220 |
| PB6/PB7 | I2C1 to EZO isolation mux |
| PA9/PA10 | UART1 to RS-485 transceiver |
| PC0 | turbidity ADC backup |
| PC1 | pressure shunt ADC |
| PB0 | relay-safe sample solenoid enable |
| PB12 | tamper switch |

## Node 3 — Pump & Pressure Controller

### Functions
- reads current waveform from ADE7953 and classifies pump start, run, stall, cavitation, and short-cycle events;
- measures pressure tank rise/decay using two transducers;
- drives contactor coil interlock and optional VFD analog reference;
- executes local safe shutdown when pressure collapse or dry-run signature appears;
- exposes service AP for installers with calibration pages.

### Power and safety
- 24 VDC control panel input
- opto-isolated relay outputs
- MOV + RC snubber across contactor coil
- watchdog-controlled fail-safe relay defaults to pump enable only when MCU is healthy

## Node 4 — Tap Sentinel

### Under-sink instrumentation
- TMP117 for cold/hot line temperature drift
- OPT3001 pointed at UV chamber sight port or filter-service LED
- reed or hall flow pulse capture from faucet adapter
- PN532 for consumable and cartridge NFC tap-to-register
- capacitive door-open sensor for cabinet access history

### Why this node matters
Water can be safe at the pressure tank and unsafe at the kitchen sink if UV fails, a cartridge bypasses, or a homeowner ignores an advisory. The Tap Sentinel closes that gap.

## Node 5 — Watershed Weather Sentinel

### Environmental signals
- tipping bucket rainfall accumulation and burst intensity
- BME280 pressure/humidity/temperature trends
- three soil probes at shallow/mid/deep depth for infiltration proxy
- DS18B20 frost-depth probe for buried line freeze alerts
- INA219 solar/battery telemetry for self-health

### Placement
Mount 10-30 m from the wellhead, above splashback, where rainfall and soil drainage reflect watershed loading but the node remains serviceable.

## Communications and Protocol

- **868 MHz TDMA mesh** across outbuildings, well houses, and long driveways.
- **RS-485 Modbus RTU** between the Hub and fixed mechanical-room nodes when wiring is available.
- **BLE 5.0** for provisioning Tap Sentinels and direct installer access.
- **MQTT over Wi‑Fi/LTE** between Hub and cloud.
- **Signed compact binary frames** defined in `firmware/common/protocol.h`.

Core frame types:
- `WSYNC_MSG_JOIN_REQ`
- `WSYNC_MSG_HEALTH`
- `WSYNC_MSG_WATER_QUALITY`
- `WSYNC_MSG_PUMP_STATE`
- `WSYNC_MSG_TAP_EVENT`
- `WSYNC_MSG_WEATHER`
- `WSYNC_MSG_ALERT`
- `WSYNC_MSG_COMMAND`
- `WSYNC_MSG_OTA_STATUS`

## Repository Layout

```text
WellSync/
├── README.md
├── schematic/
│   ├── README.md
│   ├── hub/hub.sch
│   ├── inline-water-quality/inline-water-quality.sch
│   ├── pump-controller/pump-controller.sch
│   ├── tap-sentinel/tap-sentinel.sch
│   └── weather-sentinel/weather-sentinel.sch
├── firmware/
│   ├── common/
│   ├── well-hub/
│   ├── inline-water-quality/
│   ├── pump-controller/
│   ├── tap-sentinel/
│   └── weather-sentinel/
├── hardware/bom/
├── software/
│   ├── dashboard/
│   ├── ml-pipeline/
│   └── mobile-app/
├── docs/
└── scripts/
```

## Cloud / Edge Software

`software/dashboard/` contains a FastAPI service with:

- telemetry ingestion for all node classes;
- household status derivation (`safe`, `watch`, `treat`, `do_not_drink`);
- risk-scoring endpoints for contamination, pump failure, dry-well probability, and treatment integrity;
- a service log for lab tests, chlorination events, filter changes, and pump replacements;
- websocket fan-out for live mobile dashboards.

## ML Pipeline

Reference models in `software/ml-pipeline/`:

1. `train_contamination_risk.py` — storm + chemistry based contamination surge score.
2. `train_pump_failure.py` — pump health score from current, pressure rise, and starts/hour.
3. `train_dry_well_forecast.py` — 7-day drawdown forecast.
4. `train_treatment_integrity.py` — UV/filter integrity anomaly score.
5. `train_service_priority.py` — homeowner-friendly remediation ranking.

Each script generates synthetic datasets and JSON artifacts so the backend can run without external training infrastructure.

## Mobile App

The React Native stub provides:
- household water state card;
- live chemistry dashboard;
- pump room service screen;
- advisory mode with step-by-step remediation;
- maintenance history and consumable inventory.

## BOM Summary

| Node | Estimated prototype BOM |
|------|--------------------------|
| Well Hub Gateway | $228 |
| Inline Water Quality Node | $186 |
| Pump & Pressure Controller | $122 |
| Tap Sentinel | $41 |
| Watershed Weather Sentinel | $58 |

## Build & Validation

1. Assemble nodes from the BOM CSVs.
2. Review the schematic notes in `schematic/`.
3. Compile firmware modules with host GCC for logic validation or map them into PlatformIO/MCU SDK projects.
4. Create a Python virtualenv in `software/dashboard/` and install `requirements.txt`.
5. Run `python -m unittest discover -s tests` inside `software/dashboard/`.
6. Run every ML training script to refresh artifacts.
7. Use `scripts/simulate_well_event.py` to push a contamination scenario into the backend.

## Safety Notes

- WellSync is a decision-support and automation aid, not a substitute for certified lab testing or code-compliant water treatment.
- Whole-house shutoff or pump-disable behavior must be reviewed against the home’s plumbing design and fire-suppression requirements.
- Any potable-water-contact materials must be NSF/ANSI compliant for the target jurisdiction.
- High-voltage pump controls require qualified installation.

## Next Engineering Steps

- validate chemistry drift and calibration intervals on real groundwater profiles;
- add coliform test-strip imaging dock for homeowner sampling workflows;
- integrate signed OTA manifests with rollback across mixed MCU fleets;
- pair WellSync advisories with regional watershed/open-data contamination feeds;
- build DIN-rail installer enclosure set and field calibration jig.

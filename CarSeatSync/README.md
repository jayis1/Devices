# CarSeatSync

AI-powered infant and child vehicle safety system — prevents hot-car tragedies, catches loose or twisted harnesses, verifies caregiver handoff, monitors child comfort and vitals, and coordinates family alerts before a routine breakdown turns into an emergency.

## What It Solves

Millions of families move children between home, daycare, school, relatives, and errands every day. The failure modes are common and severe:

- a caregiver forgets a sleeping child in the back seat during a routine change;
- a harness is clipped but too loose, chest clip too low, or straps are twisted;
- cabin heat rises dangerously within minutes after parking;
- a child falls asleep in a risky posture or becomes distressed during traffic;
- pickup and drop-off handoffs fail because adults assume someone else has the child;
- baby bags, medication, or EpiPens are left behind after the child exits.

CarSeatSync is built as a multi-node safety system instead of a single gadget. The seat, the child, the cabin, the door location, and the family cloud all contribute evidence. The system only escalates when multiple signals agree, which keeps trust high while reducing nuisance alerts.

## System Architecture

```text
[Child Band] --BLE-->                     ┌───────────────────────────────┐
[SafeLatch Clip] --BLE-->                 │  Family Vehicle Hub           │
[Cabin Sentinel] --BLE/Wi-Fi-->           │  CM4 + RP2040 + LTE + SX1262 │
                                          │  FastAPI edge + MQTT bridge   │
[Home/Daycare Handoff Beacon] <-868 MHz-> │  CAN ingest + GPS + siren     │
                                          └──────────────┬────────────────┘
                                                         │
                                                Wi-Fi / LTE MQTT
                                                         │
                                           ┌─────────────▼─────────────┐
                                           │ Cloud dashboard + ML      │
                                           │ risk scoring + audit log   │
                                           └─────────────┬─────────────┘
                                                         │
                                                React Native app
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **Family Vehicle Hub** | Raspberry Pi CM4 + RP2040 + SX1262 + MCP2515 + Quectel EG25-G | Edge coordinator, GPS/LTE alerting, MQTT bridge, CAN/ignition awareness, local alarm logic | 12 V vehicle input + supercap ride-through + LiFePO4 backup | BLE 5.3, Wi-Fi, LTE, GNSS, Sub-GHz 868 MHz, CAN |
| **SafeLatch Clip** | nRF52840 + HX711 + MMC5983MA + TMP117 + reed switch | Harness tension, chest-clip position, buckle state, twist/orientation detection | CR2450 + energy-harvest strap flex charger | BLE 5.3 |
| **Cabin Sentinel** | ESP32-S3 + AMG8833 + SCD41 + SGP41 + ICS-43434 + BME688 | Thermal occupancy, crying/distress acoustics, cabin air quality, heat-rise rate | 5 V from OBD/USB-C | BLE 5.3, Wi‑Fi |
| **Child Band** | nRF5340 + MAX86176 + TMP117 + LIS2DW12 + DRV2605L | Child skin temp, pulse, motion, sleep posture, haptic safety cues | 160 mAh LiPo | BLE 5.3 |
| **Handoff Beacon** | RP2040 + SX1262 + DW3000 + PN532 + 2.13in e-paper | Verifies arrival/departure at home/daycare, time-boxed pickup prompts, bag/item checks | USB-C 5 V or 18650 backup | Sub-GHz 868 MHz, UWB, NFC |

## Daily User Experience

1. Parent buckles the child into the seat.
2. SafeLatch Clip confirms buckle closure, strap preload, chest clip height, and twist angle.
3. Child Band confirms the right child is present and measures resting comfort state.
4. Vehicle Hub sees ignition ON over CAN, starts a trip session, and asks Cabin Sentinel to watch thermal occupancy and distress audio.
5. When parking occurs, the hub expects a valid unload sequence: door open, buckle release, child exits, band range changes, and optionally a Handoff Beacon confirmation.
6. If the caregiver walks away without the child, alerts escalate from phone push to in-car siren to LTE calls/SMS to emergency contacts.
7. At daycare or home, the Handoff Beacon closes the loop by verifying arrival and item completion: child out, bag out, medicine out.

## Node 1 — Family Vehicle Hub

### Hardware
- Raspberry Pi CM4 Lite, 2 GB RAM
- RP2040 supervisor MCU for deterministic local alarm state machine
- SX1262 868 MHz radio on RP2040 SPI0
- MCP2515 CAN controller + TJA1051 transceiver for ignition, door, speed, seat occupancy status where available
- Quectel EG25-G LTE/GNSS modem via USB 2.0
- 100 dB piezo siren, RGB status tower, capacitive acknowledge pad
- INA219 rail monitor, DS3231 RTC, 2-cell LiFePO4 backup HAT

### Key interfaces
| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 SPI1 | MCP2515 CAN controller |
| RP2040 I2C0 | INA219, RTC, capacitive LED driver |
| RP2040 UART0 | CM4 supervision link |
| CM4 USB2 | EG25-G modem |
| CM4 GPIO | siren relay, RGB tower, service switch |
| CM4 Wi‑Fi/BLE | seat nodes + mobile app commissioning |

### Power architecture
- 9-16 V automotive input
- TVS diode + reverse polarity MOSFET + 3 A fuse
- Buck rails: 5 V for CM4/peripherals, 3.3 V logic rail
- LiFePO4 backup supports 45 minutes emergency alerting after battery disconnect

## Node 2 — SafeLatch Clip

### Sensors and mechanics
- Reed switch detects buckle tongue inserted
- 20 kg half-bridge strain element + HX711 measures strap tension
- MMC5983MA + fixed magnetic target estimates chest-clip vertical placement relative to sternum zone
- TMP117 watches buckle/harness temperature for sun-heated burn risk
- TPU overmold enclosure clips onto existing harness geometry

### Pin map
| nRF52840 pin | Function |
|--------------|----------|
| P0.13/P0.14 | I2C TMP117 + MMC5983MA |
| P0.15/P0.16 | HX711 DOUT/SCK |
| P0.17 | reed switch IRQ |
| P0.18 | user chirp buzzer |
| P0.19 | battery ADC |

## Node 3 — Cabin Sentinel

### Sensors
- AMG8833 8x8 thermal array for occupant thermal silhouette and parked-car occupancy persistence
- SCD41 CO2, temperature, humidity
- SGP41 VOC/NOx
- BME688 for pressure and gas trend backup
- ICS-43434 I2S microphone for cry/distress classification and cabin noise level

### Heat safety logic
The sentinel computes cabin thermal slope in C/min. If ignition is off and a child thermal signature remains while cabin temperature rises faster than 0.35 C/min, it sends a high-priority persistence event even before the caregiver leaves BLE range.

## Node 4 — Child Band

### Measurements
- MAX86176 PPG for pulse and perfusion trend
- TMP117 skin temperature
- LIS2DW12 posture + movement
- DRV2605L haptic driver for gentle caregiver recall cues and child soothing routines

### Safety focus
The band is not a medical diagnostic. It is a corroborating sensor for presence, sleep posture, and distress trend. Edge firmware flags:
- prolonged forward-flexed sleep posture;
- elevated skin temp in hot-cabin scenarios;
- motionless presence after parking;
- unexpected separation from seat clip without caregiver acknowledgement.

## Node 5 — Handoff Beacon

### Deployment options
- front door beacon at home;
- classroom/daycare beacon;
- grandparent-house beacon;
- pediatric clinic beacon.

### Functions
- confirms child arrived within a defined unloading window;
- confirms bag/medicine via NFC tag tap;
- displays last action on e-paper: `CHILD IN`, `CHILD OUT`, `BAG MISSING`, `HANDOFF OK`;
- supplies precise near-door presence via DW3000 UWB to distinguish driving past from actual unloading.

## Communications and Protocol

- **BLE 5.3 star** inside the vehicle for low-power seat nodes and wearable.
- **868 MHz FSK/LoRa control plane** between Vehicle Hub and Handoff Beacons for long-range, low-dependency handoff confirmation.
- **MQTT over Wi‑Fi/LTE** from Vehicle Hub to cloud.
- **Signed protobuf-like compact frames** defined in `firmware/common/protocol.*`.

Core frame types:
- `MSG_SEAT_STATUS`
- `MSG_CHILD_STATUS`
- `MSG_CABIN_STATUS`
- `MSG_HANDOFF_EVENT`
- `MSG_TRIP_STATE`
- `MSG_ALERT`
- `MSG_ACK`

## Firmware Layout

```text
firmware/
├── common/              # shared frame encoder, CRC, safety enums
├── vehicle-hub/         # RP2040 supervisor + trip orchestration
├── safelatch-clip/      # nRF52 harness sensing
├── cabin-sentinel/      # ESP32-S3 occupancy + cry classifier hooks
├── child-band/          # nRF53 wearable logic
└── handoff-beacon/      # RP2040 arrival verification
```

## Cloud / Edge Software

`software/dashboard/` contains a FastAPI service that accepts telemetry, computes trip risk summaries, logs caregiver actions, and serves mobile clients.

Main API capabilities:
- ingest trip telemetry and alert state;
- fetch current child status per vehicle;
- produce explainable risk scores for hot-car, buckle misuse, and missed handoff;
- list recommended actions and escalation ladder;
- generate immutable event timelines for insurance, daycare, or safety review.

## ML Pipeline

Five lightweight reference models are included:

1. `train_heat_risk.py` — synthetic regression for cabin heat escalation risk.
2. `train_unload_gap.py` — unload-sequence failure probability.
3. `train_buckle_anomaly.py` — harness misuse score.
4. `train_sleep_posture.py` — posture comfort classifier.
5. `train_route_handoff.py` — route/context missed-handoff classifier.

The scripts generate reproducible synthetic datasets plus JSON artifacts that the demo backend can load without external dependencies.

## Mobile App

The React Native stub provides:
- live trip card with child-in-seat, ignition, cabin temp, and unload timer;
- alert ladder view with acknowledge/escalate buttons;
- caregiver roster and pickup responsibility;
- bag/medicine checklist at destination;
- trip history and safety score trends.

## BOM Summary

| Node | Estimated prototype BOM |
|------|--------------------------|
| Vehicle Hub | $212 |
| SafeLatch Clip | $27 |
| Cabin Sentinel | $38 |
| Child Band | $33 |
| Handoff Beacon | $46 |

## Build & Validation

1. Assemble each node from the BOM CSVs.
2. Review the KiCad schematic notes in `schematic/`.
3. Compile firmware with your preferred SDK after mapping board support.
4. Run dashboard tests:
   - `python -m unittest discover -s tests`
5. Run ML scripts to regenerate artifacts.
6. Use `scripts/simulate_trip.py` to generate end-to-end demo telemetry.

## Safety Notes

- CarSeatSync is a supplemental safety system, not a substitute for direct caregiver responsibility.
- All emergency dispatch actions should be reviewed for local legal/regulatory requirements.
- Child wearable skin-contact materials must be biocompatible and tested for age range.
- Automotive installation must comply with load-dump, EMI, and thermal requirements.

## Next Engineering Steps

- integrate production BLE pairing and secure element support;
- validate harness tension thresholds across seat manufacturers;
- train cry/distress and posture models on real consented datasets;
- add redundant interior radar for zero-light occupancy persistence;
- complete ISO 26262-inspired safety case for alarm escalation.

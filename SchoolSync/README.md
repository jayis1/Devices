# SchoolSync

**AI-powered family school-readiness, child transit safety, and daily routine orchestration system** — a multi-node home + commute platform that verifies backpacks and lunches, predicts morning lateness, confirms pickup/drop-off transitions, and reduces the mental load that makes families forget essentials during the most chaotic part of the day.

## What It Solves

School mornings fail in predictable but expensive ways:

- **Children leave without essentials** — lunch, medication, homework, instruments, chargers, permission slips, ID cards, and jackets are forgotten during rushed departures.
- **Parents do too much invisible coordination** — checking bags, packing food, watching the clock, monitoring weather, and tracking transit status burns attention before the day even starts.
- **Lunch safety is inconsistent** — insulated packs warm up, ice packs are forgotten, and perishable food sits too long.
- **Pickup and drop-off transitions are fragile** — kids can miss the correct vehicle, leave a backpack behind, or arrive late without anyone noticing until stress spikes.
- **Routines are hard to personalize** — every child has different readiness patterns, time blindness, sensory needs, and school schedules.

**SchoolSync** treats the morning routine as a measurable system. A home hub maintains schedules and family policy, a backpack tag verifies carry state and commute transitions, a lunchbox dock validates packed meals and temperature safety, a doorway sentinel checks readiness at the point of exit, and a transit beacon confirms the correct vehicle/route handoff. Machine learning forecasts lateness, forgotten-item risk, food safety, and routine drift so families get proactive help instead of reactive panic.

---

## System Architecture

```text
┌────────────────────────────────────────────────────────────────────────────────────┐
│                               SchoolSync Cloud / Edge                             │
│ FastAPI + MQTT + PostgreSQL + family schedule service + ML pipeline               │
│                                                                                    │
│ Models                                                                             │
│ • ReadyScore GBDT      - family morning readiness score (0-100)                  │
│ • LateCast TFT         - 5/10/20 min lateness forecast                            │
│ • ForgotRisk XGBoost   - per-child forgotten-item risk                            │
│ • LunchSafe LSTM       - lunch thermal safety window forecast                     │
│ • RouteGuard IF        - commute route / handoff anomaly detection                │
└──────────────────────────────────────┬─────────────────────────────────────────────┘
                                       │ MQTT over TLS / HTTPS / WebSocket
                                       │
                         ┌─────────────┴────────────────────────┐
                         │ SchoolSync Family Hub Gateway         │
                         │ Raspberry Pi CM4 + RP2040 + SX1262   │
                         │ local scheduler + voice prompts       │
                         └───────┬───────────────┬───────────────┘
                                 │               │
                 868 MHz TDMA mesh + BLE/UWB     Wi-Fi / app provisioning
                                 │
       ┌─────────────────────────┼────────────────────────┬────────────────────────┐
       │                         │                        │                        │
┌──────┴─────────┐   ┌───────────┴──────────┐   ┌────────┴──────────┐   ┌────────┴──────────┐
│ Backpack Tag   │   │ Lunchbox Dock         │   │ Doorway Sentinel  │   │ Transit Beacon     │
│ UWB+IMU+NFC    │   │ weight+temp+ice pack  │   │ UWB anchor + PIR  │   │ GNSS+LTE/BLE route │
│ carry state    │   │ readiness check       │   │ exit checklist    │   │ pickup/drop verify │
└────────────────┘   └───────────────────────┘   └───────────────────┘   └───────────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **SchoolSync Family Hub** | Raspberry Pi CM4 + RP2040 + SX1262 | Local scheduler, MQTT bridge, family rules, OTA, voice/visual guidance | 12 V / 4 A wall input + 2x18650 UPS | Wi-Fi, Ethernet, BLE 5.0, Sub-GHz 868 MHz |
| **Backpack Tag** | nRF5340 + Qorvo DW3110 + SX1262 | Presence, carry-state, zipper/strap events, shock, in/out geofence, child acknowledgment | 900 mAh LiPo | BLE 5.3, UWB, Sub-GHz 868 MHz |
| **Lunchbox Dock** | ESP32-S3-WROOM-1 + HX711 + TMP117 + SHT41 | Lunch packed verification, mass delta, ice-pack compliance, thermal holdover prediction | 5 V USB-C or 2x18650 dock | Wi-Fi for setup, Sub-GHz 868 MHz |
| **Doorway Sentinel** | STM32WL55 + DW3110 + RCWL-0516 + PN532 | Exit verification, checklist display, door-side haptic/voice prompts, optional NFC assignment | 12 V wall power | Sub-GHz 868 MHz, UWB |
| **Transit Beacon** | nRF9160 SiP + LIS2DW12 + SX1262 | Vehicle/bus route check, pickup/drop confirmation, ETA relay, separation alert, LTE fallback | 12 V vehicle or 2000 mAh LiPo | LTE-M/NB-IoT, GNSS, BLE, Sub-GHz 868 MHz |

---

## Daily User Experience

1. At 6:40 AM, the **Family Hub** pulls today's class schedule, weather, after-school activities, and bus ETA.
2. The **Lunchbox Dock** confirms the lunch container and ice pack are present, total packed mass is above the child's personalized threshold, and the predicted safe cold-hold window covers lunch period.
3. The **Backpack Tag** detects that the homework pouch was opened but the backpack has not yet been lifted after the usual departure window.
4. The **Doorway Sentinel** lights a checklist: **Backpack ✅ Lunch ✅ Library book ❌ Jacket ✅**.
5. A child taps the backpack tag button to acknowledge the reminder and clips the missing library book into the bag.
6. During departure, UWB ranging between tag and sentinel confirms the child actually exited with the backpack rather than leaving it beside the door.
7. The **Transit Beacon** in the family car or school-bus handoff point confirms pickup, compares the planned route to expected behavior, and sends ETA to caregivers.
8. If the backpack leaves the vehicle while the child remains seated, or vice versa, the app escalates before the family drives away.

---

## Node 1 - SchoolSync Family Hub

### Core hardware

- **Compute module:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for watchdog, deterministic radio timing, safe OTA rollback trigger, and offline prompt fallback
- **Sub-GHz radio:** Semtech SX1262 with 868 MHz antenna matching network
- **Local UX:** 7 inch HDMI/DSI family dashboard, MAX98357A I2S amp + 3 W speaker, RGB status bar
- **Timekeeping:** DS3231 RTC for schedule continuity during network outage
- **Power:** 12 V input -> 5 V / 6 A buck for CM4/display -> 3V3 buck for RP2040/radio; dual-18650 UPS board for ~90 min operation

### Responsibilities

- Maintains calendar, roster, dismissal plans, child profiles, medication notes, and exception rules
- Bridges Sub-GHz node telemetry to MQTT and local WebSocket dashboard
- Calculates morning Readiness Score and lateness risk every minute during active routine windows
- Runs local fail-safe prompts when internet, school API, or phone connectivity is unavailable
- Distributes OTA firmware manifests and commissioning secrets
- Produces caregiver and pediatric OT/ADHD-friendly readiness reports without exporting raw child voice/audio

### Hub pin / interface map

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 I2C0 | DS3231 RTC, INA219 power monitor, PCA9633 LED driver |
| RP2040 UART0 | CM4 heartbeat/watchdog link |
| CM4 DSI | 7 inch family display |
| CM4 I2S | MAX98357A speaker amplifier |
| CM4 USB2 | optional LTE modem / keyboard service |
| CM4 Ethernet | router uplink / PoE splitter |

---

## Node 2 - Backpack Tag

A rugged clip-on tag mounted inside or outside a school backpack.

### Electronics

- **Application MCU:** Nordic **nRF5340** for BLE 5.3, secure provisioning, and low-power scheduling
- **Ranging:** Qorvo **DW3110** UWB transceiver for sub-meter doorway verification and handoff checks
- **Long-range radio:** **SX1262** for direct 868 MHz telemetry to the hub through walls and parked vehicles
- **Motion:** Bosch **BMA400** accelerometer for carry-state, shock, and drop detection
- **Environment:** Sensirion **SHTC3** for bag temperature/humidity trend relevant to lunch safety and forgotten backpack-in-car alerts
- **Pocket/zipper sensing:** 2x A3144 hall sensors with sewn-in magnets on homework and medication pockets
- **User input/output:** RGB LED, piezo beeper, acknowledgment button, optional small vibration motor
- **Tap-to-assign:** ST **ST25DV04K** NFC tag for quick child/device pairing and bag ownership

### Derived features

- Lifted / worn / stationary / vehicle-motion states
- Backpack left-behind risk at home, school, or vehicle
- Pocket-open but item-not-carried sequences
- Child acknowledgment latency for reminders
- Shock history for device, instrument, or laptop protection analytics

### Power architecture

- 900 mAh LiPo with **BQ24075** charger/power-path IC over USB-C
- 3V3 buck-boost rail for radios and haptics
- UWB duty-cycled only during departure, arrival, and transit transitions to preserve battery life

---

## Node 3 - Lunchbox Dock

A countertop or fridge-door dock that verifies the lunchbox is packed, chilled, and actually taken.

### Sensors and actuators

- **SoC:** Espressif **ESP32-S3-WROOM-1-N8R2**
- **Mass sensing:** 5 kg single-point load cell + **HX711** ADC to verify packed food mass and distinguish lunchbox vs empty container
- **Temperature:** 2x **TMP117** (dock ambient + lunch-contact plate)
- **Humidity:** **SHT41** for condensation and thermal modeling
- **Ice-pack verification:** reed switch / Hall sensor pocket and optional NFC label in reusable ice pack
- **Presence:** **VL53L0X** ToF for dock occupancy and pickup timing
- **UX:** 1.54 inch e-paper panel, green/amber/red LED, capacitive snooze button
- **Actuation:** optional 5 V Peltier fan-assist lid pre-cooler relay for premium version

### Logic

- Detects whether the lunchbox is packed before departure window
- Predicts safe lunch temperature at cafeteria opening based on current thermal state and outdoor conditions
- Distinguishes "packed but not taken" from "empty box returned after school"
- Flags likely forgotten ice pack, insufficient meal volume, or spoiled leftovers

---

## Node 4 - Doorway Sentinel

Installed near the exit actually used on school mornings.

### Hardware

- **MCU/radio:** ST **STM32WL55JC** with integrated Sub-GHz radio
- **Ranging anchor:** Qorvo **DW3110** for proximity confirmation with backpack tag
- **Motion/presence:** **RCWL-0516** microwave motion or LD2410 mini-mmWave option for hall occupancy
- **Checklist assignment:** **PN532** NFC frontend for tapping bus cards, instrument tags, or lunch tokens
- **Visual UX:** 2.13 inch tri-color e-paper checklist and WS2812 status strip
- **Audio/Haptics:** PAM8302 amp + small speaker for soft spoken cues; DRV2605L on optional haptic plate for sensory-sensitive routines
- **Door sensing:** reed switch for open/close correlation with exit events

### Behaviors

- Verifies backpack and lunch are physically moving through the doorway together
- Announces only the next missing item instead of dumping a long list
- Uses child-specific sensory profile: visual-only, tone-only, speech, or vibration plate
- Starts countdown escalation when predicted lateness exceeds threshold

---

## Node 5 - Transit Beacon

A dashboard, bike-basket, or bus-stop-side companion that confirms the correct commute handoff.

### Hardware

- **Cellular + GNSS:** Nordic **nRF9160** SiP for LTE-M/NB-IoT connectivity and onboard GNSS
- **Local radio:** **SX1262** for resilient short-packet messaging to backpack tag/hub without relying on LTE latency
- **BLE coprocessor:** nRF9160 integrated BLE via external companion abstraction in firmware template
- **Motion:** **LIS2DW12** accelerometer for boarding, braking, and vehicle movement signatures
- **UI:** 1.9 inch monochrome LCD, single SOS/cancel button, buzzer
- **Power:** 12 V vehicle input with MP1584 buck or 2000 mAh LiPo backup + MCP73831 charger

### Use cases

- Mounted in a family car to confirm correct child + backpack departure and arrival
- Installed in a bike basket or scooter stem for older children commuting independently
- Placed at a private bus stop / apartment lobby pickup zone to log route deviations and missed pickups

---

## Communication and Protocol

SchoolSync uses **868 MHz TDMA mesh** for deterministic household coverage, **BLE 5.3** for provisioning, and **UWB** for high-confidence presence verification where false positives matter.

- **Mesh beacon interval:** 60 s nominal, 5 s during morning/afternoon routine windows
- **Frame payload max:** 48 bytes
- **Encryption:** AES-128 CTR payload protection with rotating session nonce
- **Integrity:** CRC-16/CCITT on every frame
- **Node IDs:** 16-bit household-scoped addressing
- **UWB roles:** Doorway Sentinel = anchor, Backpack Tag = tag, Transit Beacon = optional anchor/listener
- **Fallback path:** LTE-M alerts from Transit Beacon if home internet is down during pickup/drop-off

See [`docs/protocol_spec.md`](docs/protocol_spec.md) and [`firmware/common/protocol.h`](firmware/common/protocol.h).

---

## ML Stack

### 1. ReadyScore
Gradient-boosted decision tree that predicts a per-child readiness score from:
- time-to-departure
- backpack carry-state history
- lunch packed state
- sleep / wake timing import
- weather complexity (rain/cold requiring extra gear)
- caregiver intervention count

### 2. LateCast
Temporal Fusion Transformer style forecast approximating probability of leaving late by 5, 10, and 20 minutes.

### 3. ForgotRisk
XGBoost classifier estimating forgotten-item risk for categories such as lunch, library item, sports gear, instrument, meds, or paperwork.

### 4. LunchSafe
Sequence model predicting lunch thermal safety window from dock plate temp, lunchbox mass, ice-pack presence, ambient temp, and expected school schedule.

### 5. RouteGuard
Isolation Forest on commute features:
- expected route duration
- stop sequence
- handoff timing
- separation events between child, bag, and vehicle beacon

---

## Software Stack

### Backend
- **FastAPI** REST + WebSocket API
- **MQTT** ingestion from hub and nodes
- **PostgreSQL** for telemetry, schedules, and routine events
- **Edge inference helpers** for readiness, lateness, lunch safety, and anomaly alerts

### Mobile app
- **React Native** caregiver app with morning dashboard, checklist view, commute timeline, and alert feed
- supports multiple children, custody schedules, and role-based caregivers

### Firmware
- C firmware for each node with shared protocol library, CRC16, and mesh queue abstraction

---

## BOMs and Schematics

Each node includes:
- BOM CSV in `hardware/bom/`
- KiCad-style placeholder schematic in `schematic/<node>/`
- firmware source in `firmware/<node>/`

---

## Safety, Privacy, and Human Factors

- No always-on cameras in the home path; the system relies on tags, weight, temp, motion, UWB, and optional NFC.
- Children can acknowledge prompts locally without needing a phone.
- Sensory output is configurable for neurodivergent users.
- Transit alerts default to caregivers only; school staff sharing is opt-in.
- OTA updates are signed and staged through the hub.

---

## Repository Layout

```text
SchoolSync/
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

## Build Notes

1. Assemble hardware using per-node BOM and schematic notes.
2. Flash node firmware from `firmware/*`.
3. Launch the backend in `software/dashboard`.
4. Train or retrain models in `software/ml-pipeline`.
5. Pair devices through the React Native app.

## Future Extensions

- Binder / cubby node for after-school homework routines
- Smart washer-safe PE tag in gym clothes for sports-day verification
- Classroom arrival beacon integration where schools permit it
- On-device multilingual voice prompting

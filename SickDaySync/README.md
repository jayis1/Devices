# SickDaySync

**AI-powered home illness recovery and household contagion-control system** — a coordinated care platform that helps families detect worsening illness early, keep the right room conditions for recovery, improve medication and hydration adherence, reduce within-home spread, and give caregivers a plain-language view of what to do next.

## What It Solves

Acute respiratory illness, flu, RSV, COVID, stomach bugs, and seasonal viral infections create the same daily problems in millions of households:

- **People miss the moment when a “mild sick day” is becoming a real problem.** Fever trends, respiration changes, and hydration decline are gradual until they suddenly are not.
- **Caregivers juggle room air, medication timing, fluids, isolation, and sleep manually.** That becomes harder at 2 a.m. when everyone is tired.
- **Homes are not set up as recovery spaces.** Poor ventilation, dry air, hot bedrooms, and door-opening traffic can increase discomfort and spread.
- **Medication adherence breaks down during fatigue.** Doses get delayed, duplicate doses happen, and temperature-sensitive medications are mishandled.
- **Families need guidance, not raw graphs.** They want answers like: “Is this person improving?”, “Should we isolate harder?”, “Is dehydration becoming likely?”, and “When should we escalate care?”

**SickDaySync** turns a bedroom, nursery, guest room, or home isolation space into an orchestrated recovery environment. It combines wearable physiology, room air telemetry, smart medication tracking, and airflow control with cloud/edge software that forecasts fever trajectory, cough burden, hydration risk, and household transmission pressure.

---

## System Architecture

```text
┌────────────────────────────────────────────────────────────────────────────────────┐
│                           SickDaySync Cloud / Edge Stack                          │
│ FastAPI + MQTT + SQLite/Postgres adapter + event rules + model store              │
│                                                                                    │
│ Models                                                                             │
│ • CoughBurdenNet   - room-level cough intensity and night disruption score         │
│ • FeverCast        - 6 h fever trajectory forecast and antipyretic response        │
│ • HydrationGuard   - dehydration risk estimator from vitals + intake + environment │
│ • SpreadScore      - household contagion pressure and room-isolation effectiveness  │
│ • RecoveryClock    - probable recovery phase and escalation recommendation          │
└──────────────────────────────────────┬─────────────────────────────────────────────┘
                                       │ HTTPS / MQTT / WebSocket
                                       │
                        ┌──────────────┴─────────────────┐
                        │        SickDaySync Care Hub     │
                        │  CM4 + RP2040 + SX1262 + LTE    │
                        │ local rules, voice prompts, OTA │
                        └───────┬─────────────┬───────────┘
                                │             │
                 Sub-GHz 868 MHz│             │ BLE 5 / Wi-Fi setup
                                │             │
         ┌──────────────────────┼─────────────┼─────────────────────┬──────────────────────┐
         │                      │             │                     │                      │
┌────────┴─────────┐  ┌─────────┴─────────┐ ┌─┴────────────────┐ ┌──┴─────────────────┐ ┌─┴───────────────┐
│ Recovery Band    │  │ Room Sentinel xN  │ │ Med Station      │ │ Vent Controller xM │ │ Mobile App      │
│ skin temp, SpO2, │  │ cough, CO2, PM,   │ │ meds, fluids,    │ │ HEPA, window, fan, │ │ caregiver &     │
│ HRV, motion      │  │ humidity, mmWave  │ │ thermometer dock  │ │ humidifier control  │ │ patient view    │
└──────────────────┘  └───────────────────┘ └──────────────────┘ └────────────────────┘ └─────────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **SickDaySync Care Hub** | Raspberry Pi CM4 + RP2040 + SX1262 + Quectel EG25-G | Orchestration, local dashboard, MQTT bridge, clinician/caregiver sync, fallback alerts | 12 VDC input + 2-cell Li-ion UPS HAT | Ethernet, Wi‑Fi, BLE 5, LTE, Sub-GHz 868 MHz |
| **Recovery Band** | nRF52840 + MAXM86161 + MAX30208 | Wearable fever, HR, HRV, SpO₂, motion, symptom-button telemetry | 180 mAh LiPo | BLE 5 + Sub-GHz 868 MHz |
| **Room Sentinel** | ESP32-S3-WROOM-1 + SX1262 + SCD41 + SEN55 + ICM-42688-P + BGT60LTR11AIP | Cough counting, air quality, occupancy, sleep disruption, comfort monitoring | USB-C 5 V | Wi‑Fi, BLE, Sub-GHz 868 MHz |
| **Med Station** | RP2040 + HX711 + MLX90614 + PN532 + SX1262 | Medication verification, oral thermometer dock, hydration bottle weighing, dose reminders | 5 V USB-C + 18650 backup | BLE 5 + Sub-GHz 868 MHz |
| **Vent Controller** | STM32G0B1 + SX1262 + SHT45 + SDP31 | Window/fan/hepa/humidifier actuation, airflow balancing, pressure-aware isolation | 24 VDC | RS-485, dry-contact relays, Sub-GHz 868 MHz |

---

## Daily User Experience

1. A child develops a low-grade fever and frequent cough after school.
2. The **Recovery Band** notices rising skin temperature, elevated resting heart rate, and reduced step count.
3. The bedroom **Room Sentinel** classifies 42 cough events during the first hour, sees CO₂ climb above 1400 ppm, and marks the room as poorly ventilated.
4. The hub raises the **Sick Room Score** from green to amber and recommends opening the isolation window 8 cm and starting HEPA mode 2.
5. The caregiver places the bottle and medication bottle on the **Med Station**, which logs fluid intake, checks the correct medicine by NFC tag, and confirms a no-duplicate dose window.
6. The **Vent Controller** balances exhaust fan + window opening to maintain fresh air without overcooling the room.
7. Overnight, **FeverCast** predicts fever will peak in 90 minutes; the mobile app recommends preparing fluids, cooling bedding, and rechecking comfort rather than waking the patient immediately.
8. If SpO₂ dips, cough burden spikes, room humidity falls below 35%, and hydration risk rises, the app changes from home-care coaching to **“watch closely / consider clinician call”**.
9. When the patient improves, the app shows a declining spread score and suggests when common spaces can safely return to normal ventilation schedules.

---

## Node 1 — SickDaySync Care Hub

### Core hardware

- **Compute module:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for watchdog, radio timing, and offline failsafe rules
- **Sub-GHz radio:** Semtech SX1262 at 868 MHz with external SMA antenna
- **Cellular fallback:** Quectel EG25-G for SMS escalation when home internet fails
- **Local UX:** 5-inch capacitive touch display, RGB room-state tower, speaker for spoken instructions
- **Power:** 12 V input -> 5 V / 6 A buck for CM4/display, 3.3 V buck for MCU/radio; 2×18650 UPS keeps alerts active for >6 hours

### Responsibilities

- Maintains household illness profiles, room assignments, quiet hours, and alert policies
- Bridges Sub-GHz telemetry to MQTT topics
- Runs edge inference when internet is unavailable
- Hosts caregiver dashboard, device enrollment, and policy editor
- Stores fever/cough/hydration logs and medication events
- Issues escalation rules for low oxygen, rising fever, dose conflicts, and poor room isolation

### Hub pin / interface map

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 UART0 | CM4 heartbeat + failsafe RPC |
| RP2040 I2C0 | RTC, INA219, speaker amp control |
| CM4 USB2 | EG25-G modem |
| CM4 DSI | 5-inch touch display |
| CM4 GPIO | RGB tower, service key, mute switch |

---

## Node 2 — Recovery Band

### Why it matters

A sick person’s status changes between manual thermometer checks. The band captures trend information continuously with low burden.

### Hardware

- **MCU/Radio:** nRF52840
- **PPG module:** MAXM86161 for HR, HRV, SpO₂ trend tracking
- **Skin temperature:** MAX30208 clinical-grade body temperature sensor
- **Motion:** BMI270 6-axis IMU for restlessness and sleep interruption
- **Haptics:** DRV2605L ERM driver for medication and hydration reminders
- **Input:** one symptom event button for “pain spike / chills / nausea / cough fit”
- **Power:** 180 mAh LiPo with MCP73831 charging and MAX17048 fuel gauge

### Primary outputs

- Fever trend score
- resting HR deviation
- overnight wakefulness index
- dehydration proxy from HR + temp + low motion + intake gap
- symptom marker timeline for clinician reports

---

## Node 3 — Room Sentinel

### Hardware

- **SoC:** ESP32-S3-WROOM-1 with 8 MB PSRAM
- **Sub-GHz backhaul:** SX1262
- **Air sensing:** Sensirion SCD41 (CO₂), SEN55 (PM1/2.5/4/10 + VOC + RH + temp)
- **Occupancy/privacy:** Infineon BGT60LTR11AIP 60 GHz presence radar
- **Vibration/noise:** ICM-42688-P for bedside cough-vibration discrimination and tamper detection
- **Audio front end:** dual ICS-43434 I²S microphones for privacy-first cough classifier; raw audio discarded after feature extraction
- **Indicators:** tri-color status LED + e-paper tile for room state

### Inference on node

- cough event classifier
- cough burst clustering
- night disturbance score
- ventilation-needed heuristic
- occupancy-aware quiet mode

---

## Node 4 — Med Station

### Hardware

- **MCU:** RP2040
- **Long-range radio:** SX1262
- **Medication verification:** PN532 NFC tags on medicine bottles and supplies
- **Weight sensing:** 2× HX711 channels for medicine tray and hydration bottle dock
- **Temperature dock:** MLX90614 IR sensor aligned to oral thermometer cradle for check-in confirmation
- **Visual feedback:** 2.9-inch e-paper status panel + RGB light strip
- **Outputs:** piezo reminder buzzer + relay-switched smart nebulizer/heat-pad outlet

### Tasks

- Prevent duplicate dosing inside configurable lockout windows
- Log hydration bottle mass changes
- Confirm that thermometer checks actually happened
- Track nebulizer or humidifier sessions
- Export plain-language medication timeline

---

## Node 5 — Vent Controller

### Hardware

- **MCU:** STM32G0B1CCT6
- **Pressure and airflow:** Sensirion SDP31 differential pressure sensor
- **Humidity feedback:** SHT45
- **Relays:** 4× 10 A relays for HEPA fan, inline exhaust, humidifier, and room fan
- **Motor drivers:** DRV8871 H-bridge for chain-window actuator or damper motor
- **Field bus:** isolated MAX3485 RS-485 for ERV/HRV integration
- **Safety:** current fuse, relay feedback, manual override rocker, end-stop inputs

### Control modes

- recovery comfort mode
- isolation mode with slight negative pressure target
- sleep quiet mode
- purge mode after patient leaves room
- mold-avoidance humidity ceiling mode

---

## Communications and Protocol

SickDaySync uses a deterministic **868 MHz TDMA star** with room-level retry support.

- **Superframe:** 500 ms during normal operation, 200 ms during alerts
- **Max payload:** 56 bytes
- **Addressing:** 16-bit household node IDs + 8-bit room IDs
- **Security:** AES-128 CTR payload encryption + CRC-16/CCITT
- **Priority classes:** medical-alert > environment-control > routine telemetry > bulk logs
- **Commissioning fallback:** BLE pairing from mobile app

Frame details are in [`docs/protocol_spec.md`](docs/protocol_spec.md) and shared definitions live in [`firmware/common/protocol.h`](firmware/common/protocol.h).

---

## Firmware Layout

```text
firmware/
├── common/
│   ├── protocol.h
│   └── protocol.c
├── hub/main.c
├── recovery-band/main.c
├── room-sentinel/main.c
├── med-station/main.c
└── vent-controller/main.c
```

Each firmware target is written in portable C so the control logic, packet handling, and state machines can be compiled in CI before vendor HAL integration.

---

## Cloud / Edge Software

`software/dashboard/` contains a FastAPI service that provides:

- `/health` for liveness
- `/telemetry` ingestion
- `/risk/summary` combined patient and room risk scoring
- `/patients/{patient_id}/timeline` event history view
- `/recommendations` plain-language intervention suggestions

The service stores events in SQLite by default and publishes normalized records to MQTT topics.

---

## ML Pipeline

`software/ml-pipeline/` contains reproducible training scripts for:

- `train_cough_classifier.py`
- `train_fever_forecast.py`
- `train_hydration_risk.py`
- `train_spread_score.py`
- `train_recovery_clock.py`

The included scripts generate synthetic training corpora and export JSON artifacts so the repo remains runnable without external protected datasets.

---

## Mobile App

The React Native app provides:

- patient cards and room status
- medication + hydration reminders
- escalation ladder view
- quiet-hours aware caregiver notifications
- clinician export trigger

See `software/mobile-app/`.

---

## BOM Summary

| Node | Estimated prototype BOM |
|------|--------------------------|
| Care Hub | $198 |
| Recovery Band | $34 |
| Room Sentinel | $49 |
| Med Station | $41 |
| Vent Controller | $56 |

Detailed CSV BOMs live in `hardware/bom/`.

---

## Build & Validation

```bash
python3 -m unittest discover -s software/dashboard/tests
python3 software/ml-pipeline/train_cough_classifier.py
python3 software/ml-pipeline/train_fever_forecast.py
python3 software/ml-pipeline/train_hydration_risk.py
python3 software/ml-pipeline/train_spread_score.py
python3 software/ml-pipeline/train_recovery_clock.py
gcc -std=c11 -Wall -Wextra -I firmware/common -fsyntax-only firmware/common/protocol.c firmware/hub/main.c firmware/recovery-band/main.c firmware/room-sentinel/main.c firmware/med-station/main.c firmware/vent-controller/main.c
```

---

## Safety Notes

- SickDaySync is a **decision-support** system, not a diagnostic device.
- It should not replace emergency care, pulse-ox clinical guidance, or medication labels.
- Ventilation automation must follow local electrical and fire code.
- Window automation must include pinch detection, rain lockout, and emergency manual release.
- Medication workflows must respect pediatric dosing, allergies, and physician instructions.

---

## Next Engineering Steps

1. Replace synthetic models with clinically curated datasets and IRB-cleared protocols.
2. Add BLE thermometer interoperability for Braun/Kinsa-style devices.
3. Integrate smart plug profiles for approved nebulizers and vaporizers.
4. Add multilingual spoken guidance packs for overnight caregiver use.
5. Validate room spread model against tracer-gas ACH measurements.

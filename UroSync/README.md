# UroSync

**AI-powered bathroom health, urinary wellness, and nighttime safety system** — a multi-node home health platform that converts ordinary bathroom routines into early warning signals for hydration problems, urinary tract infection risk, nocturia escalation, kidney stone recurrence, medication side effects, and nighttime fall danger.

## What It Solves

Millions of people deal with urinary and bathroom-related issues that are annoying, embarrassing, expensive, or dangerous:

- **Dehydration sneaks up silently** — people often realize they are under-hydrated only after headaches, fatigue, constipation, or dizziness start.
- **UTIs are detected late** — many people wait until pain, urgency, fever, or confusion appear, especially older adults and postpartum patients.
- **Nighttime bathroom trips are risky** — nocturia drives falls, orthostatic dizziness, disorientation, and poor sleep quality.
- **Kidney stone prevention is hard to sustain** — clinicians recommend hydration and urinary monitoring, but people have no continuous home feedback loop.
- **Bathroom habits reveal health change** — voiding frequency, volume, color, strip chemistry, hydration behavior, and environment together can indicate meaningful deterioration days before a clinic visit.

**UroSync** turns the bathroom into a privacy-preserving early-warning environment. A toilet-side analyzer reads disposable urine chemistry strips and void metrics, a smart floor mat monitors nighttime transfers and balance, a bottle tag tracks real hydration behavior, a bathroom sentinel controls ventilation and leak safety, and a mirror hub fuses everything into clear guidance for users, caregivers, and clinicians.

---

## System Architecture

```text
┌────────────────────────────────────────────────────────────────────────────────────┐
│                                 UroSync Cloud / Edge                              │
│ FastAPI + MQTT + PostgreSQL + clinician export + ML training + edge cache        │
│                                                                                    │
│ Models                                                                             │
│ • HydroCast LSTM        - 24 h hydration and dehydration-risk forecast            │
│ • UTIWatch XGBoost      - chemistry + habit + temp + nocturia UTI risk            │
│ • NocturiaTrend TFT     - 7 day nighttime-void trend and escalation detection     │
│ • NightSafe RF          - transfer + sway + environment fall-risk model           │
│ • StoneShield Cox/XGB   - kidney-stone recurrence adherence + risk scoring        │
│ • NudgeBandit           - personalized hydration / bathroom-behavior interventions│
└──────────────────────────────────────┬─────────────────────────────────────────────┘
                                       │ MQTT over TLS / HTTPS / WebSocket
                                       │
                           ┌───────────┴────────────────────┐
                           │ UroSync Mirror Hub Gateway      │
                           │ Raspberry Pi CM4 + RP2040       │
                           │ SX1262 mesh coordinator         │
                           │ 8" touch mirror + speaker       │
                           └───────┬───────────────┬─────────┘
                                   │               │
                        868 MHz TDMA mesh          BLE / Wi-Fi commissioning
                                   │
         ┌─────────────────────────┼────────────────────────┬───────────────────────┐
         │                         │                        │                       │
┌────────┴─────────┐   ┌───────────┴─────────┐   ┌──────────┴─────────┐   ┌────────┴─────────┐
│ Toilet Dock      │   │ Night Safety Mat    │   │ Hydration Bottle    │   │ Bath Sentinel     │
│ strip reader +   │   │ load cells + sway   │   │ Tag weight + sip    │   │ humidity/VOC/leak │
│ uroflow + audio  │   │ + foot temp         │   │ detection           │   │ + fan relay       │
└──────────────────┘   └─────────────────────┘   └────────────────────┘   └──────────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **UroSync Mirror Hub** | Raspberry Pi CM4 + RP2040 + Semtech SX1262 | Local coordination, MQTT bridge, edge analytics cache, caregiver dashboard, OTA | 12 V / 4 A wall input + 2x18650 UPS | Wi‑Fi, Ethernet, BLE 5.0, Sub‑GHz 868 MHz |
| **Toilet Dock Analyzer** | ESP32-S3-WROOM-1 + AS7341 + AD5933 + HX711 | Disposable strip chemistry reading, uroflow/volume estimation, stream acoustics, local prompts | 12 V DC adapter | Wi‑Fi for setup, Sub‑GHz 868 MHz |
| **Night Safety Mat** | STM32L4R5 + ADS1232 + TMP117 + VL53L5CX | Sit-to-stand timing, balance asymmetry, night-trip count, orthostatic risk cues | 5 V adapter or LiFePO4 backup | Sub‑GHz 868 MHz |
| **Hydration Bottle Tag** | nRF52840 + SX1262 + HX711 + LIS2DW12 | Bottle mass, sip detection, drink logging, goal nudges | 1200 mAh LiPo, USB-C charged | BLE 5.0, Sub‑GHz 868 MHz |
| **Bath Sentinel** | STM32WL55 + SGP41 + SHT31 + LEM leak strip | Humidity/VOC ventilation control, slip-risk context, leak detection, night-light | 12 V wall adapter | Sub‑GHz 868 MHz |

---

## Daily User Experience

1. The user wakes at 2:13 AM and gets out of bed for a bathroom trip.
2. The **Night Safety Mat** detects slower transfer speed, left/right imbalance, and elevated foot temperature trend.
3. The **Bath Sentinel** turns on a low-glare amber night light and pre-runs the exhaust fan because humidity was already high.
4. The user voids; the **Toilet Dock Analyzer** measures approximate volume, reads a single-use strip for leukocyte/nitrite/blood/protein/ketone/glucose/pH/specific gravity proxies, and classifies urine color.
5. The **Hydration Bottle Tag** sees that total intake over the last 14 hours is low.
6. The **Mirror Hub** fuses the signals and says: **"Hydration risk high. UTI risk moderate. Please drink 300 mL water now. If burning or fever is present, contact care."**
7. Over the next three days, **UTIWatch** notices chemistry worsening and nocturia increasing from 1.1 to 3.4 episodes/night.
8. The app escalates from self-care guidance to a caregiver / clinician export recommendation.

---

## Clinical / Lifestyle Outcomes Targeted

- Earlier dehydration detection and adherence reinforcement
- Earlier UTI suspicion and escalation, especially for older adults and postpartum recovery
- Reduced nighttime falls during nocturia episodes
- Better kidney stone prevention through sustained hydration tracking
- Better awareness of LUTS trend changes after medication changes, pregnancy, surgery, or aging
- Cleaner records for telehealth and primary care follow-up

---

## Node 1 - UroSync Mirror Hub

### Core hardware

- **Compute module:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for watchdog, TDMA slot timing, brownout-safe radio control, and OTA rollback trigger
- **Sub-GHz radio:** Semtech SX1262 with matched 868 MHz antenna path
- **Display:** 8 inch two-way smart mirror LCD assembly, 1280x800
- **Audio:** MAX98357A I2S amplifier + 3 W speaker for spoken prompts
- **Connectivity:** Ethernet, Wi‑Fi, BLE, optional Quectel BG95-M3 LTE fallback on USB
- **Power:** 12 V input -> 5 V / 6 A buck for CM4/display -> 3V3 buck for RP2040/radio; UPS HAT with 2x18650 for 90 min runtime

### Responsibilities

- Maintains privacy-preserving local patient profile and household policy
- Bridges Sub-GHz node traffic to MQTT topics and REST APIs
- Runs edge inference when internet is unavailable
- Schedules strip lot calibration, bottle tare calibration, and mat zeroing
- Hosts mirror UI, caregiver portal, and PDF export generation
- Applies rules engine for alert severity: self-care, clinician follow-up, urgent safety

### Hub pin / interface map

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 I2C0 | RTC DS3231, INA219, TMP117 panel temp |
| RP2040 UART0 | CM4 heartbeat / watchdog control |
| CM4 DSI | Mirror LCD |
| CM4 I2S | MAX98357A speaker amplifier |
| CM4 USB2 | LTE modem or service dongle |
| CM4 Ethernet | Home router / PoE splitter |

---

## Node 2 - Toilet Dock Analyzer

The Toilet Dock clamps to the toilet rim or side wall and uses a replaceable microfluidic strip cassette that briefly wicks a urine sample during a void or after a guided dip-strip workflow.

### Electronics

- **MCU:** ESP32-S3-WROOM-1-N16R8
- **Spectral/color sensing:** AMS **AS7341** for multi-channel color and chemistry patch reflectance
- **Impedance sensing:** **AD5933** to estimate conductivity / specific-gravity proxy through cartridge electrodes
- **Analog front end:** **ADS1115** for cartridge channels and reference photodiode
- **Load cell:** 5 kg single-point + **HX711** under sample cup / drip reservoir to estimate volume over time for uroflow features
- **Acoustic sensor:** **ICS-43434** I2S MEMS microphone for stream acoustics and splash profile
- **Presence / geometry:** **VL53L0X** ToF for seat occupancy and dock alignment
- **Actuation:** 28BYJ-48 + ULN2003 to advance sealed strip cassette; miniature peristaltic pump for rinse cycle in premium design
- **UX:** 1.9 inch ST7789 display, RGB LED bar, capacitive clean button, maintenance reed switch

### Measured features

- Color index / turbidity estimate
- Strip lane states: leukocyte, nitrite, blood, protein, ketone, glucose, pH zone
- Conductivity-derived specific gravity proxy
- Approximate void volume, peak flow, flow duration, intermittency
- Night vs day void timing and frequency

### Power architecture

- 12 V / 2 A adapter in a GFCI-protected bathroom outlet
- 5 V buck for motor / auxiliaries
- 3V3 LDO island for analog front end and sensors
- TVS + reverse protection because the node lives in a wet environment

### Mechanical and sanitation notes

- Strip cassette never cross-contaminates lanes; each test advances a new section
- User-contact surfaces are ABS + silicone and fully wipe-clean
- No camera is aimed at the body; privacy is preserved by chemistry, acoustics, weight, and timing only

---

## Node 3 - Night Safety Mat

A thin mat placed at the bedside or toilet entrance to quantify nighttime transfer safety.

### Sensors and functions

- **MCU:** STM32L4R5
- **Mass / balance:** 4x 50 kg load cells into **ADS1232** for center-of-pressure shift and asymmetry
- **Foot temperature:** 2x **TMP117** for bilateral plantar skin temperature trend
- **Presence mapping:** **VL53L5CX** 8x8 ToF overhead or edge-mounted occupancy field for foot placement confidence
- **Motion:** **BMA400** low-power accelerometer for bump/slip detection if mat is moved
- **UX:** low-glare side LED strip for adaptive path light; small piezo for calibration cues

### Derived features

- Time to stand after loading the bed edge
- Sway index for 5-second post-stand window
- Left/right loading asymmetry
- Night-trip count and duration
- Potential orthostatic dizziness proxy when step initiation is delayed after standing

### Safety behaviors

- If sway exceeds threshold, hub can recommend sitting back down and trigger caregiver push notification
- If no return-to-bed event is seen within configurable window, system checks for possible prolonged bathroom event

---

## Node 4 - Hydration Bottle Tag

A universal smart coaster/tag that magnetically docks to a bottle sleeve or cup base.

### Hardware

- **MCU:** nRF52840
- **Sub-GHz radio:** SX1262 for direct uplink to hub outside BLE range
- **Mass sensing:** mini ring load cell + HX711
- **Motion:** LIS2DW12 accelerometer for sip / lift pattern detection
- **Haptics:** coin vibration motor for discreet hydration reminders
- **Charging:** MCP73831 Li-ion charger over USB-C
- **Battery:** 1200 mAh LiPo, ~4 weeks between charges at 1 minute beacon interval

### Logic

- Detects lifts, tilt, and mass delta to distinguish sipping from accidental bumps
- Learns user bottle refill habit, not just raw drinking events
- Correlates water intake timing with urine concentration and nocturia burden

---

## Node 5 - Bath Sentinel

### Hardware

- **MCU/radio:** STM32WL55JC (integrated Sub-GHz)
- **Humidity / temperature:** Sensirion SHT31
- **VOC / odor context:** Sensirion SGP41
- **Ambient light:** VEML7700 for adaptive low-glare night mode
- **Leak detection:** resistive floor trace + comparator input
- **Actuation:** 12 V fan relay, 12 V amber floor light strip MOSFET, optional towel-warmer smart plug control

### Purpose

- Reduces slippery, humid conditions during night trips
- Flags persistent odor and humidity patterns that may correlate with incontinence cleanup burden or ventilation failures
- Detects sink/toilet supply leaks before flooring damage occurs

---

## Network and Protocol

UroSync uses a **deterministic 868 MHz TDMA mesh** with household-scoped AES-CTR encryption. BLE is used only for provisioning, direct phone setup, and near-field service.

- **Nominal beacon interval:** 60 s, reduced to 5 s during active bathroom session
- **Max encrypted payload:** 48 bytes
- **CRC:** CRC-16/CCITT
- **Addressing:** 16-bit node IDs, user-profile ID, session nonce
- **QoS:** alert frames require ACK within 300 ms; health telemetry can be retried lazily
- **Privacy:** no raw audio leaves the Toilet Dock; only derived uroflow/acoustic features are transmitted

See [`docs/protocol_spec.md`](docs/protocol_spec.md) and [`firmware/common/protocol.h`](firmware/common/protocol.h).

---

## ML Stack

### 1. HydroCast LSTM
Inputs:
- bottle intake events and timing
- urine color and specific-gravity proxy
- ambient temperature and bathroom humidity
- prior-day nocturia count

Outputs:
- 6 h and 24 h dehydration risk
- suggested intake timing windows

### 2. UTIWatch XGBoost
Inputs:
- leukocyte / nitrite / blood / protein strip features
- void frequency, urgency proxy, nocturia change
- hydration status and temperature trends

Outputs:
- low / medium / high UTI suspicion
- confidence and recommended action tier

### 3. NocturiaTrend Temporal Fusion Transformer
Forecasts next-7-night nocturia burden and detects escalation after medication, pregnancy, postpartum changes, or worsening LUTS.

### 4. NightSafe Random Forest
Predicts fall risk from transfer latency, sway, foot-temp asymmetry, lighting, humidity, and prolonged bathroom dwell time.

### 5. StoneShield Survival / XGBoost model
Estimates kidney stone recurrence adherence risk from sustained hydration, urine concentration trend, and nocturnal urine burden.

### 6. NudgeBandit
Chooses reminder timing, messaging tone, and caregiver escalation threshold to maximize hydration adherence while minimizing annoyance.

---

## Firmware Layout

```text
firmware/
├── common/              # protocol, CRC, mesh queue helpers
├── hub/                 # coordinator + mirror watchdog logic
├── toilet-dock/         # strip analysis and uroflow capture
├── night-mat/           # balance + transfer safety logic
├── bottle-tag/          # intake logging and reminders
└── bath-sentinel/       # ventilation + leak + lighting logic
```

Each node is written in portable C with hardware abstraction points noted inline so the logic can be compiled on a host machine for validation.

---

## BOM Summary

Detailed per-node BOMs live in [`hardware/bom/`](hardware/bom/).

| Node | Approx BOM (prototype qty 1) |
|------|------------------------------|
| Mirror Hub | $188 |
| Toilet Dock Analyzer | $96 |
| Night Safety Mat | $71 |
| Hydration Bottle Tag | $29 |
| Bath Sentinel | $24 |

Total example 5-node starter household: **~$408 BOM** before enclosure, PCB fab, and disposable strip consumables.

---

## Disposable Chemistry Strip Concept

- Roll cassette with indexed test pads for leukocyte, nitrite, blood, protein, ketone, glucose, pH, and reference white patch
- Sealed desiccant chamber with lot code in QR label
- Calibration file loaded into hub during cassette install
- Target cost: $0.28 to $0.65 per strip at scale depending on lane count and barrier film

---

## Mobile App

The React Native app provides:

- hydration dashboard and daily score
- night-trip timeline
- bathroom safety alerts
- symptom journaling and notes
- care-circle sharing for family / clinician review
- consumable management for strip cassette reorder reminders

---

## Safety, Privacy, and Regulatory Notes

- Not a diagnostic device as delivered in this repo; intended as a wellness and early-warning platform
- All body-sensitive data stays local first; cloud sync is opt-in
- No camera or microphone records intelligible speech in the bathroom path
- Chemical strip interpretation requires validation before medical claims
- GFCI, isolated low-voltage design, and sealed enclosures are mandatory for deployment in wet locations

---

## Repository Structure

```text
UroSync/
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

## Buildable Next Steps

1. Fabricate the 5 prototype PCBs from the included starter schematics.
2. Replace the host-compiled firmware HAL stubs with target SDK drivers.
3. Collect initial strip-calibration and hydration data from consenting users.
4. Validate ML thresholds against clinician-reviewed labels.
5. Package the mirror UI and caregiver workflows for pilot deployments.

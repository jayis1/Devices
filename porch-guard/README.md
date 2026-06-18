# PorchGuard

**AI-powered multi-node package delivery security & entry intelligence system.** Keeps your deliveries safe, your mail tracked, and your doors smart — autonomously.

---

## What It Does

PorchGuard is a 4-node system that turns any home entrance into an intelligent, self-defending delivery and entry system:

1. **Detects** packages the moment a courier leaves them — classifies parcel size, logs the drop, arms the porch for theft monitoring
2. **Recognizes** who's at the door — resident vs. known courier vs. unknown stranger, using on-device person re-identification
3. **Catches** porch pirates in the act — a temporal ML model watches for the loiter → approach → grab → flee-with-package behavior pattern and triggers the siren + app + cloud clip *before* they escape
4. **Tracks** your mail — a long-range solar mailbox node reports letter/parcel arrival, mailbox tampering, and low battery, even 100m from the house
5. **Controls** your doors — a BLE smart lock + garage relay grant one-time courier codes so drivers can drop parcels *inside* your garage instead of on the porch (no more theft, no more weather damage)
6. **Alerts** you instantly — delivery, mail, visitor, pirate, tamper, door-left-open, all with push notifications + optional local 100 dB siren
7. **Learns** your routines — knows your usual delivery windows, regular couriers (FedEx guy, Amazon van), and flags anomalies (someone at the door at 3am)

All safety-critical event detection runs on the porch camera and hub over a dedicated Sub-GHz LoRa mesh — the siren can fire even if WiFi is down. The hub bridges to WiFi/cloud for clips, the dashboard, and the mobile app; the lock talks BLE to your phone and the hub.

### The Problem It Solves

- **Porch piracy:** ~262 million packages were stolen from US doorsteps in 2023 (SafeWise), costing households $12B+ and rising every year. PorchGuard detects the theft behavior pattern and sounds the alarm *while the pirate is still on the porch*, plus captures a cloud clip for police.
- **Missed / weather-damaged deliveries:** Parcels left in rain, snow, or direct sun. PorchGuard's one-time courier codes let drivers place packages inside your garage — secure and sheltered.
- **Mail theft:** Mailbox "fishing" and smash-and-grab are rising crimes. The mailbox node detects tamper/open events and reports arrival of letters and parcels over long-range LoRa.
- **Key management chaos:** Hiding keys under mats, sharing codes that never expire, lost spare keys. PorchGuard issues per-courier, per-delivery, time-limited one-time codes.
- **False alarms from cheap doorbell cameras:** A leaf blows → "person detected." PorchGuard's mmWave presence + PIR + camera fusion + temporal behavior model eliminates false positives.
- **Battery anxiety on smart locks:** Most smart locks die in weeks. The nRF52 lock node sleeps at <5µA and runs 8-12 months on 4×AA.
- **"Did my package arrive?"** PorchGuard tells you the second a parcel lands, with a thumbnail, courier ID, and size estimate — no more refreshing tracking pages.

PorchGuard watches, recognizes, deters, and secures — so you never come home to an empty porch again.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                         PORCHGUARD SYSTEM                                     │
│                                                                               │
│  ┌───────────────────┐   Sub-GHz    ┌───────────────────┐                     │
│  │ MAILBOX NODE       │◄───────────►│                   │                     │
│  │ (at curb/door)     │  915MHz LoRa │   HUB NODE        │──── WiFi6 ───►Cloud │
│  │ Reed+load cell    │  (long range)│  (RP2040 +        │               Dashboard
│  │ Temp/light/tamper  │              │   ESP32-C6)       │               + ML    │
│  │ Solar + coin cell  │              │                   │               Pipeline│
│  └───────────────────┘              │  Edge ML:         │               + Clips │
│                                     │  pirate behavior  │               + Alerts│
│  ┌───────────────────┐              │  person re-ID     │                     │
│  │ PORCH CAMERA NODE  │◄────────────►│  Siren controller│─── BLE ──────► Mobile │
│  │ (above door)       │ Sub-GHz mesh │  TFT: live status │                 (React │
│  │ OV2640 + PIR       │ + WiFi (clips│                   │                 Native)│
│  │ mmWave presence     │  upload)     │                   │                     │
│  │ Mic + speaker       │              └─────────┬─────────┘                     │
│  └───────────────────┘                         │ BLE                           │
│                                                ▼                               │
│                                     ┌───────────────────┐                      │
│                                     │ LOCK NODE          │                      │
│                                     │ (on door/garage)   │                      │
│                                     │ nRF52840 + motor   │                      │
│                                     │ deadbolt + keypad  │                      │
│                                     │ + garage relay     │                      │
│                                     │ 4×AA, 8-12 months  │                      │
│                                     └───────────────────┘                      │
│                                                                               │
│  ┌───────────────────────────────────────────────────────────────────────┐   │
│  │                    CLOUD / EDGE SOFTWARE                              │   │
│  │  ┌─────────┐  ┌──────────────┐  ┌───────────────────────┐             │   │
│  │  │Dashboard│  │ ML Pipeline  │  │ Mobile App            │             │   │
│  │  │ (React) │  │ Package det  │  │ Live porch status     │             │   │
│  │  │ Live    │  │ Person re-ID │  │ Push: delivery/pirate │             │   │
│  │  │ Clips   │  │ Pirate behav │  │ Unlock + one-time code│             │   │
│  │  │ Delivery│  │ Mail classif  │  │ Clip viewer          │             │   │
│  │  │ Log     │  │ Anomaly       │  │ Courier code issue   │             │   │
│  │  └─────────┘  └──────────────┘  └───────────────────────┘             │   │
│  └───────────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per system)

The brain. Bridges the Sub-GHz mesh to WiFi/BLE/cloud. Runs edge ML inference and the siren.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | RP2040 + ESP32-C6 | RP2040 runs mesh + ML + display; ESP32-C6 handles WiFi/BLE |
| Radio | SX1262 (868/915MHz) | Sub-GHz LoRa mesh to all nodes (+20dBm high power) |
| Display | 2.4" IPS TFT (ILI9341) | Local status: porch armed, parcel detected, last event |
| Storage | W25Q256 32MB Flash + MicroSD | Event log, clip cache, ML model cache, OTA |
| Audio | 100 dB piezo siren + MAX9814 mic | Intruder alarm + ambient sound analysis |
| Power | 5V USB-C + LiPo 2500mAh backup | Stays running during power outage (security!) |
| Connectors | 4× I2C, 2× UART, 8× GPIO | Expansion |
| LEDs | RGB status LED | System state: armed/disarmed/alarm |

**Hub firmware responsibilities:**
- Mesh network coordinator (TDMA scheduler for all nodes)
- Event aggregation + time-series buffering
- WiFi uplink to MQTT broker (QoS 1, TLS) + clip upload to cloud storage
- BLE GATT server for mobile app (status, unlock, code issue)
- TFT dashboard rendering (live porch status, parcel count, last event, siren state)
- Local siren triggers (100 dB for pirate/tamper alerts)
- OTA update distribution to all nodes
- TFLite Micro inference: pirate behavior temporal model, person re-ID matching

### 2. Porch Camera Node (1 per system)

The eyes. Mounts above the front door, watches the porch 24/7. Fuses camera + PIR + mmWave to eliminate false positives.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 (ESP32-S3-WROOM-1, 8MB PSRAM) | Camera + WiFi clip upload + Sub-GHz mesh |
| Radio | SX1261 (868/915MHz) | Sub-GHz mesh client (event channel — works without WiFi) |
| Camera | OV2640 (2MP, 160° FOV) | Porch imaging, parcel + person detection |
| PIR | AM612 (low-power) | Motion wakeup (camera stays in low-power until PIR fires) |
| mmWave | HLK-LD2410 (24GHz presence) | Sub-meter presence detection — distinguishes person vs pet vs package |
| Microphone | INMP441 (I2S MEMS) | Two-way audio + glass-break / knock detection |
| Speaker | MAX98357A + 28mm speaker | Two-way talk, deterrent chime |
| Illumination | White LED + IR 940nm array | Day/night imaging |
| Storage | MicroSD | Local clip buffer (loop recording) |
| Power | USB-C (wired to doorbell transformer) + 1F supercap | Survives brief power dips |
| Enclosure | IP65, UV-resistant | Outdoor rated |
| Tamper | Tilt sensor (SW-420) | Detects camera being moved/covered |

**Porch camera firmware (always watching):**
- Low-power PIR-gated capture: camera sleeps until PIR/mmWave fires, then wakes in <300ms
- Package detection: on-device CNN (TFLite Micro) detects a parcel appearing on the porch → "DELIVERY" event with size class (small/medium/large)
- Person detection + re-ID: lightweight embedding model matches against known residents/couriers; unknown stranger flagged
- Pirate behavior: streams presence frames to hub, hub runs temporal LSTM (loiter → approach → grab → leave-with-parcel) → siren + clip
- Two-way audio: app can initiate talk; auto deterrent chime on unknown stranger after 10s loiter
- Clip upload: 5s pre + 5s post event clip to MicroSD, then WiFi → cloud when available
- Knock/glass-break detection on mic
- Reports events over Sub-GHz mesh *immediately* (siren path is independent of WiFi)

### 3. Mailbox Node (1 per system)

The long-range outpost. Lives at the curb or a far door, reports mail/package arrival and tamper.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | STM32L011K4 (Cortex-M0+, 8KB RAM) | Ultra-low-power, coin-cell friendly |
| Radio | SX1261 (915MHz LoRa, SF9-12) | Long range to hub (up to 500m), mesh client |
| Mail sensor | Reed switch (door open) | Mailbox door open detection |
| Load cell | 1kg + HX711 (low power) | Mail weight — letter vs parcel vs empty |
| Light | ALS-PT19 | Door-open confirmation + ambient |
| Temperature | DS18B20 | Mailbox temp (heat damage to parcels) |
| Tamper | Tilt/accel (LIS2DH12) | Smash-and-grab / fishing detection |
| Power | 2× CR2032 + 0.5W solar + MCP73831 | Months+ battery, solar tops up |
| Enclosure | IP67, UV-rated | Fully outdoor, mailbox-mounted |

**Mailbox node firmware (battery-critical):**
- Deep sleep <5µA, wakes on reed switch (door open) or 5-min temperature/light poll
- Weight-based mail classification: <20g = letter, 20-200g = thick letter/small parcel, >200g = parcel
- Tamper detection: tilt sensor fires on mailbox being shaken/moved → instant TAMPER_ALERT over LoRa SF12 (max range)
- Low-battery alert at 15%
- Reports to hub over LoRa SF9 (long range, robust); hub relays to cloud/app
- Solar-aware: boosts poll rate during daylight, deep-sleeps at night

### 4. Lock Node (1 per system, on door + garage)

The hands. BLE smart lock with motorized deadbolt + keypad + garage door relay for secure courier parcel drop.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52840 (Cortex-M4F, BLE 5.0) | BLE lock control + keypad + motor |
| Radio | BLE 5.0 (built-in) + SX1261 (optional Sub-GHz) | BLE to phone/hub; Sub-GHz fallback for out-of-BLE-range |
| Lock motor | Stepper (NEMA-style) + A4988 driver | Motorized deadbolt (auto-lock/unlock) |
| Keypad | 12-key capacitive | PIN entry, one-time courier codes |
| Garage relay | Relay + optocoupler | Trigger garage door opener for parcel drop |
| Door sensor | Reed switch | Door state (open/closed/left-open) |
| Tamper | Internal tilt/accel (LIS2DH12) | Detect lock being forced/removed |
| Power | 4× AA + AP2112-3.3 | 8-12 months battery |
| Enclosure | Interior escutcheon + exterior keypad | Standard deadbolt retrofit |

**Lock node firmware (security + battery):**
- BLE GATT peripheral: phone unlocks via app (proximity auto-unlock optional)
- Hub unlocks via BLE command (cloud/app relay)
- One-time codes: app issues a 6-digit code valid for a window (e.g., 1 hour) tied to a delivery — courier enters code, deadbolt opens, garage relay can also fire
- Auto-lock: re-locks 30s after unlock if door closed
- Door-left-open alert: reed switch reports open >2min → app alert
- Forced-entry detection: tilt + motor back-EMF anomaly → TAMPER_ALARM over BLE→hub→siren
- Low power: system-off RAM <3µA between events
- Keypad anti-shoulder-surf: random digit scramble on wake

---

## Communication Protocol

### Sub-GHz Mesh (SX1262/61, 868/915MHz LoRa)

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU) / 915 MHz (US) |
| Modulation | LoRa SF7 (normal) / SF9 (mailbox long-range) / SF12 (pirate/tamper alarm) |
| Bandwidth | 125 kHz |
| TX Power | +14 dBm (nodes) / +20 dBm (hub) |
| Range | 30m indoor (normal) / 200m (SF9) / 500m+ (SF12) |
| Protocol | Custom TDMA (hub is coordinator) |
| Slot Duration | 100ms per node |
| Cycle Time | 500ms (5 slots) |

### TDMA Frame Structure

```
│ SLOT 0 (HUB) │ SLOT 1 (CAMERA) │ SLOT 2 (MAILBOX) │ SLOT 3 (LOCK) │ SLOT 4 (CTRL) │
│   100ms      │    100ms        │    100ms         │   100ms       │   100ms      │
│
Total frame: 500ms
Slot 0: Hub broadcasts sync + commands
Slot 1: Porch camera uplink (event/alert/telemetry)
Slot 2: Mailbox uplink (mail/tamper/telemetry) — uses SF9 long-range when polled
Slot 3: Lock uplink (door state/tamper) — usually BLE, Sub-GHz on fallback
Slot 4: Control/ACK/retransmit/OTA

Pirate/Tamper Override:
  When porch camera detects pirate behavior >0.8, it immediately broadcasts
  PIRATE_ALERT on SF12 (max range + robustness). Hub halts TDMA, activates
  100 dB siren, relays clip ref to app + cloud. Same for lock forced-entry.
```

### Mesh Packet Format

```
[ PREAMBLE(4) | SYNC(2) | LEN(1) | SRC_ID(1) | DST_ID(1) | TYPE(1) | PAYLOAD(0-50) | CRC16(2) ]

TYPE values:
  0x01 = CAMERA_DATA (presence, person_id, parcel, pirat_risk, tamper, temp)
  0x02 = MAILBOX_DATA (door_state, weight_mg, mail_class, temp, tamper, battery)
  0x03 = LOCK_DATA (door_state, lock_state, battery, last_code_id, tamper)
  0x04 = COMMAND (unlock, lock, issue_code, garage_relay, siren, arm/disarm)
  0x05 = ACK
  0x06 = OTA_BLOCK (firmware update chunk)
  0x07 = CALIBRATION
  0x08 = PIRATE_ALERT (critical — highest priority, SF12 broadcast)
  0x09 = TAMPER_ALERT (forced entry / cover removed)
  0x0A = DELIVERY_EVENT (parcel detected, mail arrived)
  0x0B = HEARTBEAT
  0x0C = CLIP_REF (pointer to MicroSD/cloud clip)
```

### BLE Lock Channel (nRF52840)

| Parameter | Value |
|-----------|-------|
| Profile | Custom GATT + standard HID for keypad |
| Advertising | 100ms (when armed) / 1s (sleep) |
| Connection | Encrypted (LE Secure Connections, AES-CCM) |
| Range | ~10m |
| Bonding | Phone + hub paired at setup |

---

## AI / ML Pipeline

### 1. Package Detection (on porch camera, TFLite Micro)

- Input: 320×240 RGB frame from OV2640
- Model: MobileNet-SSD lite, INT8 quantized, ~210 KB
- Classes: {no-parcel, small-parcel, medium-parcel, large-parcel, envelope}
- Output: parcel presence + size class + bounding box + confidence
- Triggers: parcel *appearing* (not present in prev frame) → DELIVERY_EVENT → app "Package delivered!" with thumbnail
- Triggers: parcel *disappearing* while a stranger is present → pirate suspicion

### 2. Person Detection + Re-ID (on porch camera + hub)

- Person detection: lightweight MobileNet head, INT8, ~120 KB
- Re-ID embedding: 128-d embedding from a small CNN, INT8, ~90 KB
- Gallery: stored embeddings for residents (enrolled via app) and recurring couriers (auto-learned after 3+ visits)
- Matching: cosine similarity vs gallery; >0.6 = known, <0.4 = stranger
- Output: person present + identity label + confidence
- Unknown stranger loitering >10s → deterrent chime + app "Unknown person at door"

### 3. Pirate Behavior Detection (on hub, TFLite Micro, temporal)

- Input: rolling 30s window of presence/parcel/person events (sampled at 2Hz = 60 steps)
- Features: mmWave presence distance, parcel-present flag, person-present flag, person-identity, motion vector
- Model: 1D-CNN + LSTM hybrid, INT8, ~110 KB
- Output: pirate risk score (0-1)
- Behavior pattern: loiter (presence, no parcel interaction) → approach porch → parcel disappears → person leaves with object
- Triggers: >0.6 = warning (chime + app "Possible theft"), >0.8 = CRITICAL (siren + clip + cloud), >0.95 = EMERGENCY (siren + 911-style alert)

### 4. Mail Classification (mailbox node rule + cloud refinement)

- Edge (STM32L0): weight-based — <20g letter, 20-200g thick/small parcel, >200g parcel
- Cloud: learns your typical mail weight distribution; flags anomalies (unexpected heavy parcel = possible theft plant)
- Output: mail class + weight + arrival timestamp

### 5. Anomaly Detection (cloud, time-series)

- Input: 30-day event log (delivery times, visitor times, mail arrivals, door openings)
- Model: Isolation Forest + seasonal baseline
- Output: anomaly flag for events at unusual times (visitor at 3am, delivery at midnight)
- Triggers: app alert "Unusual activity: visitor at 3:14 AM"

---

## Cloud Dashboard

React.js web app + Python FastAPI backend.

- Real-time porch status (armed/disarmed, parcel present, person present, siren state)
- Live event feed: deliveries, visitors, mail, alerts (all with clips)
- Delivery log: courier, time, size, clip, outcome (delivered / stolen / retrieved)
- Courier gallery: known couriers with visit count + last seen
- One-time code management: issue/revoke/view active courier codes
- Lock history: every lock/unlock with source (app/code/keypad/auto)
- OTA firmware update management
- Multi-entrance support (front door, back door, garage)

---

## Power Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    POWER DISTRIBUTION                                     │
│                                                                          │
│  HUB:    USB-C 5V ──► MCP73831 ──► LiPo 2500mAh                         │
│          (backup: hub + siren run 12+ hrs on battery)                    │
│          AP2112-3.3 (logic) + AP6212-1.8 (flash)                        │
│                                                                          │
│  CAMERA: USB-C 5V (doorbell transformer 16-24VAC → 5V buck)             │
│          1F supercap — survives 5s power dips (no missed clip)          │
│                                                                          │
│  MAILBOX: 2× CR2032 (3V) + 0.5W solar panel ──► MCP73831 trickle        │
│           Months+ runtime; solar tops up during daylight                 │
│           System-off <5µA, active ~15mA for <2s per event                │
│                                                                          │
│  LOCK:   4× AA (6V) ──► AP2112-3.3                                        │
│          8-12 months; system-off <3µA, motor peak ~250mA for <1s          │
└─────────────────────────────────────────────────────────────────────────┘
```

**Critical:** The hub and camera have backup power. If a thief cuts the doorbell wire (power to camera), the camera's supercap fires a final TAMPER_ALERT and the hub (on battery) keeps the siren + mesh + cloud alive. The mailbox is fully solar/battery and unreachable from the porch.

---

## Bill of Materials (Summary)

| Node | Est. BOM Cost | Key Components |
|------|--------------|----------------|
| Hub | ~$23 | RP2040, ESP32-C6, SX1262, ILI9341 TFT, W25Q256, piezo siren, LiPo 2500mAh |
| Porch Camera | ~$19 | ESP32-S3 (PSRAM), SX1261, OV2640, HLK-LD2410 mmWave, AM612 PIR, INMP441, MAX98357A, supercap |
| Mailbox | ~$15 | STM32L011, SX1261, HX711, 1kg load cell, DS18B20, LIS2DH12, CR2032 + solar |
| Lock | ~$17 | nRF52840, A4988 stepper, keypad, relay, LIS2DH12, 4×AA |
| **Total** | **~$74** | Full 4-node system |

See `hardware/bom/` for detailed per-node BOMs.

---

## Assembly Overview

1. **Hub:** Indoors near the entrance, USB-C power. Has display + battery backup. Wall-mount or shelf.
2. **Porch camera:** Above front door, wired to doorbell transformer (16-24VAC). Connects to WiFi for clip upload + Sub-GHz for events. IP65 enclosure.
3. **Mailbox:** Mount in/under mailbox. CR2032 + solar. Long-range LoRa reaches hub.
4. **Lock node:** Retrofit deadbolt (interior motor + exterior keypad). Garage relay wires to opener. 4×AA.

Detailed assembly in `docs/assembly_guide.md`.

---

## Security Features

| Feature | Detection | Response |
|---------|-----------|----------|
| Porch piracy | Pirate behavior pattern (loiter+grab+flee) | Siren (100 dB) + clip + app + cloud |
| Package theft (parcel gone) | Parcel disappears while stranger present | CRITICAL alert + clip |
| Mail theft / tamper | Mailbox tilt or door-forced | TAMPER_ALERT (SF12) + app |
| Lock forced entry | Tilt + motor back-EMF anomaly | Siren + app + clip |
| Camera tamper | Tilt/cover sensor | TAMPER_ALERT + cloud "camera covered" |
| Door left open | Reed open >2min | App alert |
| Power cut (camera) | Camera power lost, hub on battery | "Camera power lost — possible tamper" |
| Unusual-hours visitor | Time-series anomaly | App "Unusual activity" |
| Unknown loiterer | Stranger present >10s | Deterrent chime + app |

---

## Getting Started

1. Set up the hub (USB-C power, connect to WiFi via mobile app over BLE)
2. Install porch camera (wire to doorbell transformer, 15 min)
3. Mount mailbox node (5 min)
4. Install lock node + garage relay (30 min)
5. Enroll residents (walk past camera ×3 each for re-ID gallery)
6. Run calibration (`scripts/calibrate_sensors.py`)
7. Issue your first courier code and arm the porch!

---

## Project Structure

```
porch-guard/
├── README.md              # This file
├── schematic/              # KiCad projects (one per node)
├── firmware/               # C source per node + shared common/
├── hardware/               # BOMs, enclosures
├── software/               # Cloud dashboard, ML pipeline, mobile app
├── scripts/                # Deployment, calibration
└── docs/                   # Architecture, API, protocol, assembly
```

---

## License

MIT — build it, sell it, improve it.

---

*Part of the [Devices](../README.md) collection — complex hardware+software systems that improve daily life.*
# RoutineSync

**AI-powered executive-function and daily-routine support system** — a multi-node home coordination platform that helps people leave the house with the right items, find misplaced essentials fast, start and end focus sessions with less friction, and reduce the daily tax of ADHD, overwhelm, and routine breakdown.

## What It Solves

Millions of people lose time and emotional energy every day to executive-function friction:

- **Departure failure** — keys, wallet, ID badge, transit pass, medication pouch, lunch, or charger get left behind.
- **Item invisibility** — an object is physically nearby, but effectively lost because attention and working memory are overloaded.
- **Task-switching paralysis** — starting focused work is hard, and stopping hyperfocus before meetings, school pickup, or sleep is just as hard.
- **Routine instability** — mornings, school handoffs, work transitions, and bedtime collapse when one step slips.
- **Nagging UX** — traditional reminders fire at the wrong time and create notification fatigue instead of useful support.

**RoutineSync** turns the home into an executive-function prosthetic. It combines doorway verification, UWB item finding, room-level focus sensing, and adaptive timing models to catch missing essentials before departure, surface the next doable step, and gently guide transitions with low-friction cues.

---

## System Architecture

```text
┌─────────────────────────────────────────────────────────────────────────────────┐
│                         RoutineSync Cloud / Edge Stack                          │
│ FastAPI + MQTT + SQLite/PostgreSQL + object store + ML training pipeline        │
│                                                                                 │
│ Models                                                                          │
│ • ExitRiskNet         - predicts missed-item / missed-step probability          │
│ • AnchorLocate        - room-level item-presence and likely-last-seen ranking   │
│ • FocusStateNet       - focus / distracted / hyperfocus / transition-needed     │
│ • NudgeBandit         - chooses cue type and timing with low notification debt  │
│ • RoutineDrift        - forecasts schedule instability over 7 days              │
└──────────────────────────────────┬──────────────────────────────────────────────┘
                                   │ MQTT over TLS / HTTPS
                                   │
                   ┌───────────────┴────────────────┐
                   │      RoutineSync Hub Gateway    │
                   │ Raspberry Pi CM4 + RP2040 +     │
                   │ nRF52840 + DWM3001C UWB anchor  │
                   └───────┬───────────────┬──────────┘
                           │               │
                    BLE 5.3 mesh      UWB ranging / BLE AoA
                           │               │
 ┌─────────────────────────┼───────────────┼───────────────────────────────┐
 │                         │               │                               │
 │                 ┌───────┴────────┐  ┌───┴─────────────┐         ┌───────┴────────┐
 │                 │ Doorway Dock   │  │ Focus Beacon    │         │ Object Tag ×N  │
 │                 │ item tray +    │  │ room state      │         │ keys/wallet/    │
 │                 │ checklist      │  │ coach + cues    │         │ backpack/etc.   │
 │                 └────────────────┘  └─────────────────┘         └────────────────┘
 │
 └─ Mobile app (React Native) for setup, routines, item search, and family sharing
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **RoutineSync Hub Gateway** | Raspberry Pi CM4 + RP2040 + nRF52840 + Qorvo DWM3001C | Local automation engine, MQTT bridge, model host, OTA coordinator, family dashboard | 12V/3A wall input + 2-cell UPS | Ethernet, Wi-Fi, BLE 5.3, UWB |
| **Doorway Dock** | ESP32-S3-WROOM-1 + DWM3000 + HX711 + PN532 | Departure checklist, tray/item verification, tag ranging anchor, NFC badge/pass verification | 12V DC input -> 5V/3.3V rails | BLE 5.0, UWB |
| **Focus Beacon** | ESP32-C6-WROOM-1 + LD2410B + SGP40 + TSL2591 | Focus-state sensing, room cues, adaptive break prompts, environment context | USB-C 5V | Wi-Fi 6, BLE 5.3 |
| **Object Tag** | nRF52840 + DWM3000 + LIS2DW12 | Misplaced-item finding, movement history, buzzer/LED locate, last-seen logging | 150 mAh LiPo or CR2450 variant | BLE 5.0, UWB |

---

## Daily User Experience

1. A parent drops keys, badge, and medication pouch onto the **Doorway Dock** tray before bed.
2. Overnight, the hub sees tomorrow is an office day and loads the "commute" routine.
3. In the morning, the dock verifies wallet mass, NFC transit card presence, and UWB proximity of the key tag.
4. The user grabs their bag and heads to the door.
5. The dock flashes amber: **"Laptop missing."** The app shows it was last seen in the office 14 minutes ago.
6. During work, the **Focus Beacon** detects a stable focus block and suppresses non-urgent cues.
7. At 3:10 PM, hyperfocus risk rises before school pickup. The beacon shifts light temperature, the tag on car keys chirps once, and the app gives a single transition prompt.
8. In the evening, the hub logs which nudges worked and adjusts future cue timing with the contextual bandit.

---

## Node 1 - RoutineSync Hub Gateway

### Core hardware

- **Compute module:** Raspberry Pi CM4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for deterministic watchdog, BLE/UWB coprocessor supervision, safe shutdown, and LED/buzzer fail-safe behavior
- **BLE coprocessor:** nRF52840 over USB/UART for always-on BLE mesh coordinator
- **UWB anchor:** Qorvo DWM3001C for room-level ranging and tag localization
- **Local UX:** 5-inch DSI touch display, RGB tower LED, piezo buzzer, front status button
- **Storage:** CM4 eMMC + industrial microSD for logs, model snapshots, and offline export
- **Power:** 12V input -> 5V/5A buck -> 3V3 low-noise buck; UPS HAT with dual 18650 cells for 2 hours ride-through

### Responsibilities

- Runs FastAPI API, MQTT bridge, local scheduler, and household routine state machine
- Maintains routine templates for workday, school day, gym, travel, bedtime, and custom scenes
- Fuses doorway events, tag movement, and room presence into a "likely missing item" graph
- Hosts edge inference for low-latency departure risk and focus-state predictions
- Handles secure provisioning, OTA manifests, encrypted backups, and family member profiles

### Hub pin / interface map

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | DWM3001C UWB module |
| RP2040 UART0 | nRF52840 coprocessor |
| RP2040 I2C0 | RTC, INA219 power monitor, RGB LED driver |
| CM4 DSI | 5-inch touch display |
| CM4 USB2 | nRF52840 or service serial bridge |
| CM4 Ethernet | Router uplink |
| CM4 I2S | local chime / TTS amplifier |

### Power architecture

- 12V barrel input with ideal-diode protection
- MP1584EN 5V buck for CM4 + display
- TPS62172 3V3 rail for UWB, RP2040, sensors
- BQ25713 UPS charger with dual protected 18650 cells
- INA219 measures rail current for battery-health forecasting

---

## Node 2 - Doorway Dock

The Doorway Dock is the "airlock" for leaving home. It combines a tray, checklist, and identity verification point.

### Hardware architecture

- **SoC:** ESP32-S3-WROOM-1-N16R8
- **UWB anchor:** DWM3000 for precise distance checks to tags on keys, wallet, backpack, and badge holder
- **Weight sensing:** 4x 5 kg half-bridge load cells + HX711 to verify tray contents and item removal/addition
- **NFC/RFID:** PN532 for transit cards, office badges, and medication pouch tags
- **Display:** 2.9-inch tri-color e-paper showing checklist, weather, and countdown-to-leave
- **Door state:** reed switch or magnetic contact input to correlate departures with actual door opening
- **Presence:** VL53L1X ToF to detect a person standing at the dock
- **UX:** SK6812 status bar, piezo buzzer, capacitive confirm/snooze pads, locate-item button
- **Expansion:** 2 relay outputs for shoe-warmer, lamp, or lock signaling

### What it verifies

- Required tagged items are nearby
- Tray mass matches expected wallet/keys/lunch range
- NFC badge/pass is present for office/transit routines
- User has acknowledged optional steps (water bottle, gym gear, permission slip, homework)

### Example departure states

- **Green:** all required items verified, next event on time
- **Amber:** one item missing or confidence below threshold
- **Red:** departure deadline + multiple missing dependencies

### Pin map

| ESP32-S3 pin | Function |
|--------------|----------|
| GPIO4/GPIO5/GPIO6/GPIO7 | HX711 / load-cell multiplexer control |
| GPIO8/GPIO9 | I2C to PN532 + VL53L1X |
| GPIO10/GPIO11/GPIO12/GPIO13 | SPI to DWM3000 |
| GPIO14 | reed switch input |
| GPIO15 | buzzer PWM |
| GPIO16 | capacitive pad confirm |
| GPIO17 | capacitive pad snooze |
| GPIO18 | e-paper busy |
| GPIO19/GPIO20/GPIO21 | e-paper SPI |
| GPIO38 | LED bar data |

---

## Node 3 - Focus Beacon

The Focus Beacon lives in bedrooms, offices, or homework areas and senses whether the room is set up for sustained work or whether intervention is needed.

### Sensors and outputs

- **MCU:** ESP32-C6-WROOM-1 for Wi-Fi 6 + BLE 5.3
- **Presence radar:** Hi-Link LD2410B 24 GHz mmWave for stationary + moving presence without cameras
- **Air quality:** Sensirion SGP40 VOC index to identify stale-air distraction and stuffiness
- **Ambient light:** TSL2591 to detect glare, dimness, or evening overstimulation
- **Sound envelope:** ICS-43434 I2S microphone processed only for SPL / event cadence, never speech transcription
- **Cues:** 16-LED SK6812 ring, DRV2605L haptic puck output, mini speaker for tones, two relay outputs for desk lamp or fan
- **Buttons:** single large focus button, smaller break button

### Behavioral logic

- Starts a focus session with one press, no phone required
- Suppresses non-critical cues when focus score is high
- Detects distraction patterns such as repeated seat exits, noise bursts, or low-light strain
- Escalates from light tint shift -> short tone -> haptic puck -> app notification only if needed

### Power

- USB-C 5V input
- TPS63070 buck-boost 3V3 for LED transient resilience
- Separate filtered analog rail for microphone front-end

---

## Node 4 - Object Tag

Object Tags attach to essentials: keys, wallet, backpack, lunchbox, laptop sleeve, glasses case, inhaler pouch, or homework folder.

### Hardware

- **MCU:** nRF52840-QIAA
- **UWB:** Qorvo DWM3000 for tag-to-anchor ranging
- **Motion:** LIS2DW12 accelerometer for movement, drop, and pickup detection
- **Locate outputs:** 85 dB piezo buzzer + RGB micro-LED
- **User input:** single side button for panic-find / routine override / pairing
- **Battery:** 150 mAh LiPo + MCP73831 charger, or CR2450 low-duty variant for low-priority objects
- **Charging / data:** pogo-pin dock or USB-C tag cradle on premium version

### Power profile targets

- BLE advertising: every 2 s idle, every 200 ms during active find mode
- UWB ranging bursts: on demand or doorway preflight only
- Typical battery life: 14 days rechargeable mode, 8+ months CR2450 low-duty mode

### Tag states

- **Idle:** low-rate BLE beacon, movement logging
- **Anchored:** sitting on dock or known room anchor
- **Find Me:** buzzer + LED + fast ranging
- **Departure Critical:** elevated broadcast if required item is missing during an active departure window

---

## Communications and Protocol

RoutineSync uses **BLE 5.3 for telemetry/control** and **UWB for precise ranging**.

- **BLE topology:** hub as coordinator, nodes as peripherals or mesh relays where applicable
- **UWB usage:** symmetric double-sided two-way ranging for object-location confidence and doorway verification
- **Frame protection:** CRC-16/CCITT in application frame + BLE/UWB link-layer protection
- **Encryption:** AES-128-CTR application payload encryption with per-home network key
- **Node addressing:** 16-bit node IDs, 8-bit profile IDs, 32-bit session nonce
- **Commissioning:** BLE OOB QR code + app-assisted key exchange
- **Offline behavior:** local rules and routines continue if internet is unavailable

See [`docs/protocol_spec.md`](docs/protocol_spec.md) and [`firmware/common/protocol.h`](firmware/common/protocol.h).

---

## ML Stack

### 1. ExitRiskNet
Predicts whether a departure attempt will fail or be stressful.

**Inputs**
- current routine type
- missing required-item count
- tray mass delta
- door-open timing
- user lateness trend
- prior recovery time for similar mornings

**Outputs**
- miss risk (0-1)
- likely missing item class
- recommended nudge timing

### 2. AnchorLocate
Ranks where an item probably is.

**Inputs**
- last-seen room anchor
- UWB distances
- tag motion events
- focus room occupancy windows
- routine context (school/work/gym)

**Outputs**
- most likely room
- confidence score
- search order list

### 3. FocusStateNet
Classifies current room state.

**Classes**
- focused
- distracted
- transition-needed
- hyperfocus-risk

### 4. NudgeBandit
Chooses which cue works best with the lowest annoyance cost.

**Candidate actions**
- LED color shift
- e-paper prompt
- tag chirp
- haptic puck pulse
- short audio tone
- phone push notification

### 5. RoutineDrift
Forecasts seven-day instability from sleep timing, departure slips, skipped resets, and focus fragmentation.

---

## Repository Layout

```text
RoutineSync/
├── README.md
├── docs/
│   ├── api_spec.md
│   ├── architecture.md
│   └── protocol_spec.md
├── firmware/
│   ├── common/
│   ├── doorway-dock/
│   ├── focus-beacon/
│   ├── hub/
│   └── object-tag/
├── hardware/
│   └── bom/
├── schematic/
│   ├── doorway-dock/
│   ├── focus-beacon/
│   ├── hub/
│   └── object-tag/
├── scripts/
└── software/
    ├── dashboard/
    ├── ml-pipeline/
    └── mobile-app/
```

---

## Build and Bring-Up Flow

1. Assemble hub and flash RP2040 supervisor + install CM4 OS image.
2. Flash Doorway Dock, Focus Beacon, and Object Tag firmware.
3. Power the hub, create the household profile, and add users/routines.
4. Pair each tag to an object category and assign required/optional status per routine.
5. Run `scripts/calibrate.py` to calibrate tray mass, UWB anchors, and focus thresholds.
6. Train baseline models using synthetic + household seed data from `software/ml-pipeline/`.
7. Launch the backend and mobile app.

---

## Safety, Privacy, and Human Factors

- No cameras are required anywhere in the base system.
- Microphone data in the Focus Beacon is reduced to sound-level/event features only; no speech transcription or cloud audio upload.
- All departure decisions remain overrideable by the user.
- "Low-shame" design: the system reports friction and support opportunities, not failure language.
- Family and caregiver sharing can be scoped per routine and per object category.

---

## BOM Summary

Detailed CSV BOMs live in [`hardware/bom/`](hardware/bom/).

| Node | Estimated prototype BOM |
|------|--------------------------|
| Hub Gateway | $178 - $245 |
| Doorway Dock | $58 - $86 |
| Focus Beacon | $28 - $44 |
| Object Tag | $15 - $26 |

---

## Roadmap Ideas

- UWB room anchors for whole-home centimeter-level locating
- Smart closet hooks and backpack pegs
- WearOS / Apple Watch companion cues
- Home Assistant bridge for lock, garage, or hallway lighting automations
- Shared family "handoff mode" for school, sports, and custody transitions

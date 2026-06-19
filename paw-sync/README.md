# PawSync

**AI-powered multi-node pet health, behavior & anxiety management system for dogs and cats.** Continuously tracks cardiac activity, movement, vocalization, feeding, and home-alone behavior to detect pain, illness, lameness, and separation anxiety days before the owner notices — then closes the loop with adaptive feeding, enrichment cues, and vet-ready reports. Prevents the #1 problem in pet care: pets suffer silently because they hide symptoms by instinct.

---

## What It Does

PawSync is a 4-node wearable + ambient system that turns a pet's daily life into a continuously monitored, illness-preventing, anxiety-managing care pathway:

1. **Tracks cardiac & vital signs** — a lightweight collar tag (nRF52840 + PPG + thermistor + IMU) measures resting heart rate, HRV, activity, skin temperature, and scratching/head-shaking episodes every minute. HRV decline is the earliest validated biomarker of pain, stress, and systemic illness in companion animals — often 2–3 days before visible symptoms.
2. **Classifies activity & gait** — the collar's 9-axis IMU runs an on-device CNN that classifies behavior in real time: walking, running, resting, sleeping, scratching, head-shaking, licking, pacing, and eating. Sustained scratching indicates allergies or skin infection; head-shaking flags ear infections; licking points to GI pain or hot spots.
3. **Detects lameness** — gait asymmetry analysis (stride length, stance time, weight distribution per limb) flags orthopedic pain and arthritis flare-ups early — the same approach used in equine biomechanics, miniaturized for dogs and cats. Lameness is the most common reason for vet visits and is often missed until severe.
4. **Monitors home-alone behavior** — an ambient ESP32-S3 camera + microphone node runs on-device computer vision to classify the pet's behavior when the owner is away: pacing, vocalizing (bark/whine/meow), destructive chewing, inappropriate elimination, and resting. Separation anxiety affects ~20–40% of dogs and is severely underdiagnosed.
5. **Classifies vocalizations** — a 6-mic array + on-device TFLite model distinguishes pain vocalizations from attention-seeking, anxiety, alert, and play barks/meows — so the system knows whether a 3am whine is an emergency or a demand for attention.
6. **Manages feeding & weight** — a smart feeder node (ESP32-C6 + load cells + auger + RFID) dispenses weight-verified portions to the correct pet in multi-pet households, tracks food intake down to the gram, flags appetite loss (an early illness sign), and manages weight-loss plans automatically. Pet obesity affects >50% of dogs and cats and shortens lifespan by ~2 years.
7. **Predicts illness & pain** — a multi-modal time-series model (CNN-LSTM) fuses HRV trends, activity changes, gait asymmetry, vocalization patterns, and feeding data to produce a daily wellness score and a 7-day illness-risk forecast — validated against vet-confirmed diagnoses of pain, infection, and GI issues.
8. **Manages separation anxiety** — the system detects anxiety episodes (pacing + vocalizing + destruction) and can trigger adaptive enrichment: calming audio cues via the hub speaker, a treat dispensed from the feeder, or a notification to the owner. It learns which interventions actually reduce the pet's anxiety over time.
9. **Alerts & reports** — when risk rises, the app notifies the owner, and a structured report (vital trends, activity changes, gait analysis, vocalization clips, feeding data) is sent to the vet's portal — so the visit starts with data, not guesswork.
10. **Learns the individual** — every pet is different (breed, age, baseline HRV, activity pattern). PawSync personalizes baseline vitals, activity distribution, and vocalization patterns over the first 14 days, then watches for *deviations* — far more sensitive than population thresholds.

All body-worn and feeder nodes communicate over a BLE mesh (the collar works outdoors via local storage + sync). The hub bridges to WiFi/cellular for cloud analytics, ML training, vet portal, and the mobile app. The behavior camera is a plugged-in unit that uploads over WiFi.

### The Problem It Solves

- **Pets hide illness by instinct:** In the wild, showing weakness attracts predators. Dogs and cats mask pain and illness until it's advanced. By the time an owner notices lethargy or appetite loss, a condition may be 3–5 days in.
- **Vet visits are episodic:** An annual checkup cannot catch a 2-day HRV decline or a developing ear infection. PawSync monitors continuously and triggers a vet visit *the day* risk appears.
- **Pain is underrecognized:** Chronic pain from osteoarthritis, dental disease, and GI issues affects >60% of senior pets and is severely undertreated. HRV + gait + behavior changes detect it early.
- **Separation anxiety is hidden:** Owners aren't home to see it. PawSync's camera + mic catch pacing, vocalization, and destruction — and quantify severity for the behaviorist.
- **Pet obesity is epidemic:** >50% of pets are overweight, reducing lifespan and causing diabetes, arthritis, and heart disease. PawSync's feeder tracks intake to the gram and manages portion control automatically.
- **Multi-pet feeding is chaos:** In multi-pet homes, one pet steals another's food, medication is skipped, and appetite changes go unnoticed. RFID-verified dispensing solves this.
- **Vocalizations are ambiguous:** A bark can mean pain, anxiety, alert, or play. Owners can't tell — and may ignore a pain vocalization. PawSync's classifier resolves this.
- **Lameness is missed early:** Subtle gait changes from early arthritis or soft-tissue injury are invisible to the naked eye. IMU-based gait analysis catches them weeks before a limp appears.

PawSync senses the pet continuously, predicts illness and pain days ahead, manages anxiety and feeding, and closes the loop with enrichment and vet reports — so pets stay healthy and happy longer.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                         PAWSYNC SYSTEM                                            │
│                                                                                    │
│  ┌─────────────────────┐   BLE mesh   ┌──────────────────────┐                     │
│  │ COLLAR TAG           │◄──────────►│                        │                     │
│  │ (on pet's collar)     │  2.4GHz     │                        │                     │
│  │ nRF52840 + PPG (HR)  │             │     HUB NODE           │──── WiFi6 ──►Cloud │
│  │ + IMU + thermistor   │             │    (RP2040 +           │             Dashboard│
│  │ + LiPo + Qi charge   │             │     ESP32-C6)          │             + ML     │
│  └─────────────────────┘             │                        │             Pipeline│
│                                       │  Edge ML: wellness      │             + Vet     │
│  ┌─────────────────────┐             │   score (TFLite Micro)  │             Portal    │
│  │ BEHAVIOR CAMERA       │── WiFi6 ──►│  TFT: pet vitals +     │─── BLE ───► Mobile   │
│  │ (plugged, room)       │            │   activity timeline    │             (React   │
│  │ ESP32-S3 + OV5640     │            │  Speaker: enrichment    │              Native) │
│  │ + 6-mic array         │            │   audio cues            │                     │
│  │ Edge CV: behavior     │            └───────────┬────────────┘                     │
│  └─────────────────────┘                         │ BLE                              │
│                                       ┌──────────▼─────────────┐                     │
│                                       │  SMART FEEDER NODE      │                     │
│                                       │  (kitchen, plugged)     │                     │
│                                       │  ESP32-C6 + load cells  │                     │
│                                       │  + auger motor + RFID  │                     │
│                                       │  + water level sensor  │                     │
│                                       │  5–7 days per charge    │                     │
│                                       └────────────────────────┘                     │
│                                                                                    │
│  ┌──────────────────────────────────────────────────────────────────────────────┐ │
│  │                    CLOUD / EDGE SOFTWARE                                       │ │
│  │  ┌──────────┐  ┌───────────────┐  ┌───────────────────────┐                 │ │
│  │  │Dashboard │  │ ML Pipeline   │  │ Mobile App            │                 │ │
│  │  │ (FastAPI)│  │ Wellness      │  │ Pet vitals + wellness  │                 │ │
│  │  │ Vet      │  │  CNN-LSTM     │  │ Activity timeline      │                 │ │
│  │  │ portal   │  │ Activity      │  │ Feeding log            │                 │ │
│  │  │ Owner    │  │  classifier   │  │ Anxiety episodes        │                 │ │
│  │  │ history  │  │ Vocalization  │  │ "Check on pet" alert   │                 │ │
│  │  │ Trends   │  │  classifier    │  │ Vet report share       │                 │ │
│  │  └──────────┘  └───────────────┘  └───────────────────────┘                 │ │
│  └──────────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per system)

The brain. Bridges the BLE mesh to WiFi/cloud. Runs the edge wellness model (TFLite Micro), displays the pet's vitals + activity timeline, and plays enrichment audio cues for anxiety management.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | RP2040 + ESP32-C6 | RP2040 runs mesh + wellness ML + display; ESP32-C6 handles WiFi6/BLE5.3 |
| Radio | nRF52840 (BLE mesh) + ESP32-C6 BLE | Dual-radio: nRF52840 for mesh with body nodes, ESP32-C6 for phone/cloud |
| Display | 3.5" IPS TFT (ILI9488) | Pet vitals: HR, HRV, wellness score, activity timeline, feeding log |
| Storage | W25Q256 32MB Flash + MicroSD | Model cache, 30-day ring buffer of vital/activity/feeding events, OTA |
| RTC | PCF8563 + CR1220 | Timekeeping for timestamping even without WiFi |
| Audio | MAX98357A + 28mm speaker | Enrichment audio: calming tones, owner voice recordings, treat cue |
| Power | 5V USB-C + LiPo 2500mAh backup | Runs through power outage; bedside/kitchen-plugged normally |
| LEDs | WS2812 RGB + 4× SMD | System state: green=ok, amber=watch, red=high risk |
| Connectors | 2× I2C, 2× UART, 6× GPIO | Expansion (additional feeder, litter box sensor) |

**Hub firmware responsibilities:**
- Maintain BLE mesh network with collar tag + feeder node
- Aggregate per-minute vital data, activity classifications, and feeding events
- Run TFLite Micro wellness-score inference every 15 min (inputs: 24-hr HRV trend, activity distribution, gait asymmetry, vocalization count, feeding intake)
- Render pet vitals + activity timeline on TFT
- Trigger enrichment audio (calming tones, owner voice) when anxiety episode detected by camera
- Buffer 30 days of events locally (SD card) for cloud sync when WiFi available
- MQTT over WiFi to cloud; BLE to mobile app for instant alerts

### 2. Collar Tag Node (1 per pet)

The workhorse. Clips onto the pet's existing collar. Measures vitals and activity all day.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52840 | BLE mesh + sensor sampling + on-device activity CNN |
| PPG | MAX30101 (green+IR) | Heart rate + HRV (photoplethysmography on the neck fur gap) |
| IMU | LSM6DSO32 (32g accel + gyro) | Activity classification, gait analysis, lameness detection |
| Temperature | NTC thermistor (10kΩ, ±0.1°C) | Skin temperature (fever detection) |
| Flex PCB | 0.1mm PET flex substrate | Thin, lightweight, routes sensor traces along collar |
| Battery | LiPo 180mAh | 5–7 days per charge (duty-cycled sampling) |
| Charging | Qi RX receiver (5W) | Wireless charging — no collar removal needed |
| LEDs | 2× SMD (green/red) | Charging status + quick health indicator |
| Enclosure | PC/ABS 28×22×10mm | IP67 waterproof, 6g total |

**Collar firmware responsibilities:**
- Sample PPG at 100Hz for 20s every 60s → compute HR + HRV (RMSSD)
- Sample IMU at 50Hz continuously → on-device CNN classifies activity every 5s
- Detect scratching episodes (high-frequency vibration pattern), head-shaking (rotational gyro spike), and excessive licking (sustained low-amplitude motion)
- Compute gait features during walking: stride length, stance time, symmetry index, weight-bearing asymmetry
- Detect lameness: gait asymmetry index > threshold for >50 consecutive strides
- BLE mesh to hub every 60s; local 4-hour buffer if out of range (walks)
- Deep sleep between samples; wake on IMU activity threshold
- Qi charging status + battery reporting

### 3. Behavior Camera Node (1 per room, WiFi-plugged)

The eyes. Watches the pet when the owner is away. Runs on-device computer vision to classify behavior — no video leaves the house unless an alert is triggered.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 (N16R8) | Dual-core 240MHz, vector instructions for CV inference |
| Camera | OV5640 (5MP, autofocus) | Pet detection + behavior classification (running on ESP32-S3) |
| Mic Array | 6× SPH0645LM4H-B (I2S mems) | Sound localization + vocalization classification |
| AI Accelerator | ESP-PSRAM 8MB + model in flash | TFLite Micro behavior model + vocalization model |
| LED | IR LED 940nm (2×) | Night vision (invisible to pets) for 24/7 monitoring |
| Storage | MicroSD | Event clips (10s pre/post trigger) for owner review |
| Power | 5V USB-C (always plugged) | Continuous operation |
| Privacy | Physical lens shutter + switch | Owner can disable camera physically when home |

**Camera firmware responsibilities:**
- Run TFLite Micro pet-detection model at 5fps → bounding box + behavior class
- Classify behavior: resting, pacing, vocalizing, destructive chewing, inappropriate elimination, playing
- Run vocalization classifier on 6-mic array: pain bark, anxiety whine, alert bark, play bark, meow categories
- Detect separation anxiety episode: pacing + vocalizing + destruction sustained >5 min
- Trigger enrichment: send alert to hub → hub plays calming audio or dispenses treat
- Store 10s clips (pre/post trigger) on SD for owner review — only uploaded to cloud if owner enables sharing
- Privacy-first: all inference on-device; video never leaves unless explicitly shared
- MQTT over WiFi to cloud (events only — no raw video stream by default)

### 4. Smart Feeder Node (1+ per system)

The manager. Dispenses weight-verified food to the correct pet, tracks intake, and manages portion control.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-C6 | WiFi6 + BLE mesh + motor control + load cell reading |
| Load Cells | 4× 1kg strain gauge (HX711) | Food hopper weight (0.1g resolution) |
| Motor | Stepper motor (NEMA14) + auger | Precision dispensing (0.5g increments) |
| RFID | MFRC522 (13.56MHz) | Pet identification — dispense only to correct pet |
| Water | Water level sensor (capacitive) | Water bowl level monitoring + low-water alert |
| Hopper | 2L food container (BPA-free) | ~1 week of food for medium dog |
| Storage | W25Q128 16MB Flash | Feeding schedule cache + intake log |
| Power | 5V USB-C (always plugged) + LiPo 1000mAh backup | Continues through outage |
| Display | 0.96" OLED (SSD1306) | Next feeding time + portion + pet name |
| LEDs | WS2812 RGB | Feeding status: green=ok, blue=feeding, red=low food |

**Feeder firmware responsibilities:**
- Maintain feeding schedule (per-pet portions, times) — works offline if WiFi down
- RFID-verified dispensing: only dispense when the correct pet is detected (multi-pet homes)
- Track food intake: weight before/after feeding, flag appetite loss (>25% uneaten)
- Manage weight-loss plan: auto-adjust portions based on target weight + intake trends
- Water bowl monitoring: alert when low, track daily water consumption
- Low-food alert: notify app when hopper <15% capacity
- BLE mesh to hub + collar (for RFID proximity); WiFi to cloud for schedule sync
- Manual feed button + "treat" command from app or anxiety-enrichment trigger

---

## Communication Architecture

### Protocol Stack

| Layer | Technology | Purpose |
|-------|-----------|---------|
| Body mesh | BLE 5.3 mesh (nRF52840) | Collar ↔ Hub ↔ Feeder (low-power, 2.4GHz, ~30m range) |
| Cloud bridge | WiFi6 (ESP32-C6) | Hub ↔ Cloud (MQTT/TLS) |
| Camera link | WiFi6 (ESP32-S3) | Camera ↔ Cloud + Hub (events) |
| Mobile | BLE 5.3 (ESP32-C6) + WiFi | App ↔ Hub (instant alerts) |
| Outdoor | Local buffer + sync | Collar stores 4hr offline, syncs when back in range |

### Message Flow

```
Collar Tag ──BLE mesh──► Hub Node ──WiFi──► Cloud (MQTT)
   │  (HR, HRV, activity,      │                   │
   │   gait, scratching,       │  (aggregated      │
   │   temp, battery)           │   vitals +        │
   │                            │   wellness score)  │
Behavior Camera ──WiFi──► Hub + Cloud
   │  (behavior class, vocalization,
   │   anxiety episode flag, clip ref)
   │
Smart Feeder ──BLE mesh──► Hub Node
   │  (feeding event, intake, water level,
   │   RFID pet ID, low-food alert)
   │
Hub ──BLE──► Mobile App (instant alerts)
Hub ──WiFi──► Cloud (MQTT → FastAPI → TimescaleDB)
Cloud ──► Vet Portal (structured reports)
```

---

## Firmware

All firmware is in C, targeting the respective SDKs:

- **Hub Node** — RP2040 (Pico SDK) + ESP32-C6 (ESP-IDF) + nRF52840 (nRF5 SDK / SoftDevice mesh)
- **Collar Tag** — nRF52840 (nRF5 SDK, SoftDevice + BLE mesh)
- **Behavior Camera** — ESP32-S3 (ESP-IDF + TFLite Micro)
- **Smart Feeder** — ESP32-C6 (ESP-IDF)

### Shared Protocol

All nodes share `paw_protocol.h` — a compact binary protocol over the BLE mesh with CRC16-protected payloads for heart rate, activity, gait, vocalization, feeding, and alert messages.

See `firmware/common/paw_protocol.h` and `firmware/common/paw_protocol.c`.

### Directory Structure

```
firmware/
├── common/
│   ├── paw_protocol.h          # Shared message types + structs
│   ├── paw_protocol.c          # CRC16 + pack/verify + helpers
│   └── mesh_model.c            # BLE mesh vendor model wrapper
├── hub-node/
│   ├── CMakeLists.txt
│   ├── hub_main.c              # RP2040 main: mesh + wellness ML + TFT
│   ├── wellness_model.c        # TFLite Micro wellness inference
│   ├── vitals_render.c         # TFT display: pet vitals + timeline
│   └── enrichment.c            # Audio cue playback for anxiety
├── collar-tag/
│   ├── CMakeLists.txt
│   ├── collar_main.c           # nRF52840 main: sampling + mesh
│   ├── ppg_hr.c                # MAX30101 PPG → HR + HRV (RMSSD)
│   ├── activity_cnn.c          # On-device activity classifier
│   ├── gait_analysis.c         # Lameness + stride analysis
│   └── scratch_detect.c        # Scratching/head-shake detection
├── behavior-camera/
│   ├── CMakeLists.txt
│   ├── camera_main.c           # ESP32-S3 main: CV + mic + WiFi
│   ├── behavior_model.c        # TFLite Micro behavior classifier
│   ├── vocalization.c          # 6-mic vocalization classification
│   └── privacy.c               # Shutter + on-device-only enforcement
└── feeder-node/
    ├── CMakeLists.txt
    ├── feeder_main.c           # ESP32-C6 main: motor + load + RFID
    ├── load_cell.c              # HX711 weight measurement
    ├── rfid_pet.c              # MFRC522 pet identification
    └── schedule.c              # Feeding schedule + weight plan
```

---

## Software

### Cloud Dashboard (FastAPI + MQTT + TimescaleDB)

```
software/dashboard/
└── backend/
    ├── main.py                 # FastAPI app: ingest, wellness API, vet portal
    ├── requirements.txt        # fastapi, sqlalchemy, paho-mqtt, boto3, psycopg2
    ├── Dockerfile
    └── docker-compose.yml      # FastAPI + PostgreSQL/Timescale + Mosquitto + MinIO
```

**Key endpoints:**
- `POST /api/v1/ingest/vitals` — collar vitals (HR, HRV, temp, activity, gait)
- `POST /api/v1/ingest/behavior` — camera behavior events
- `POST /api/v1/ingest/feeding` — feeder events
- `GET /api/v1/pet/{pid}/wellness` — current + 7-day wellness score trend
- `GET /api/v1/pet/{pid}/activity` — activity timeline (rest/walk/play/scratch/etc.)
- `GET /api/v1/pet/{pid}/feeding` — feeding log + intake trends
- `GET /api/v1/pet/{pid}/anxiety` — separation anxiety episodes + severity
- `GET /api/v1/pet/{pid}/alerts` — alert history
- `POST /api/v1/vet/report/{pid}` — structured vet report (vitals + gait + behavior + feeding)
- `WS /api/v1/ws/alerts/{pet_id}` — real-time mobile alert push

### ML Pipeline

```
software/ml-pipeline/
├── train_wellness_score.py    # CNN-LSTM: HRV + activity + gait + feeding → illness risk
├── train_activity_classifier.py # 1D-CNN: IMU → 9 activity classes
├── train_vocalization.py       # Audio CNN: bark/meow → 6 vocalization classes
├── train_lameness.py           # Gait symmetry → lameness severity
├── personal_baseline.py        # Per-pet 14-day baseline personalization
└── requirements.txt
```

**Models:**
1. **Wellness Score (CNN-LSTM)** — fuses 24-hr HRV trend, activity distribution (9 classes), gait asymmetry, vocalization count, and feeding intake → daily wellness score (0–100) + 7-day illness-risk forecast. Exported as int8 TFLite (<80KB) for on-hub inference.
2. **Activity Classifier (1D-CNN)** — 50Hz IMU (accel + gyro) → 9 classes: walking, running, resting, sleeping, scratching, head-shaking, licking, eating, playing. Runs on-collar (nRF52840 TFLite Micro, <40KB).
3. **Vocalization Classifier (Audio CNN)** — 6-mic array spectrogram → 6 classes: pain, anxiety, alert, play, attention, distress. Runs on-camera (ESP32-S3 TFLite Micro, <50KB).
4. **Lameness Detector** — gait symmetry index (stride length, stance time, weight-bearing asymmetry) → lameness severity (0–4). Runs on-collar.

### Mobile App (React Native)

```
software/mobile-app/
├── App.tsx
├── api.ts                     # API client + WebSocket
├── package.json
├── components/
│   └── WellnessGauge.tsx      # Circular wellness score gauge
└── screens/
    ├── HomeScreen.tsx          # Wellness score + quick stats
    ├── ActivityScreen.tsx      # Activity timeline + trends
    ├── FeedingScreen.tsx       # Feeding log + schedule + manual feed
    ├── AnxietyScreen.tsx      # Anxiety episodes + enrichment history
    ├── VitalsScreen.tsx       # HR, HRV, temp trends
    ├── AlertsScreen.tsx        # Alert history + acknowledge
    └── VetScreen.tsx           # Vet report + share
```

---

## Bill of Materials

| Node | BOM | Unit Cost (qty 1) | Unit Cost (qty 10k) |
|------|-----|-------------------|---------------------|
| Hub Node | `hardware/bom/hub_node_bom.csv` | $38.50 | $21.40 |
| Collar Tag | `hardware/bom/collar_tag_bom.csv` | $24.80 | $14.20 |
| Behavior Camera | `hardware/bom/behavior_camera_bom.csv` | $22.40 | $12.80 |
| Smart Feeder | `hardware/bom/feeder_node_bom.csv` | $32.10 | $18.60 |
| **Total System** | | **$117.80** | **$67.00** |

---

## Power Architecture

### Collar Tag (battery-powered)
- **Battery:** 180mAh LiPo (3.7V) — 5–7 days per charge
- **Duty cycle:** PPG samples 20s/min (100Hz), IMU continuous at 50Hz, BLE TX 1/min
- **Sleep:** ~0.3mA deep sleep between PPG/IMU bursts; active draw ~8mA
- **Charging:** Qi 5W wireless — charges in ~45 min on the hub's charging pad
- **Low-battery:** Hub alerts at 15%; collar enters power-save mode (IMU-only, 10Hz) at 5%

### Hub Node (plugged + battery backup)
- USB-C 5V → TP4056 → LiPo 2500mAh → RT9013 3.3V LDO
- Qi transmitter (BQ500212A + coil) for collar charging
- Battery backup: ~6 hours of operation without mains

### Behavior Camera (always plugged)
- USB-C 5V, ~300mA active, ~20mA idle (IR-only night mode)

### Smart Feeder (plugged + battery backup)
- USB-C 5V → LiPo 1000mAh backup → motor draws ~400mA during dispensing

---

## Pin Assignments

### Hub Node — RP2040

| Pin | Function | Notes |
|-----|----------|-------|
| GP0  | UART0 TX | → nRF52840 RX (mesh) |
| GP1  | UART0 RX | ← nRF52840 TX |
| GP2  | I2S BCK  | → MAX98357A |
| GP3  | I2S WS   | → MAX98357A |
| GP4  | UART1 TX | → ESP32-C6 RX (WiFi) |
| GP5  | UART1 RX | ← ESP32-C6 TX |
| GP6  | I2S DATA | → MAX98357A DIN |
| GP7  | TFT DC   | ILI9488 data/command |
| GP8  | TFT CS   | SPI0 CS for TFT |
| GP9  | SPI0 SCK | shared SPI0 |
| GP10 | SPI0 MOSI| shared SPI0 |
| GP11 | SPI0 MISO| shared SPI0 (SD card) |
| GP12 | SD CS    | MicroSD card select |
| GP13 | Flash CS | W25Q256 external flash |
| GP14 | I2C0 SDA | PCF8563 RTC |
| GP15 | I2C0 SCL | PCF8563 RTC |
| GP16 | WS2812   | RGB status LED |
| GP17-20 | GPIO | 4× status LEDs |
| GP21 | Qi TX EN | enable Qi charging transmitter |
| GP22 | nRF BOOT | nRF52840 bootloader pin |
| GP26 | ADC0     | battery voltage divider |
| GP27 | ADC1     | hub temperature sensor |

### Collar Tag — nRF52840

| Pin | Function | Notes |
|-----|----------|-------|
| P0.02 | I2C SDA | MAX30101 PPG + thermistor ADC |
| P0.03 | I2C SCL | MAX30101 PPG |
| P0.04 | IMU SDA | LSM6DSO32 (separate I2C bus) |
| P0.05 | IMU SCL | LSM6DSO32 |
| P0.06 | UART RX | ← programming/UART |
| P0.08 | UART TX | → programming/UART |
| P0.09 | BLE antenna | internal |
| P0.10 | LED green | charging ok |
| P0.11 | LED red | low battery / alert |
| P0.12 | Qi RX EN | wireless charging enable |
| P0.13 | VBAT sense | battery voltage divider |
| P0.15 | PPG INT | MAX30101 interrupt |
| P0.20 | IMU INT1 | LSM6DSO32 activity interrupt (wake) |

### Behavior Camera — ESP32-S3

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO4  | I2S WS   | 6-mic array (SPH0645) |
| GPIO5  | I2S BCK  | 6-mic array |
| GPIO6  | I2S DATA | 6-mic array data in |
| GPIO7  | CAM D0   | OV5640 parallel bus |
| GPIO8-15 | CAM D1-D7 | OV5640 parallel bus |
| GPIO16 | CAM PCLK | OV5640 pixel clock |
| GPIO17 | CAM VSYNC| OV5640 vsync |
| GPIO18 | CAM HREF | OV5640 href |
| GPIO19 | CAM SIOC | OV5640 SCCB (I2C) |
| GPIO20 | CAM SIOD | OV5640 SCCB |
| GPIO21 | CAM XCLK | OV5640 master clock (20MHz) |
| GPIO38 | IR LED EN | 940nm IR illumination |
| GPIO39 | Shutter SW | physical privacy shutter switch |
| GPIO0  | Boot     | + SD card via SPI |
| GPIO1-3 | SD SPI   | MicroSD (MOSI/MISO/SCK/CS) |

### Smart Feeder — ESP32-C6

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO1  | HX711 SCK | load cell clock |
| GPIO2  | HX711 DOUT| load cell data |
| GPIO3  | Motor DIR | stepper direction |
| GPIO4  | Motor STEP| stepper step pulse |
| GPIO5  | Motor EN  | stepper enable |
| GPIO6  | RFID SCK  | MFRC522 SPI |
| GPIO7  | RFID MISO | MFRC522 |
| GPIO8  | RFID MOSI | MFRC522 |
| GPIO9  | RFID CS   | MFRC522 CS |
| GPIO10 | RFID RST  | MFRC522 reset |
| GPIO12 | OLED SDA  | SSD1306 I2C |
| GPIO13 | OLED SCL  | SSD1306 I2C |
| GPIO14 | Water ADC  | capacitive water level sensor |
| GPIO15 | WS2812    | status LED |
| GPIO16 | Button    | manual feed button |
| GPIO17 | Low-food SW| hopper low-food reed switch |

---

## Schematics

KiCad project files for each node:

- `schematic/hub-node/` — Hub (RP2040 + ESP32-C6 + nRF52840 + TFT + audio + Qi TX)
- `schematic/collar-tag/` — Collar tag (nRF52840 + MAX30101 + LSM6DSO32 + Qi RX)
- `schematic/behavior-camera/` — Camera (ESP32-S3 + OV5640 + 6-mic array + IR)
- `schematic/feeder-node/` — Feeder (ESP32-C6 + HX711 + stepper + MFRC522 + OLED)

See each folder's README for block diagrams, pin assignments, and power design.

---

## ML Pipeline Details

### Wellness Score Model (CNN-LSTM)

**Input:** 24-hour time-series at 5-min resolution (288 steps) × 24 features:
- Resting HR (1), HRV-RMSSD (1), skin temp (1)
- Activity distribution: % time in 9 activity classes (9)
- Gait: symmetry index, stride length, stance time (3)
- Vocalization count + type distribution (4)
- Feeding: intake grams, water ml, appetite ratio (3)
- Collar battery (1) + motion count (1)

**Architecture:**
- HR/Vital branch: Conv1D(1→32, k=5) → ReLU → MaxPool
- Activity branch: Conv1D(9→24, k=5) → ReLU → MaxPool
- Gait branch: Conv1D(3→16, k=5) → ReLU → MaxPool
- Concat → LSTM(128)×2 → Dense(64) → ReLU → Dense(1) → Sigmoid
- Output: wellness score [0,1] → scaled to 0–100

**Training data:** Vet-confirmed illness onset labels (pain, infection, GI, dental, urinary) from clinical study cohort. Synthetic placeholder data for development.

**Export:** int8 quantized TFLite (<80KB) for on-hub TFLite Micro inference every 15 min.

### Activity Classifier (1D-CNN on collar)

**Input:** 50Hz IMU (6-axis: accel XYZ + gyro XYZ) × 2s windows (100 samples)
**Classes:** walking, running, resting, sleeping, scratching, head-shaking, licking, eating, playing
**Architecture:** Conv1D(6→32, k=5) → ReLU → Conv1D(32→32, k=5) → ReLU → MaxPool → Conv1D(32→16, k=3) → Flatten → Dense(64) → Dense(9) → Softmax
**Export:** int8 TFLite (<40KB) for nRF52840 TFLite Micro

### Vocalization Classifier (Audio CNN on camera)

**Input:** 16kHz mono × 2s → Mel-spectrogram (64 mel bins × 128 frames)
**Classes:** pain, anxiety, alert, play, attention, distress
**Architecture:** Conv2D(1→32, 3×3) → ReLU → MaxPool → Conv2D(32→16, 3×3) → ReLU → Flatten → Dense(64) → Dense(6)
**Export:** int8 TFLite (<50KB) for ESP32-S3

---

## Clinical Validation

| Metric | Threshold | Source |
|--------|-----------|--------|
| HRV decline | >20% below 14-day baseline | Veterinary cardiology studies |
| Resting HR elevation | >15% above baseline | Pain/stress indicator |
| Skin temp elevation | >0.5°C above baseline | Fever/inflammation |
| Gait asymmetry index | >0.15 (unitless) | Lameness detection literature |
| Scratching frequency | >3× baseline episodes/day | Allergy/skin infection |
| Appetite loss | >25% uneaten food | Early illness sign |
| Anxiety episode | Pacing+vocalizing >5 min | Separation anxiety diagnostic criteria |
| Feeding intake drop | >30% below 7-day average | GI/illness indicator |

---

## Deployment

1. **Hub:** Place in kitchen/living room, plug in USB-C, connect to WiFi via app
2. **Collar tag:** Clip onto pet's collar, pair with hub via app, charge on hub pad
3. **Behavior camera:** Mount in main living area, plug in USB-C, connect to WiFi
4. **Smart feeder:** Place in kitchen, plug in, fill hopper, scan pet's RFID chip
5. **App:** Download, create pet profile (species, breed, age, weight, target weight), pair devices
6. **Baseline:** System learns pet's baseline over 14 days; wellness score activates after baseline
7. **Vet:** Share vet portal link for report access

---

## Privacy

- **Camera:** All behavior inference on-device (ESP32-S3). Video never leaves the house unless owner explicitly shares a clip. Physical lens shutter + switch.
- **Audio:** Vocalization classification on-device. No audio stream to cloud.
- **Vitals:** Encrypted in transit (TLS) and at rest (PostgreSQL encryption).
- **Data ownership:** Owner controls all data sharing. Vet access is explicitly granted and revocable.
- **No third-party data sales.**

---

## License

MIT — build it, sell it, improve it.
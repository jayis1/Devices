# CalmGrid

**AI-powered multi-node personal stress & mental wellness system.** Continuously tracks cardiac variability, electrodermal activity, activity, sleep quality, and voice prosody to detect stress, anxiety, and burnout days before they escalate — then closes the loop with adaptive lighting, guided breathing, ambient soundscapes, and therapist-ready reports. Prevents the #1 hidden epidemic of modern life: chronic stress that erodes health, sleep, relationships, and productivity silently until it becomes burnout, depression, or a cardiac event.

---

## What It Does

CalmGrid is a 4-node wearable + ambient system that turns a person's daily physiology and environment into a continuously monitored, stress-preventing, burnout-forecasting wellness pathway:

1. **Tracks cardiac stress biomarkers** — a lightweight wrist band (nRF52840 + PPG + EDA + thermistor + IMU) measures resting heart rate, HRV (RMSSD), electrodermal activity (skin conductance), skin temperature, and activity every minute. HRV decline and EDA arousal spikes are the earliest validated biomarkers of acute stress and allostatic load — often hours to days before a person feels "overwhelmed."
2. **Classifies activity & recovery** — the wrist band's 9-axis IMU runs an on-device CNN that classifies behavior in real time: sitting, walking, running, resting, sleeping, working (typing), and stressful-motion patterns. Movement is a key modulator of stress recovery; sedentary time + low HRV is a dangerous combination.
3. **Detects acute stress episodes** — a fusion of EDA arousal (tonic + phasic skin conductance), HRV suppression, and heart-rate elevation detects acute stress episodes in real time — a meeting that spiked your stress, a commute that triggered anxiety, a conflict. CalmGrid timestamps these so you see the *causes*, not just the symptoms.
4. **Analyzes voice prosody** — an ambient ESP32-S3 room sentinel with a 6-mic array runs on-device voice *prosody* analysis (pitch, jitter, shimmer, speech rate, energy) to detect stress in how you speak — **without transcribing or storing a single word**. Stressed speech has measurable acoustic signatures (raised pitch, monotone, faster rate) independent of language or content.
5. **Monitors the environment** — the room sentinel tracks ambient light (lux + CCT), temperature, humidity, and noise level — all of which modulate stress. Poor lighting, noise, and heat are invisible stressors that compound throughout the day.
6. **Actuates interventions** — a tunable-white light node (ESP32-C6 + dual-channel LED driver) adjusts lighting in real time: warm circadian dimming at night for sleep, cool focused light for work, and biophilic "de-stress" scenes when stress is detected. The hub plays guided breathing audio and ambient soundscapes via its speaker.
7. **Predicts burnout** — a multi-modal time-series model (CNN-LSTM) fuses 30-day HRV trends, EDA arousal patterns, sleep quality, activity, prosody stress, and environmental data to produce a daily Stress Score and a 14-day burnout-risk forecast — validated against clinical burnout scales (MBI, PSS-10).
8. **Guides recovery** — when stress is detected, the system can trigger: a breathing-guide animation on the hub display + paced audio, a lighting shift to a calming scene, a nature soundscape, or a gentle phone notification with a 60-second reset. It learns which interventions actually lower your stress over time.
9. **Alerts & therapist reports** — when burnout risk rises or acute stress episodes cluster, the app notifies you, and a structured report (HRV trends, EDA arousal map, stress-episode timeline, sleep quality, intervention efficacy) can be shared with your therapist or physician — so the session starts with data, not guesswork.
10. **Learns the individual** — every person has a different baseline HRV, EDA level, and stress response. CalmGrid personalizes baseline vitals and stress reactivity over the first 14 days, then watches for *deviations* — far more sensitive than population thresholds. It also learns your personal stress triggers (times, contexts) and which interventions work for *you*.

All body-worn nodes communicate over a BLE mesh (the wrist band works outdoors via local storage + sync). The hub bridges to WiFi/cellular for cloud analytics, ML training, therapist portal, and the mobile app. The room sentinel and light node are plugged-in units that communicate over WiFi/BLE.

### The Problem It Solves

- **Chronic stress is silent:** Stress accumulates below the level of conscious awareness. By the time someone feels "burned out," their HRV has been suppressed for weeks and their cortisol rhythm is disrupted. CalmGrid catches the trajectory early.
- **Burnout is epidemic:** >60% of workers report burnout symptoms; burnout costs the global economy ~$1 trillion/year in lost productivity. It's a leading cause of depression, cardiovascular disease, insomnia, and relationship breakdown. Current detection is retrospective questionnaires at annual checkups.
- **Stress triggers are invisible:** You don't realize a 9am meeting raises your stress every day, or that your commute spikes EDA arousal. CalmGrid timestamps every stress episode so you can change the *cause*, not just cope with the symptom.
- **Interventions are reactive and generic:** Apps tell you to "breathe" but don't know when you're actually stressed or whether it helped. CalmGrid triggers interventions *the moment* stress is detected and measures whether your HRV/EDA actually improved afterward.
- **Voice stress is underrecognized:** People mask stress in what they say but not in *how* they say it. Prosody analysis catches stress that self-report and wearable vitals miss — and does so without invading conversational privacy.
- **Environment compounds stress silently:** Bad lighting, noise, and heat raise stress continuously without you noticing. CalmGrid quantifies environmental load and actuates the light node to reduce it.
- **Therapy is data-blind:** Therapists rely on self-report (notoriously unreliable for stress and mood). CalmGrid gives them 30 days of objective physiological data so treatment targets the real patterns.

CalmGrid senses the person and environment continuously, predicts burnout weeks ahead, triggers personalized interventions the moment stress appears, and closes the loop with therapist reports — so people stay resilient and well.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                         CALMGRID SYSTEM                                           │
│                                                                                    │
│  ┌─────────────────────┐   BLE mesh   ┌──────────────────────┐                     │
│  │ WRIST BAND           │◄──────────►│                        │                     │
│  │ (on wrist)           │  2.4GHz     │                        │                     │
│  │ nRF52840 + PPG (HR)  │             │     HUB NODE           │──── WiFi6 ──►Cloud │
│  │ + EDA + IMU + temp   │             │    (RP2040 +           │             Dashboard│
│  │ + LiPo + Qi charge   │             │     ESP32-C6)          │             + ML     │
│  └─────────────────────┘             │                        │             Pipeline│
│                                       │  Edge ML: stress score  │             + Therapy │
│  ┌─────────────────────┐             │   (TFLite Micro)        │             Portal    │
│  │ ROOM SENTINEL        │── WiFi6 ──►│  TFT: stress gauge +    │─── BLE ───► Mobile   │
│  │ (plugged, room)      │            │   breathing guide       │             (React   │
│  │ ESP32-S3 + 6-mic     │            │  Speaker: breathing +   │              Native) │
│  │ + ambient light/temp │            │   soundscapes           │                     │
│  │ Edge: prosody stress │            └───────────┬────────────┘                     │
│  └─────────────────────┘                         │ BLE                              │
│                                       ┌──────────▼─────────────┐                     │
│                                       │  LIGHT NODE             │                     │
│                                       │  (room, plugged)        │                     │
│                                       │  ESP32-C6 + tunable     │                     │
│                                       │  white LED driver       │                     │
│                                       │  + ambient light FB     │                     │
│                                       │  circadian + de-stress  │                     │
│                                       └────────────────────────┘                     │
│                                                                                    │
│  ┌──────────────────────────────────────────────────────────────────────────────┐ │
│  │                    CLOUD / EDGE SOFTWARE                                       │ │
│  │  ┌──────────┐  ┌───────────────┐  ┌───────────────────────┐                 │ │
│  │  │Dashboard │  │ ML Pipeline   │  │ Mobile App            │                 │ │
│  │  │ (FastAPI)│  │ Stress CNN-   │  │ Stress score + trends  │                 │ │
│  │  │ Therapist│  │  LSTM         │  │ Episode timeline       │                 │ │
│  │  │ portal   │  │ Activity      │  │ Breathing guide        │                 │ │
│  │  │ Burnout  │  │  classifier   │  │ Intervention log       │                 │ │
│  │  │ forecast │  │ Prosody CNN   │  │ "Take a break" alert   │                 │ │
│  │  └──────────┘  └───────────────┘  └───────────────────────┘                 │ │
│  └──────────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per system)

The brain. Bridges the BLE mesh to WiFi/cloud. Runs the edge stress model (TFLite Micro), displays the stress gauge + breathing-guide animation, plays breathing/soundscape audio, and sends lighting commands to the light node.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | RP2040 + ESP32-C6 | RP2040 runs mesh + stress ML + display; ESP32-C6 handles WiFi6/BLE5.3 |
| Radio | nRF52840 (BLE mesh) + ESP32-C6 BLE | Dual-radio: nRF52840 for mesh with body nodes, ESP32-C6 for phone/cloud |
| Display | 3.5" IPS TFT (ILI9488) | Stress gauge, breathing-guide animation (expanding/contracting circle), episode timeline |
| Storage | W25Q256 32MB Flash + MicroSD | Model cache, 30-day ring buffer of vital/stress/environment events, OTA |
| RTC | PCF8563 + CR1220 | Timekeeping for timestamping even without WiFi |
| Audio | MAX98357A + 28mm speaker | Breathing paces (4-7-8, box), nature soundscapes, chime alerts |
| Power | 5V USB-C + LiPo 2500mAh backup | Runs through power outage; bedside/desk-plugged normally |
| LEDs | WS2812 RGB + 4× SMD | System state: green=calm, amber=elevated, red=high stress |
| Connectors | 2× I2C, 2× UART, 6× GPIO | Expansion (additional light nodes, scent diffuser) |

**Hub firmware responsibilities:**
- Maintain BLE mesh network with wrist band + light node
- Aggregate per-minute vital data, activity classifications, prosody stress, and ambient environment
- Run TFLite Micro stress-score inference every 15 min (inputs: 24-hr HRV trend, EDA arousal, activity, sleep quality, prosody stress, environmental load)
- Render stress gauge + breathing-guide animation on TFT
- Trigger interventions: breathing audio, soundscape, lighting command to light node, phone notification
- Buffer 30 days of events locally (SD card) for cloud sync when WiFi available
- MQTT over WiFi to cloud; BLE to mobile app for instant alerts

### 2. Wrist Band Node (1 per person)

The workhorse. Worn on the wrist. Measures stress physiology all day.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52840 | BLE mesh + sensor sampling + on-device activity CNN |
| PPG | MAX30101 (green+IR) | Heart rate + HRV (photoplethysmography on the wrist) |
| EDA | AD5940 + TIA + Ag/AgCl electrodes | Electrodermal activity (skin conductance) — tonic SCL + phasic SCR |
| IMU | LSM6DSO32 (32g accel + gyro) | Activity classification, motion artifact rejection |
| Temperature | TMP117 (±0.1°C digital) | Skin temperature (stress thermoregulation, sleep) |
| Flex PCB | 0.1mm PET flex substrate | Thin, lightweight, routes sensor traces along band |
| Battery | LiPo 220mAh | 4–5 days per charge (duty-cycled sampling) |
| Charging | Qi RX receiver (5W) | Wireless charging — no band removal needed |
| LEDs | 2× SMD (green/red) | Charging status + quick stress indicator |
| Enclosure | PC/ABS 38×22×10mm | IP67 waterproof, 8g total |

**Wrist band firmware responsibilities:**
- Sample PPG at 100Hz for 20s every 60s → compute HR + HRV (RMSSD)
- Sample EDA at 4Hz continuously → compute tonic SCL + phasic SCR (arousal spikes)
- Sample IMU at 50Hz continuously → on-device CNN classifies activity every 5s
- Sample skin temp every 60s
- Detect acute stress episode: EDA SCR spike + HRV suppression + HR elevation sustained >2 min
- BLE mesh to hub every 60s; local 6-hour buffer if out of range
- Deep sleep between samples; wake on IMU activity threshold
- Qi charging status + battery reporting

### 3. Room Sentinel Node (1 per room, WiFi-plugged)

The ears + eyes. Listens to voice prosody and monitors the environment. Runs on-device prosody stress analysis — **no speech content is ever transcribed, stored, or transmitted**.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 (N16R8) | Dual-core 240MHz, vector instructions for audio DSP |
| Mic Array | 6× SPH0645LM4H-B (I2S mems) | Voice capture for prosody analysis + sound localization |
| Ambient Light | VEML7700 (I2C) | Ambient lux — environmental stressor + circadian cue |
| Temp/Humidity | SHT40 (I2C) | Room temp + humidity — thermal comfort + stress |
| Noise | Derived from mic array | dB level — environmental stressor |
| AI Accelerator | ESP-PSRAM 8MB + model in flash | TFLite Micro prosody stress model |
| LED | IR LED 940nm (2×) | Night mode indicator |
| Storage | MicroSD | Prosody feature cache (features only, never audio) |
| Power | 5V USB-C (always plugged) | Continuous operation |
| Privacy | Physical mic mute switch + LED | Owner can disable mic physically |

**Room sentinel firmware responsibilities:**
- Run VAD (voice activity detection) on 6-mic array → detect speech segments
- Extract prosody features on-device: pitch (F0 mean + variability), jitter, shimmer, speech rate, energy, spectral tilt — **never transcribe words**
- Run TFLite Micro prosody stress classifier: calm / neutral / elevated / high-stress
- Sample VEML7700 ambient light every 60s; SHT40 temp/humidity every 60s
- Compute noise level (dB) from mic RMS every 60s
- Detect environmental stress events: sudden loud noise, light flicker, heat spike
- MQTT over WiFi to cloud (features + classifications only — no audio)
- Privacy-first: physical mic mute switch; all analysis on-device; no audio stream

### 4. Light Node (1+ per room)

The actuator. Tunable-white LED lighting that responds to circadian rhythm, stress level, and ambient feedback.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-C6 | WiFi6 + BLE mesh + LED PWM control |
| LED Driver | TLC5973 (16-channel PWM) | Drives tunable-white LED strip (warm 2700K + cool 6500K channels) |
| LEDs | Tunable-white strip (12V, 24W) | Circadian + de-stress lighting scenes |
| Ambient FB | TSL2591 (I2C) | Closed-loop lux feedback — maintains target brightness |
| Storage | W25Q128 16MB Flash | Scene cache + schedule |
| Power | 12V/24V PSU (always plugged) | Continuous operation |
| Display | 0.96" OLED (SSD1306) | Current scene + brightness |
| LEDs | WS2812 RGB | Status: green=calm scene, blue=circadian, amber=de-stress |

**Light node firmware responsibilities:**
- Maintain circadian schedule (warm dim at night, cool bright by day) — works offline
- Receive de-stress commands from hub: shift to biophilic scene (warm, low-CCT, gentle)
- Closed-loop brightness control via TSL2591 ambient feedback
- BLE mesh to hub; WiFi to cloud for schedule sync
- Manual override button + scene cycling
- Sunset/sunrise simulation for wake-up (gradual warm brightening)

---

## Communication Architecture

### Protocol Stack

| Layer | Technology | Purpose |
|-------|-----------|---------|
| Body mesh | BLE 5.3 mesh (nRF52840) | Wrist band ↔ Hub ↔ Light node (low-power, 2.4GHz, ~30m range) |
| Cloud bridge | WiFi6 (ESP32-C6) | Hub ↔ Cloud (MQTT/TLS) |
| Sentinel link | WiFi6 (ESP32-S3) | Room sentinel ↔ Cloud + Hub (features only) |
| Mobile | BLE 5.3 (ESP32-C6) + WiFi | App ↔ Hub (instant alerts) |
| Outdoor | Local buffer + sync | Wrist band stores 6hr offline, syncs when back in range |

### Message Flow

```
Wrist Band ──BLE mesh──► Hub Node ──WiFi──► Cloud (MQTT)
   │  (HR, HRV, EDA, temp,      │                   │
   │   activity, stress flag,    │  (aggregated      │
   │   battery)                   │   vitals +        │
│                                │   stress score)   │
Room Sentinel ──WiFi──► Hub + Cloud
   │  (prosody stress, ambient light,
   │   temp, humidity, noise level,
   │   env-stress flag)
   │
Light Node ──BLE mesh──► Hub Node
   │  (scene ack, brightness, ambient lux,
   │   override status)
   │
Hub ──BLE──► Mobile App (instant alerts + breathing cue)
Hub ──WiFi──► Cloud (MQTT → FastAPI → TimescaleDB)
Cloud ──► Therapist Portal (structured reports)
```

---

## Firmware

All firmware is in C, targeting the respective SDKs:

- **Hub Node** — RP2040 (Pico SDK) + ESP32-C6 (ESP-IDF) + nRF52840 (nRF5 SDK / SoftDevice mesh)
- **Wrist Band** — nRF52840 (nRF5 SDK, SoftDevice + BLE mesh)
- **Room Sentinel** — ESP32-S3 (ESP-IDF + TFLite Micro)
- **Light Node** — ESP32-C6 (ESP-IDF)

### Shared Protocol

All nodes share `calm_protocol.h` — a compact binary protocol over the BLE mesh with CRC16-protected payloads for vitals, prosody, environment, lighting, intervention, and alert messages.

See `firmware/common/calm_protocol.h` and `firmware/common/calm_protocol.c`.

### Directory Structure

```
firmware/
├── common/
│   ├── calm_protocol.h          # Shared message types + structs
│   ├── calm_protocol.c          # CRC16 + pack/verify + helpers
│   └── mesh_model.c            # BLE mesh vendor model wrapper
├── hub-node/
│   ├── CMakeLists.txt
│   ├── hub_main.c              # RP2040 main: mesh + stress ML + TFT
│   ├── stress_model.c          # TFLite Micro stress inference
│   ├── intervention.c          # Breathing audio + lighting + notification
│   └── dashboard_render.c      # TFT: stress gauge + breathing guide
├── wrist-band/
│   ├── CMakeLists.txt
│   ├── wrist_main.c            # nRF52840 main: sampling + mesh
│   ├── ppg_hrv.c               # MAX30101 PPG → HR + HRV (RMSSD)
│   ├── eda_gsr.c               # AD5940 EDA → SCL + SCR (arousal)
│   ├── activity_classify.c     # On-device activity classifier
│   └── stress_detect.c         # Acute stress episode detection
├── room-sentinel/
│   ├── CMakeLists.txt
│   ├── sentinel_main.c         # ESP32-S3 main: mic + ambient + WiFi
│   ├── prosody.c               # Voice prosody stress classification
│   ├── ambient.c               # Light/temp/humidity/noise monitoring
│   └── privacy.c               # Mic mute + on-device-only enforcement
└── light-node/
    ├── CMakeLists.txt
    ├── light_main.c            # ESP32-C6 main: LED driver + ambient FB
    └── circadian.c             # Circadian schedule + de-stress scenes
```

---

## Software

### Cloud Dashboard (FastAPI + MQTT + TimescaleDB)

```
software/dashboard/
└── backend/
    ├── main.py                 # FastAPI app: ingest, stress API, therapist portal
    ├── requirements.txt        # fastapi, sqlalchemy, paho-mqtt, boto3, psycopg2
    ├── Dockerfile
    └── docker-compose.yml      # FastAPI + PostgreSQL/Timescale + Mosquitto + MinIO
```

**Key endpoints:**
- `POST /api/v1/ingest/vitals` — wrist band vitals (HR, HRV, EDA, temp, activity)
- `POST /api/v1/ingest/prosody` — room sentinel prosody stress + ambient
- `POST /api/v1/ingest/environment` — ambient light/temp/humidity/noise
- `POST /api/v1/ingest/intervention` — intervention triggered + outcome
- `GET /api/v1/user/{uid}/stress` — current + 14-day stress score trend
- `GET /api/v1/user/{uid}/episodes` — acute stress episode timeline (timestamps + likely cause)
- `GET /api/v1/user/{uid}/sleep` — sleep quality (HRV-derived + motion)
- `GET /api/v1/user/{uid}/burnout` — burnout risk forecast (14-day)
- `GET /api/v1/user/{uid}/interventions` — intervention history + efficacy
- `GET /api/v1/user/{uid}/triggers` — learned personal stress triggers (time/context)
- `GET /api/v1/user/{uid}/alerts` — alert history
- `POST /api/v1/therapist/report/{uid}` — structured therapist report
- `WS /api/v1/ws/alerts/{uid}` — real-time mobile alert push

### ML Pipeline

```
software/ml-pipeline/
├── train_stress_model.py       # CNN-LSTM: HRV + EDA + activity + prosody + sleep → stress + burnout
├── train_activity_classifier.py # 1D-CNN: IMU → 8 activity classes
├── train_prosody.py            # Audio CNN: prosody features → 4 stress classes
├── train_burnout.py            # 30-day trend → burnout risk (MBI-validated)
├── personal_baseline.py        # Per-user 14-day baseline personalization
└── requirements.txt
```

**Models:**
1. **Stress Score (CNN-LSTM)** — fuses 24-hr HRV trend, EDA arousal (SCL + SCR rate), activity distribution (8 classes), prosody stress level, sleep quality, and environmental load → daily stress score (0–100) + 14-day burnout-risk forecast. Exported as int8 TFLite (<80KB) for on-hub inference.
2. **Activity Classifier (1D-CNN)** — 50Hz IMU (accel + gyro) → 8 classes: sitting, walking, running, resting, sleeping, working, commuting, exercising. Runs on-wrist (nRF52840 TFLite Micro, <40KB).
3. **Prosody Stress Classifier (Audio CNN)** — prosody feature vector (F0, jitter, shimmer, rate, energy, spectral tilt) → 4 classes: calm, neutral, elevated, high-stress. Runs on-sentinel (ESP32-S3 TFLite Micro, <50KB). **No speech content processed.**
4. **Burnout Predictor (CNN-LSTM)** — 30-day multi-modal trends → burnout risk (0–100) validated against Maslach Burnout Inventory (MBI) and Perceived Stress Scale (PSS-10).

### Mobile App (React Native)

```
software/mobile-app/
├── App.tsx
├── api.ts                     # API client + WebSocket
├── package.json
├── components/
│   └── StressGauge.tsx        # Circular stress score gauge
└── screens/
    ├── HomeScreen.tsx          # Stress score + quick stats
    ├── StressScreen.tsx        # Stress timeline + episode map
    ├── InterventionsScreen.tsx # Intervention history + efficacy
    ├── VitalsScreen.tsx        # HR, HRV, EDA, temp trends
    ├── BurnoutScreen.tsx       # Burnout risk forecast + contributing factors
    ├── TherapistScreen.tsx     # Therapist report + share
    └── AlertsScreen.tsx        # Alert history + acknowledge
```

---

## Bill of Materials

| Node | BOM | Unit Cost (qty 1) | Unit Cost (qty 10k) |
|------|-----|-------------------|---------------------|
| Hub Node | `hardware/bom/hub_node_bom.csv` | $39.20 | $21.80 |
| Wrist Band | `hardware/bom/wrist_band_bom.csv` | $28.60 | $16.10 |
| Room Sentinel | `hardware/bom/room_sentinel_bom.csv` | $23.50 | $13.40 |
| Light Node | `hardware/bom/light_node_bom.csv` | $26.80 | $15.20 |
| **Total System** | | **$118.10** | **$66.50** |

---

## Power Architecture

### Wrist Band (battery-powered)
- **Battery:** 220mAh LiPo (3.7V) — 4–5 days per charge
- **Duty cycle:** PPG samples 20s/min (100Hz), EDA continuous at 4Hz, IMU continuous at 50Hz, BLE TX 1/min
- **Sleep:** ~0.3mA deep sleep between PPG/IMU bursts; active draw ~9mA (EDA + PPG + IMU)
- **Charging:** Qi 5W wireless — charges in ~50 min on the hub's charging pad
- **Low-battery:** Hub alerts at 15%; band enters power-save mode (IMU-only, 10Hz) at 5%

### Hub Node (plugged + battery backup)
- USB-C 5V → TP4056 → LiPo 2500mAh → RT9013 3.3V LDO
- Qi transmitter (BQ500212A + coil) for wrist band charging
- Battery backup: ~6 hours of operation without mains

### Room Sentinel (always plugged)
- USB-C 5V, ~280mA active (mic + DSP), ~20mA idle

### Light Node (always plugged)
- 12V/24V PSU → 5V buck for ESP32-C6; LED strip draws up to 2A at 24W

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

### Wrist Band — nRF52840

| Pin | Function | Notes |
|-----|----------|-------|
| P0.02 | I2C SDA | MAX30101 PPG + TMP117 |
| P0.03 | I2C SCL | MAX30101 PPG + TMP117 |
| P0.04 | IMU SDA | LSM6DSO32 (separate I2C bus) |
| P0.05 | IMU SCL | LSM6DSO32 |
| P0.06 | SPI CS  | AD5940 EDA (SPI) |
| P0.07 | SPI SCK | AD5940 EDA |
| P0.08 | SPI MOSI| AD5940 EDA |
| P0.09 | SPI MISO| AD5940 EDA |
| P0.10 | LED green | charging ok |
| P0.11 | LED red | low battery / stress alert |
| P0.12 | Qi RX EN | wireless charging enable |
| P0.13 | VBAT sense | battery voltage divider |
| P0.15 | PPG INT | MAX30101 interrupt |
| P0.20 | IMU INT1 | LSM6DSO32 activity interrupt (wake) |
| P0.28 | EDA excitation | H-tied excitation electrode drive |
| P0.29 | EDA sense+ | TIA positive input (Ag/AgCl) |
| P0.30 | EDA sense− | TIA negative input (Ag/AgCl) |

### Room Sentinel — ESP32-S3

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO4  | I2S WS   | 6-mic array (SPH0645) |
| GPIO5  | I2S BCK  | 6-mic array |
| GPIO6  | I2S DATA | 6-mic array data in |
| GPIO8  | I2C SDA  | VEML7700 + SHT40 |
| GPIO9  | I2C SCL  | VEML7700 + SHT40 |
| GPIO38 | IR LED EN | 940nm IR illumination |
| GPIO39 | Mic mute SW | physical mic mute switch |
| GPIO40 | Active LED | indicator when mic processing |
| GPIO0  | Boot     | + SD card via SPI |
| GPIO1-3 | SD SPI   | MicroSD (MOSI/MISO/SCK/CS) |

### Light Node — ESP32-C6

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO1  | LED PWM WARM | TLC5973 warm channel (2700K) |
| GPIO2  | LED PWM COOL | TLC5973 cool channel (6500K) |
| GPIO3  | TLC5973 SCK | LED driver clock |
| GPIO4  | TLC5973 DATA | LED driver data |
| GPIO5  | TLC5973 LAT | LED driver latch |
| GPIO6  | TSL2591 SDA | ambient light feedback (I2C) |
| GPIO7  | TSL2591 SCL | ambient light feedback (I2C) |
| GPIO12 | OLED SDA  | SSD1306 I2C |
| GPIO13 | OLED SCL  | SSD1306 I2C |
| GPIO14 | WS2812    | status LED |
| GPIO15 | Button    | manual override / scene cycle |
| GPIO16 | Override SW| wall-switch override detect |

---

## Schematics

KiCad project files for each node:

- `schematic/hub-node/` — Hub (RP2040 + ESP32-C6 + nRF52840 + TFT + audio + Qi TX)
- `schematic/wrist-band/` — Wrist band (nRF52840 + MAX30101 + AD5940 + LSM6DSO32 + TMP117 + Qi RX)
- `schematic/room-sentinel/` — Sentinel (ESP32-S3 + 6-mic array + VEML7700 + SHT40 + IR)
- `schematic/light-node/` — Light (ESP32-C6 + TLC5973 + tunable-white strip + TSL2591 + OLED)

See each folder's README for block diagrams, pin assignments, and power design.

---

## ML Pipeline Details

### Stress Score Model (CNN-LSTM)

**Input:** 24-hour time-series at 5-min resolution (288 steps) × 26 features:
- Resting HR (1), HRV-RMSSD (1), skin temp (1)
- EDA: tonic SCL (1), phasic SCR rate (1), SCR amplitude mean (1)
- Activity distribution: % time in 8 activity classes (8)
- Prosody stress level (1) + speech minutes (1)
- Sleep: duration (1), efficiency (1), HRV-during-sleep (1)
- Environment: ambient lux (1), CCT (1), temp (1), humidity (1), noise dB (1)
- Wrist battery (1) + step count (1)

**Architecture:**
- HR/Vital branch: Conv1D(3→32, k=5) → ReLU → MaxPool
- EDA branch: Conv1D(3→24, k=5) → ReLU → MaxPool
- Activity branch: Conv1D(8→24, k=5) → ReLU → MaxPool
- Concat → LSTM(128)×2 → Dense(64) → ReLU → Dense(2) → Sigmoid
- Output: stress score [0,1] → scaled to 0–100; burnout risk [0,1] → 0–100

**Training data:** Clinical study cohort labeled with MBI, PSS-10, and cortisol assays. Synthetic placeholder data for development.

**Export:** int8 quantized TFLite (<80KB) for on-hub TFLite Micro inference every 15 min.

### Activity Classifier (1D-CNN on wrist band)

**Input:** 50Hz IMU (6-axis: accel XYZ + gyro XYZ) × 2s windows (100 samples)
**Classes:** sitting, walking, running, resting, sleeping, working, commuting, exercising
**Architecture:** Conv1D(6→32, k=5) → ReLU → Conv1D(32→32, k=5) → ReLU → MaxPool → Conv1D(32→16, k=3) → Flatten → Dense(64) → Dense(8) → Softmax
**Export:** int8 TFLite (<40KB) for nRF52840 TFLite Micro

### Prosody Stress Classifier (Audio CNN on sentinel)

**Input:** 16kHz mono × 2s speech segment → prosody feature vector (F0 mean, F0 variability, jitter, shimmer, speech rate, energy mean, energy variability, spectral tilt, HNR) = 9 features × 8 frames
**Classes:** calm, neutral, elevated, high-stress
**Architecture:** Conv1D(9→32, k=3) → ReLU → Conv1D(32→16, k=3) → ReLU → Flatten → Dense(64) → Dense(4) → Softmax
**Export:** int8 TFLite (<50KB) for ESP32-S3
**Privacy:** Only acoustic features are extracted on-device. No words are transcribed, stored, or transmitted.

### Burnout Predictor (CNN-LSTM)

**Input:** 30-day daily aggregates (30 steps × 18 features): avg stress score, HRV trend, EDA arousal, sleep duration/efficiency, activity balance, prosody stress distribution, environmental load, intervention count + efficacy
**Output:** burnout risk (0–100) + contributing factors (SHAP-style attribution)
**Validation:** Maslach Burnout Inventory (MBI) — emotional exhaustion subscale

---

## Clinical Validation

| Metric | Threshold | Source |
|--------|-----------|--------|
| HRV decline | >20% below 14-day baseline | Stress / allostatic load literature |
| Resting HR elevation | >10% above baseline | Acute stress indicator |
| EDA SCR rate increase | >2× baseline | Sympathetic arousal (stress) |
| Skin temp drop (distal) | >0.4°C below baseline | Stress thermoregulation (fight-or-flight vasoconstriction) |
| Prosody F0 elevation | >15% above personal baseline | Voice stress research |
| Sleep HRV suppression | >15% below 7-day average | Sleep quality + recovery |
| Acute stress episode | EDA SCR + HRV suppression >2 min | Autonomic co-activation |
| Burnout risk | 30-day stress trend + MBI correlation | Maslach Burnout Inventory |
| Sedentary + low HRV | <3000 steps + HRV <baseline | Combined cardiovascular risk |

---

## Deployment

1. **Hub:** Place on desk/bedside, plug in USB-C, connect to WiFi via app
2. **Wrist band:** Wear on non-dominant wrist, pair with hub via app, charge on hub pad
3. **Room sentinel:** Mount in main living/work area, plug in USB-C, connect to WiFi
4. **Light node:** Install LED strip + controller in room, plug in PSU, pair with hub
5. **App:** Download, create profile (age, sex, work pattern, sleep target), pair devices
6. **Baseline:** System learns your baseline over 14 days; stress score activates after baseline
7. **Therapist:** Share therapist portal link for report access (optional)

---

## Privacy

- **Voice:** All prosody analysis on-device (ESP32-S3). **No speech is transcribed, stored, or transmitted** — only acoustic feature vectors and a stress classification leave the device. Physical mic mute switch.
- **Vitals:** Encrypted in transit (TLS) and at rest (PostgreSQL encryption).
- **Environment:** Aggregate environmental data only — no identifying information.
- **Data ownership:** Owner controls all data sharing. Therapist access is explicitly granted and revocable.
- **No third-party data sales.**

---

## License

MIT — build it, sell it, improve it.
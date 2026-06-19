# SoleGuard

**AI-powered multi-node diabetic foot ulcer prevention, gait-health, and mobility early-warning system.** Detects plantar pressure hotspots, skin-temperature asymmetry, gait degradation, and incipient wounds days-to-weeks before a diabetic foot ulcer forms — then alerts the patient, caregiver, and clinician so intervention happens before tissue breaks down. Prevents up to 75% of diabetic foot ulcers and the amputations that follow.

---

## What It Does

SoleGuard is a 4-node wearable + ambient system that turns the daily life of a person with diabetes into a continuously monitored, ulcer-preventing care pathway:

1. **Maps plantar pressure** — two instrumented insoles (one per shoe) with 24-point pressure matrices capture peak pressure, pressure-time integrals, and shear-proxy gradients every step, all day. Sustained hotspot pressure is the #1 mechanical cause of ulceration in neuropathic feet.
2. **Tracks skin temperature asymmetry** — 8 thermistors per insole measure plantar skin temperature bilaterally. A temperature difference of >2.2°C between corresponding foot zones is the earliest validated biomarker of impending ulceration (inflammation preceds breakdown by 1–4 weeks). SoleGuard alarms on asymmetric warming, not absolute temperature.
3. **Analyzes gait** — IMUs in each insole + an ankle tag compute cadence, stride length, double-support time, gait symmetry index, and shuffling detection. Gait degradation is an early marker of neuropathy progression, balance loss, and fall risk.
4. **Scans for wounds** — a bathroom foot-scanner (ESP32-S3 + camera + multispectral LED ring) photographs the plantar surface and between the toes daily; on-device + cloud ML detects cuts, blisters, callus breakdown, and fungal changes that a neuropathic patient cannot feel.
5. **Predicts ulcer risk** — a multi-modal time-series model (CNN-LSTM) fuses pressure hotspots, temperature asymmetry trends, gait decline, and scan-image changes to produce a per-foot ulcer-risk score 1–3 weeks ahead, validated against the standard thermometry + pressure clinical thresholds.
6. **Alerts & offloads** — when risk rises, the app tells the patient to offload (rest, change footwear, use the prescribed offloading boot), notifies the caregiver, and sends a structured report to the clinician's portal with the hotspot map, temperature trend, and wound image. No waiting for the next quarterly clinic visit.
7. **Learns the individual** — every foot is different. SoleGuard personalizes baseline pressure distribution, temperature symmetry, and gait pattern over the first 14 days, then watches for *deviations* from that personal baseline — far more sensitive than population thresholds.
8. **Tracks edema** — the ankle wearable measures bioimpedance to detect ankle swelling (a sign of fluid retention, infection, or Charcot foot), another early warning the patient can't feel.

All body-worn nodes communicate over a low-power BLE mesh (no phone needed in pocket for real-time alerts — the hub can be a bedside unit). The hub bridges to WiFi/cellular for cloud analytics, ML training, clinician portal, and the mobile app. The foot scanner is a bathroom-plugged unit that uploads images over WiFi.

### The Problem It Solves

- **Diabetic foot ulcers are catastrophic and common:** ~15–25% of people with diabetes develop a foot ulcer in their lifetime. Ulcers are the leading cause of diabetes-related hospitalization and account for >60% of non-traumatic lower-limb amputations.
- **Neuropathy removes the warning:** Diabetic peripheral neuropathy destroys protective sensation — patients literally cannot feel the pressure hotspot, the blister, or the early wound. By the time they see it, tissue is often already infected or necrotic.
- **Late detection = amputation:** The 5-year mortality after a diabetes-related amputation is ~50% — worse than many cancers. Early detection of pre-ulcerative signs (hotspot temperature, pressure, callus) can prevent up to 75% of ulcers.
- **Quarterly clinic visits miss the window:** A foot check every 3 months cannot catch a 1-week temperature rise or a developing blister. SoleGuard monitors continuously and triggers offloading the *day* risk appears.
- **Offloading adherence is poor:** Even when prescribed a pressure-relief boot, patients don't wear it enough. SoleGuard's insole pressure map verifies in real time whether the offloading is actually working.
- **Falls from gait decline:** Diabetic neuropathy causes balance loss and falls. SoleGuard's gait symmetry + double-support metrics flag decline early, enabling physical therapy referral before a fall.
- **Edema & Charcot foot:** Sudden foot swelling can signal Charcot neuroarthropathy (a destructive bone/joint condition) or infection. SoleGuard's bioimpedance tag detects swelling early.

SoleGuard senses the foot continuously, predicts ulceration weeks ahead, and closes the loop with offloading alerts and clinician reports — so neuropathic feet stay intact, and amputations are prevented.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                         SOLEGUARD SYSTEM                                           │
│                                                                                    │
│  ┌─────────────────────┐   BLE mesh   ┌──────────────────────┐                     │
│  │ LEFT INSOLE          │◄──────────►│                        │                     │
│  │ (in shoe)            │  2.4GHz     │                        │                     │
│  │ nRF52840 + 24×FSR    │             │     HUB NODE           │──── WiFi6 ──►Cloud │
│  │ + 8×thermistor       │             │    (RP2040 +           │             Dashboard│
│  │ + IMU + LiPo         │             │     ESP32-C6)          │             + ML     │
│  └─────────────────────┘             │                        │             Pipeline│
│                                       │  Edge ML: ulcer-risk   │             + Clinician│
│  ┌─────────────────────┐              │   score (TFLite Micro) │             Portal   │
│  │ RIGHT INSOLE         │◄──────────►│   TFT: foot heat map   │─── BLE ───► Mobile   │
│  │ (in shoe)            │  BLE mesh   │   Siren: hotspot alarm │             (React   │
│  │ nRF52840 + 24×FSR    │             │                        │              Native) │
│  │ + 8×thermistor       │             │                        │                     │
│  │ + IMU + LiPo         │             └───────────┬────────────┘                     │
│  └─────────────────────┘                         │ BLE                              │
│                                       ┌──────────▼─────────────┐                     │
│  ┌─────────────────────┐             │  ANKLE WEARABLE TAG     │                     │
│  │ FOOT SCANNER         │── WiFi6 ──►│  (on ankle/sock)        │                     │
│  │ (bathroom, plugged)  │            │  nRF52840 + IMU +       │                     │
│  │ ESP32-S3 + OV5640    │            │   bioimpedance + skin   │                     │
│  │ + multispectral LED  │            │   temp + LiPo           │                     │
│  │ Edge ML: wound detect│            │  5–7 days per charge    │                     │
│  └─────────────────────┘            └────────────────────────┘                     │
│                                                                                    │
│  ┌──────────────────────────────────────────────────────────────────────────────┐ │
│  │                    CLOUD / EDGE SOFTWARE                                       │ │
│  │  ┌──────────┐  ┌───────────────┐  ┌───────────────────────┐                 │ │
│  │  │Dashboard │  │ ML Pipeline   │  │ Mobile App            │                 │ │
│  │  │ (FastAPI)│  │ Ulcer risk    │  │ Foot heat map         │                 │ │
│  │  │ Clinician│  │  CNN-LSTM     │  │ Temperature asymmetry │                 │ │
│  │  │ portal   │  │ Wound detect  │  │ Gait score            │                 │ │
│  │  │ Patient  │  │ Gait decline  │  │ "Offload now" alert   │                 │ │
│  │  │ history  │  │ Edema detect  │  │ Caregiver share       │                 │ │
│  │  │ Trends   │  │ Personal base │  │ Clinician report      │                 │ │
│  │  └──────────┘  └───────────────┘  └───────────────────────┘                 │ │
│  └──────────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per system)

The brain. Bridges the BLE mesh to WiFi/cloud. Runs the edge ulcer-risk model (TFLite Micro), displays the foot heat map, and sounds an audible hotspot alarm for patients who may not check their phone.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | RP2040 + ESP32-C6 | RP2040 runs mesh + ulcer-risk ML + display; ESP32-C6 handles WiFi6/BLE5.3 |
| Radio | nRF52840 (BLE mesh) + ESP32-C6 BLE | Dual-radio: nRF52840 for mesh with body nodes, ESP32-C6 for phone/cloud |
| Display | 3.5" IPS TFT (ILI9488) | Foot heat map: pressure + temperature per zone, risk score, gait |
| Storage | W25Q256 32MB Flash + MicroSD | Model cache, 30-day ring buffer of pressure/temp/gait events, OTA |
| RTC | PCF8563 + CR1220 | Timekeeping for timestamping even without WiFi |
| Audio | MAX98357A + 28mm speaker | Audible "pressure hotspot, please offload" voice prompt + alarm beep |
| Power | 5V USB-C + LiPo 2500mAh backup | Runs through power outage; bedside-plugged normally |
| LEDs | WS2812 RGB + 4× SMD | System state: green=ok, amber=watch, red=high risk |
| Connectors | 2× I2C, 2× UART, 6× GPIO | Expansion (weight scale, blood-glucose bridge) |

**Hub firmware responsibilities:**
- Maintain BLE mesh network with insoles, ankle tag
- Aggregate per-step pressure maps, temperature arrays, gait features every 30s
- Run TFLite Micro ulcer-risk inference every 5 min (inputs: 24-hr pressure-time integral per zone, 24-hr temperature asymmetry trend, gait symmetry, edema)
- Render foot heat map on TFT (left/right, 6 zones each)
- Trigger voice + LED alarm when risk score > threshold or temperature asymmetry >2.2°C for >2 consecutive readings
- Buffer 30 days of events locally (SD card) for cloud sync when WiFi available
- MQTT over WiFi to cloud; BLE to mobile app for instant alerts

### 2. Smart Insole Node (2 per system — left & right)

The workhorse. Lives inside the shoe, under the foot, all day. Maps pressure and temperature across the plantar surface.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52840 | BLE mesh + sensor sampling + local feature extraction |
| Pressure | 24× FSR (Interlink 402 / 400) | Plantar pressure matrix: 6 zones × 4 sensors (heel, midfoot, metatarsal heads 1–5, hallux, lesser toes) |
| Temperature | 8× NTC thermistor (10kΩ, ±0.1°C) | Plantar skin temp at 8 corresponding bilateral zones for asymmetry |
| IMU | LSM6DSO32 (32g accel + gyro) | Gait: cadence, stride, double-support, shuffling, heel-strike/toe-off |
| Flex PCB | 0.1mm PET flex substrate | Thin enough for insole, routes FSR + thermistor traces |
| Power | LiPo 350mAh (1.5mm thin-pouch) | 14–18 hours per charge; charges in 90 min on the dock |
| Charging | Qi wireless receiver (5W) | Drop insoles on the hub's charging pad overnight |
| Antenna | PCB trace antenna (2.4GHz) | BLE mesh |
| Enclosure | None — embedded in orthotic insole | Sensors face plantar skin; MCU + battery in arch cavity |

**Insole firmware responsibilities:**
- Sample 24 FSRs at 100 Hz (burst during stance phase), downsample to 10 Hz sustained
- Sample 8 thermistors at 0.1 Hz (skin temp changes slowly)
- Sample IMU at 100 Hz for gait phase detection (heel-strike, mid-stance, toe-off)
- Compute per-step: peak pressure per zone, pressure-time integral, contact area, center-of-pressure trajectory
- Compute per-5-min: mean temp per zone, temp asymmetry vs contralateral (sent to hub for the actual bilateral diff)
- Transmit compressed pressure map + temp + gait features to hub every 30s over BLE mesh
- Low-power: sleep between stance bursts, ~3mA average, 14–18h battery

### 3. Ankle Wearable Tag (1 per system)

Worn on the ankle (clip or sock-band). Measures whole-leg gait, edema, and skin temp; acts as a BLE mesh relay to extend range when the hub is far.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52840 | BLE mesh + sensor sampling + relay |
| IMU | LSM6DSO32 | Ankle-level gait: stride symmetry, foot-clearance, shuffling, fall detection |
| Bioimpedance | AD5940 + 4 electrodes | Ankle edema: segmental bioimpedance tracks fluid accumulation (Charcot/infection warning) |
| Skin temp | TMP117 (±0.1°C) | Ankle skin temperature for swelling/inflammation context |
| Power | LiPo 180mAh | 5–7 days per charge (low duty cycle) |
| Charging | Qi wireless 5W | Drop on hub charging pad |
| Antenna | PCB trace antenna | BLE mesh + relay |
| Enclosure | 32mm disc, 6mm thick, IP67 | Silicone band clip |

**Ankle tag firmware responsibilities:**
- Sample IMU at 100 Hz for gait symmetry + foot-clearance + fall detection (free-fall + impact)
- Run bioimpedance measurement every 4h (AD5940 EIS sweep, 1kHz–100kHz, 4-electrode)
- Sample skin temp at 0.05 Hz
- Relay BLE mesh packets between insoles and hub when insoles are out of phone/hub range (mesh hop)
- Transmit gait + edema + temp features to hub every 60s
- Crash/fall detection → immediate alert to hub → cloud → caregiver

### 4. Foot Scanner Node (1 per system)

A bathroom-counter unit. The patient places each foot on the scan surface daily; multispectral imaging detects wounds, callus breakdown, and fungal changes that neuropathy hides.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 (16MB PSRAM) | WiFi + image capture + on-device wound-detection ML |
| Camera | OV5640 (5MP, autofocus) | Plantar + interdigital imaging at 2592×1944 |
| Illumination | 8× LED ring: white + 850nm IR + 405nm UV-A | White for color/texture; IR for sub-surface inflammation (heat correlates); UV-A for fungal fluorescence (tinea fluoresces) |
| Display | 2.4" TFT (ILI9341) | "Place left foot… scanning… done" + thumbnail + risk flag |
| Touch | Capacitive touch button | Start scan, confirm foot side |
| Scale | HX711 + 4× load cells (under scan platform) | Body weight tracking (sudden weight change = fluid retention) |
| Edge ML | TFLite wound-detection model (MobileNetV3) | Detects: ulcer, blister, callus breakdown, fissure, fungal lesion, normal |
| Power | USB-C 5V (plugged, always on) | Bathroom-counter appliance |
| Comms | WiFi6 (ESP32-C6 co-proc optional) | Image upload to cloud + hub coordination |
| Enclosure | 300×220×40mm scan pad, IP54 | Footrest surface with camera underneath a transparent plate + LED ring |

**Foot scanner firmware responsibilities:**
- On touch: prompt "Left or Right foot?" → capture 3 images (white, IR, UV-A) at 5MP
- Run on-device wound-detection TFLite model → classify region as normal/callus/blister/ulcer/fissure/fungal
- If wound or change-from-baseline detected: flag image, send to cloud for clinician review + alert app
- Upload all scans to cloud daily (for trend ML: wound area growth, callus thickening)
- Weigh patient on scan platform; track weight trend (edema/ fluid retention)
- Coordinate with hub over WiFi: pull current risk score to show on scanner TFT ("Foot risk: LOW")

---

## Communication Architecture

### BLE Mesh (body-worn network)

- **Stack:** Bluetooth Mesh (SIG Mesh) on nRF52840, 2.4GHz, mesh relay enabled on ankle tag
- **Topology:** Insole-L ↔ Ankle Tag ↔ Insole-R ↔ Hub. Ankle tag is a relay node so insoles can reach the hub even when the patient is in the garden and the hub is bedside.
- **Payload:** Compressed pressure map (24× 8-bit = 24B) + temp (8× 16-bit = 16B) + gait features (8× 16-bit = 16B) + flags (2B) = ~58B per 30s = ~1.9kB/min ≈ 0.11MB/hr. BLE mesh handles this comfortably.
- **Latency:** Alerts (hotspot / fall) use mesh flooding for <1s delivery to hub.
- **Power:** Insoles ~3mA avg (14–18h), ankle tag ~0.5mA avg (5–7d), hub mains-powered.

### WiFi (hub ↔ cloud, scanner ↔ cloud)

- **Hub:** ESP32-C6 WiFi6 → MQTT (TLS) to cloud dashboard. Heartbeat every 60s, event-driven for alerts.
- **Scanner:** ESP32-S3 WiFi → HTTPS upload of scan images + wound flags. Also WiFi to hub for risk-score sync.

### Cloud

- **MQTT broker** (Mosquitto) ingests pressure, temp, gait, edema, scan events
- **FastAPI backend** (dashboard + clinician portal + mobile API)
- **PostgreSQL** for patient history, TimescaleDB for time-series pressure/temp/gait
- **ML pipeline** (PyTorch): ulcer-risk CNN-LSTM training, wound-detection training, gait-decline model, personal baseline learning
- **Object storage** (MinIO/S3) for scan images

---

## Firmware

All firmware is in C (nRF Connect SDK / Zephyr RTOS for nRF52840 nodes; Pico SDK for RP2040; ESP-IDF for ESP32-S3). Shared BLE-mesh protocol code lives in `firmware/common/`.

### Firmware layout

```
firmware/
├── common/
│   ├── sole_protocol.h        # Shared message types, mesh model IDs, payload structs
│   ├── sole_protocol.c        # Encode/decode helpers, CRC
│   └── mesh_model.c           # SIG Mesh vendor model registration (pressure/temp/gait)
├── hub-node/
│   ├── hub_main.c             # RP2040: mesh agg + TFLite ulcer-risk + TFT + audio
│   ├── ulcerrisk_model.c      # TFLite Micro inference wrapper
│   ├── heatmap_render.c       # TFT foot heat-map renderer
│   └── CMakeLists.txt
├── insole-node/
│   ├── insole_main.c          # nRF52840: FSR + thermistor + IMU sampling + mesh TX
│   ├── pressure_matrix.c      # 24-FSR scanning, peak/PTI/CoP computation
│   ├── thermistor_array.c     # 8-NTC oversampling + temp calc
│   ├── gait_phase.c           # IMU heel-strike/toe-off state machine
│   └── CMakeLists.txt
├── ankle-tag/
│   ├── ankle_main.c           # nRF52840: IMU + bioimpedance + relay + fall detect
│   ├── bioimpedance.c         # AD5940 EIS sweep + edema index
│   └── CMakeLists.txt
└── foot-scanner/
    ├── scanner_main.c         # ESP32-S3: camera + LED ring + TFLite wound detect
    ├── wound_model.c          # TFLite wound classifier wrapper
    ├── weight_scale.c         # HX711 load-cell sampling
    └── CMakeLists.txt
```

### Shared protocol (excerpt)

```c
/* sole_protocol.h — SoleGuard mesh payload definitions */
typedef enum {
  SOLE_MSG_PRESSURE_TEMP = 0x10,   /* insole → hub: 24×FSR + 8×temp */
  SOLE_MSG_GAIT           = 0x11,   /* insole/ankle → hub: gait features */
  SOLE_MSG_EDEMA          = 0x12,   /* ankle → hub: bioimpedance */
  SOLE_MSG_ALERT          = 0x20,   /* any → hub: hotspot/fall/wound flag */
  SOLE_MSG_RISK_SCORE     = 0x30,   /* hub → mesh: broadcast risk score */
  SOLE_MSG_SCAN_RESULT    = 0x40,   /* scanner → hub/cloud: wound flag */
} sole_msg_type_t;

typedef struct __attribute__((packed)) {
  uint8_t  type;
  uint8_t  node_id;       /* 0=L-insole, 1=R-insole, 2=ankle, 3=scanner */
  uint8_t  seq;
  uint8_t  flags;
  uint8_t  pressure[24];   /* 8-bit, 0-255 → 0-500kPa scaled */
  int16_t  temp_centic[8]; /* centi-degC, ±0.01°C */
  int16_t  gait[8];        /* cadence, stride, symmetry, etc. (fixed-point) */
  uint16_t crc16;
} sole_payload_t;
```

---

## Cloud / Edge Software

### Dashboard (FastAPI + MQTT)

```
software/dashboard/backend/
├── main.py                 # FastAPI app: patient + clinician endpoints, MQTT subscriber
├── models.py               # SQLAlchemy: Patient, FootEvent, Scan, RiskScore, Alert
├── mqtt_subscriber.py      # Paho MQTT → ingest pressure/temp/gait/edema/scan events
├── clinician_portal.py     # /clinician/* routes: patient list, risk trends, wound images
├── alerts.py               # Rule + ML alert engine → push to app + caregiver SMS
├── docker-compose.yml      # FastAPI + Postgres + TimescaleDB + Mosquitto + MinIO
├── requirements.txt
└── Dockerfile
```

**Key endpoints:**
- `POST /api/v1/ingest/pressure` — MQTT-payload REST equivalent for testing
- `GET /api/v1/patient/{id}/risk` — current + 7-day ulcer-risk trend
- `GET /api/v1/patient/{id}/heatmap` — latest pressure + temp heat map
- `GET /api/v1/patient/{id}/gait` — gait symmetry + fall-risk trend
- `GET /api/v1/patient/{id}/scans` — wound image history + classifications
- `POST /api/v1/clinician/report` — generate structured wound + risk report PDF
- `WS /api/v1/ws/alerts/{patient_id}` — real-time alert stream to app

### ML Pipeline

```
software/ml-pipeline/
├── train_ulcer_risk.py      # CNN-LSTM: pressure + temp-asymmetry + gait → ulcer risk (0–1)
├── train_wound_detect.py    # MobileNetV3: foot-scan images → wound class
├── train_gait_decline.py    # GRU: 30-day gait features → fall-risk / neuropathy-progression score
├── personal_baseline.py     # Per-patient 14-day baseline learning (pressure + temp + gait)
├── edema_model.py           # Bioimpedance → edema index calibration
├── requirements.txt
└── README.md
```

**Ulcer-risk model (CNN-LSTM):**
- Input: 24-hr window of per-zone pressure-time integral (6 zones), bilateral temperature asymmetry (8 zones), gait symmetry index, edema index → 24×6 pressure tensor + 24×8 temp tensor + 24×4 gait tensor
- Architecture: 2× Conv1D (pressure + temp branches) → concat → 2× LSTM(128) → Dense → sigmoid (risk 0–1)
- Label: clinician-confirmed ulcer onset within next 21 days (binary) → risk score
- Output: per-foot risk score 0–1, threshold 0.65 → "offload now" alert

**Wound-detection model (MobileNetV3-Small):**
- Input: 224×224 foot-scan image (white-light channel primary; IR + UV as 2 extra channels → 3ch)
- Classes: normal, callus, blister, fissure, ulcer, fungal, maceration
- Trained on annotated diabetic foot images (partner clinic data; synthetic augmentation for rare classes)
- On-device (ESP32-S3 TFLite) for instant flag; cloud for high-res clinician review

### Mobile App (React Native)

```
software/mobile-app/
├── App.tsx                 # Tab navigator: Home, Heat Map, Gait, Scans, Alerts, Caregiver
├── screens/
│   ├── HomeScreen.tsx      # Current risk score, "offload" prompt, today's steps + gait
│   ├── HeatMapScreen.tsx   # Left/right foot pressure + temp heat map (canvas)
│   ├── GaitScreen.tsx      # Cadence, stride, symmetry, fall-risk trend charts
│   ├── ScansScreen.tsx     # Latest foot scan images + wound flags + history
│   ├── AlertsScreen.tsx    # Alert history + "I've offloaded" confirmation
│   └── CaregiverScreen.tsx # Add caregiver, share risk + alerts, clinician report send
├── components/
│   ├── FootHeatMap.tsx     # SVG pressure + temperature map renderer
│   ├── RiskGauge.tsx       # Circular risk gauge 0–100
│   └── TrendChart.tsx      # D3-based trend line
├── api.ts                  # REST + WebSocket client
└── package.json
```

---

## Bill of Materials (BOMs)

Full BOMs in `hardware/bom/`:

| Node | Key components | Est. cost (Qty 1) | Est. cost (Qty 10k) |
|------|----------------|-------------------|---------------------|
| Hub Node | RP2040, ESP32-C6, nRF52840, ILI9488 TFT, MAX98357A, W25Q256, PCF8563, LiPo 2500mAh, USB-C | $38.50 | $21.40 |
| Smart Insole (each) | nRF52840, 24× FSR-402, 8× NTC 10k, LSM6DSO32, Qi RX, LiPo 350mAh, flex PCB | $27.80 | $14.20 |
| Ankle Tag | nRF52840, LSM6DSO32, AD5940, TMP117, Qi RX, LiPo 180mAh | $24.10 | $12.60 |
| Foot Scanner | ESP32-S3, OV5640, 8× LED ring (white+IR+UV), ILI9341, HX711, 4× load cells, USB-C | $33.20 | $17.80 |
| **Full system (hub + 2 insoles + ankle + scanner)** | | **$151.40** | **$80.20** |

---

## Power Architecture

| Node | Source | Battery | Runtime | Charging |
|------|--------|---------|---------|----------|
| Hub Node | USB-C 5V mains | LiPo 2500mAh backup | Mains + 12h backup | USB-C |
| Insole ×2 | LiPo 350mAh | Internal | 14–18h | Qi pad on hub (overnight) |
| Ankle Tag | LiPo 180mAh | Internal | 5–7 days | Qi pad on hub |
| Foot Scanner | USB-C 5V mains | None | Mains | USB-C |

The hub's top surface is a Qi wireless charging pad: the patient drops both insoles + the ankle tag on it overnight. Single-plug system for the whole body-worn fleet.

---

## ML & Clinical Validation Notes

- **Temperature asymmetry threshold:** Validated clinical standard — bilateral plantar temperature difference >2.2°C sustained → high ulcer risk (Armstrong et al., *Diabetes Care*). SoleGuard uses this as a hard alert independent of the ML model.
- **Pressure-time integral (PTI):** PTI > 700 kPa·s/cycle at a metatarsal head is a validated ulcer-predictive threshold (Veves/Boulton). SoleGuard computes PTI per zone per step and tracks the 24-hr peak.
- **Offloading verification:** When a patient is prescribed a total-contact cast or offloading boot, SoleGuard's insole confirms pressure dropped below threshold — addressing the known non-adherence problem.
- **Wound image ML:** Trained on partner-clinic annotated images; cloud-inference second pass reduces false positives; all flagged images route to clinician portal for human review.

---

## Daily Life with SoleGuard

1. **Morning:** Patient puts on shoes with the charged insoles. Clips the ankle tag on. The hub (bedside) shows "Good morning — foot risk: LOW" on its TFT.
2. **Through the day:** Insoles map pressure every step; thermistors track skin temp; IMU logs gait. BLE mesh relays data via the ankle tag to the hub. If a hotspot forms (new shoes, long walk), the hub's TFT turns amber and the app pings: "Right metatarsal head 2 pressure elevated 3h — consider rest or change shoes."
3. **Bathroom visit:** Patient steps on the foot scanner; it photographs both feet (white + IR + UV), runs wound detection, weighs them, and shows "No wounds detected ✓" or "Possible blister, right heel — image sent to Dr. Patel."
4. **Evening:** Patient drops insoles + ankle tag on the hub's Qi pad. Hub syncs the day's data to the cloud; the ML pipeline updates the 21-day ulcer-risk forecast.
5. **Clinician (weekly):** Dr. Patel opens the portal — sees the patient's pressure heat map trend, temperature asymmetry trend, gait symmetry, wound images, and the risk forecast. No ulcer → adjust footwear. Risk rising → call patient in for offloading.

---

## Regulatory & Safety

- **Class II medical device** (FDA 510(k)) — ulcer-prediction software + patient-facing alert
- **Data:** HIPAA-compliant cloud (encrypted at rest + in transit); patient consents to caregiver/clinician sharing
- **Safety interlocks:** Temperature asymmetry alert fires independently of the ML model (rule-based clinical threshold) so an ML failure cannot suppress a hard clinical warning. All ML alerts are advisory; the system never automates treatment.
- **Fail-safe:** If BLE mesh / WiFi is down, insoles buffer 48h locally and the hub's last-risk-score stays on TFT; scanner works fully offline and queues images.

---

## Project Structure

```
sole-guard/
├── README.md                    # This file
├── schematic/
│   ├── hub-node/README.md       # Schematic + pin assignments
│   ├── insole-node/README.md
│   ├── ankle-tag/README.md
│   └── foot-scanner/README.md
├── firmware/
│   ├── common/                  # Shared BLE-mesh protocol
│   ├── hub-node/                # RP2040 hub firmware
│   ├── insole-node/             # nRF52840 insole firmware
│   ├── ankle-tag/               # nRF52840 ankle tag firmware
│   └── foot-scanner/            # ESP32-S3 scanner firmware
├── hardware/bom/                # Per-node BOM CSVs
├── software/
│   ├── dashboard/backend/       # FastAPI + MQTT
│   ├── ml-pipeline/             # PyTorch training scripts
│   └── mobile-app/              # React Native
├── docs/
│   ├── architecture.md
│   ├── api.md
│   └── protocol.md
└── scripts/
    ├── deploy.py
    └── calibrate_insole.py
```

---

## License

MIT — build it, sell it, save feet with it.

---

*SoleGuard: because neuropathy shouldn't end in amputation.*
# AsthmaSync — AI-Powered Asthma Management & Trigger Avoidance System

> **339 million people** worldwide have asthma. **250,000 die** every year. Most exacerbations are preventable — if patients knew their personal triggers, tracked medication adherence, and caught lung-function decline early. AsthmaSync is a multi-node hardware+software system that does all of that, automatically.

AsthmaSync continuously monitors four signals that matter for asthma control:

1. **Wheeze detection** — a wearable band listens for wheezing (the hallmark expiratory sound of bronchoconstriction) using a body-coupled microphone + on-device CNN.
2. **Rescue inhaler actuation tracking** — a tiny tag clips onto any metered-dose inhaler (MDI) and detects actuation events via accelerometer shake-signature classification, logging every rescue dose.
3. **Environmental trigger sensing** — an air sentinel measures PM2.5, VOCs, formaldehyde, CO₂, temperature, and humidity, the six indoor triggers most strongly linked to exacerbations.
4. **Physiological correlation** — the wearable band also captures PPG heart rate / HRV (stress + exertion), SpO₂ (oxygen saturation during events), and skin temperature.

A hub fuses these streams, runs edge ML inference (tflite-micro), and forwards features to a cloud backend that trains:
- A **7-day exacerbation risk forecaster** (LSTM on rescue-use frequency + wheeze count + HRV + PM2.5 exposure)
- A **personal trigger identifier** (XGBoost SHAP attribution per trigger variable)
- A **wheeze classifier** (1D-CNN, 22-class: wheeze, stridor, cough, normal, talking, etc.)
- An **inhaler actuation classifier** (Random Forest on accel signatures: actuation vs. pocket-shake vs. drop)
- A **lung-function decline trend** detector (Bayesian change-point on peak-flow proxy from wheeze pitch)

The mobile app shows real-time status, a personal trigger heatmap, 7-day risk forecast, medication adherence score, and pushes a clinical action-plan alert when risk crosses thresholds.

---

## System Architecture

```
                         ┌──────────────────────────────────────────────────┐
                         │                  CLOUD (AWS / GCP)                │
                         │  FastAPI + PostgreSQL + TimescaleDB + MQTT broker │
                         │  ┌──────────┐  ┌───────────┐  ┌──────────────┐  │
                         │  │  7-day   │  │  Trigger   │  │  Wheeze CNN  │  │
                         │  │  Exacerb.│  │  Identifier│  │  (22-class)  │  │
                         │  │  LSTM    │  │  (XGBoost) │  │  retrain     │  │
                         │  └──────────┘  └───────────┘  └──────────────┘  │
                         └────────────▲─────────────────────────────────────┘
                                      │ MQTT (TLS)
                         ┌────────────┴─────────────────────────────────────┐
                         │              HUB (ESP32-S3)                       │
                         │  Sub-GHz 868 MHz TDMA mesh  •  BLE 5.0           │
                         │  WiFi  •  tflite-micro edge inference            │
                         │  Local 14-day rolling cache (PSRAM)               │
                         └───┬──────────────┬──────────────┬────────────────┘
                             │ Sub-GHz       │ BLE 5.0      │ BLE 5.0
                   ┌─────────┴────┐  ┌───────┴────────┐  ┌──┴──────────────┐
                   │ AIR SENTINEL │  │  INHALER TAG  │  │   WHEEZE BAND   │
                   │   ESP32-S3   │  │   nRF52840    │  │    nRF52840     │
                   │              │  │               │  │                 │
                   │ PMSA003I PM  │  │ LSM6DSO accel │  │ SPH0645 mic     │
                   │ BME688 VOC   │  │ + gyro         │  │ MAX30101 PPG    │
                   │ SGP40 HCHO   │  │ LED + buzzer   │  │   (HR/HRV/SpO2) │
                   │ SCD41 CO2    │  │ CR2032 / LiPo  │  │ TMP117 skin-T   │
                   │ Sub-GHz      │  │ BLE 5.0       │  │ LSM6DSO IMU     │
                   └──────────────┘  └───────────────┘  │ LiPo + USB-C     │
                                                          └─────────────────┘
```

### Communication Stack

| Link | Protocol | Band | Range | Why |
|------|----------|------|-------|-----|
| Air Sentinel → Hub | Sub-GHz 868 MHz TDMA mesh | EU 863-870 / US 902-928 | 300 m LoS, 60 m indoor | Penetrates walls; low-power; star-of-stars with mesh fallback |
| Inhaler Tag → Hub | BLE 5.0 (GATT notify) | 2.4 GHz | 15 m | Tiny battery; always-on body; phone can also act as relay |
| Wheeze Band → Hub | BLE 5.0 (GATT notify) | 2.4 GHz | 15 m | Continuous high-rate audio + PPG needs BLE bandwidth |
| Hub → Cloud | WiFi (WPA2-PSK) → MQTT/TLS | 2.4 GHz | LAN | Standard home network; hub also works offline for 14 days |

---

## Hardware Nodes

### Node 1 — Hub (Coordinator)

**SoC**: ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB PSRAM) — needed for tflite-micro models + 14-day rolling cache.

**Radios**: ESP32-S3 WiFi/BLE + SX1262 Sub-GHz transceiver (868/915 MHz).

**Peripherals**:
- 2.4" TFT (ILI9341, SPI) — optional status display
- 3 buttons (acknowledge alert, silence, pair)
- RGB LED (status)
- buzzer (audible alarm)
- microSD slot (overflow cache)
- USB-C (power + programming)

**Power**: 5 V USB-C mains (always-on device). RTC BBK + CR2032 backup for timekeeping.

**Pin Map**:

| GPIO | Function | Peripheral |
|------|----------|------------|
| 4 | SPI CLK | ILI9341 + SD card |
| 5 | SPI MOSI | ILI9341 + SD card |
| 6 | SPI MISO | ILI9341 + SD card |
| 7 | SPI CS (TFT) | ILI9341 |
| 10 | SPI CS (SD) | microSD |
| 11 | D/C (TFT) | ILI9341 |
| 14 | RESET (TFT) | ILI9341 |
| 15 | SX1262 CS | SX1262 |
| 16 | SX1262 DIO1 (IRQ) | SX1262 |
| 17 | SX1262 BUSY | SX1262 |
| 18 | SX1262 RESET | SX1262 |
| 8 | SX1262 MOSI | SX1262 (HSPI) |
| 48 | SX1262 MISO | SX1262 (HSPI) |
| 3 | SX1262 SCK | SX1262 (HSPI) |
| 1 | Button: ACK | GPIO input |
| 2 | Button: Silence | GPIO input |
| 41 | Button: Pair | GPIO input |
| 38 | RGB LED (WS2812) | data |
| 42 | Buzzer | PWM |
| 46 | BOOT (flash) | strapping |

### Node 2 — Air Sentinel

**SoC**: ESP32-S3-WROOM-1-N8R2 (8 MB flash, 2 MB PSRAM).

**Radio**: SX1262 Sub-GHz (mesh node, 868 MHz).

**Sensors**:
- **PMSA003I** (Plantower) — PM1.0/2.5/10 (μg/m³), I²C interface (0x12)
- **BME688** (Bosch) — VOC (IAQ), CO₂-equivalent, temperature, humidity, pressure; I²C (0x77)
- **SGP40** (Sensirion) — VOC index (formaldehyde + other aldehydes); I²C (0x59)
- **SCD41** (Sensirion) — NDIR CO₂ (400-5000 ppm, ±40 ppm); I²C (0x62)

**Power**: USB-C 5 V mains (always-on). Optional 18650 Li-ion backup (TP4056 charger).

**Pin Map**:

| GPIO | Function | Peripheral |
|------|----------|------------|
| 8 | I²C SCL | PMSA003I, BME688, SGP40, SCD41 (shared bus) |
| 9 | I²C SDA | shared |
| 4 | PMSA003I SET (standby ctrl) | PMSA003I |
| 5 | PMSA003I RST | PMSA003I |
| 10 | SX1262 CS | SX1262 (VSPI) |
| 11 | SX1262 DIO1 | SX1262 |
| 12 | SX1262 BUSY | SX1262 |
| 13 | SX1262 RESET | SX1262 |
| 14 | SX1262 SCK | SX1262 (VSPI) |
| 15 | SX1262 MISO | SX1262 |
| 16 | SX1262 MOSI | SX1262 |
| 17 | Battery ADC | voltage divider |
| 18 | Status LED | WS2812 |

### Node 3 — Inhaler Tag

**SoC**: nRF52840 QFAA (1 MB flash, 256 KB RAM) — ultra-low-power BLE.

**Sensors**: LSM6DSO (ST) 6-axis IMU (accel + gyro), I²C (0x6A).

**Indicators**: single LED (blue) + piezo buzzer.

**Power**: CR2032 (3 V, 220 mAh) — primary cell, 6-month life. Option: 110 mAh LiPo with nRF on-board DCDC.

**Form factor**: silicone sleeve that clips onto standard MDI canister (diameter 25 mm). PCB is a 18 mm disc.

**Pin Map**:

| nRF pin | Function | Peripheral |
|---------|----------|------------|
| P0.04 | I²C SDA | LSM6DSO |
| P0.05 | I²C SCL | LSM6DSO |
| P0.06 | LSM6DSO INT1 | accel interrupt |
| P0.07 | LSM6DSO INT2 | activity-free fall |
| P0.08 | Button (long-press = dose confirm) | tactile switch |
| P0.11 | LED (blue) | indicator |
| P0.13 | Buzzer | PWM |
| P0.18 | NFC antenna A | NFC pair (optional tap-to-log) |
| P0.19 | NFC antenna B | |
| P0.15 | Battery ADC | voltage divider (for LiPo) |

### Node 4 — Wheeze Band

**SoC**: nRF52840 QFAA.

**Sensors**:
- **SPH0645LM4H-B** (Knowles) — I²S MEMS microphone (high SNR, 65 dB) for wheeze detection
- **MAX30101** (Maxim) — PPG (green/red/IR LEDs) → HR, HRV, SpO₂; I²C (0x57)
- **TMP117** (TI) — digital skin temperature (±0.1°C); I²C (0x48)
- **LSM6DSO** — 6-axis IMU for activity context (running vs resting); I²C (0x6A)

**Power**: 200 mAh LiPo (1.5-day battery, USB-C charging via TP4056). MAX30101 is duty-cycled (100 Hz sample, 25% duty) to extend battery.

**Form factor**: wristband (like a watch), silicone housing.

**Pin Map**:

| nRF pin | Function | Peripheral |
|---------|----------|------------|
| P0.08 | I²S SCK | SPH0645 mic |
| P0.09 | I²S WS (LRCLK) | SPH0645 mic |
| P0.10 | I²S SD (data in) | SPH0645 mic |
| P0.11 | I²S MCK | SPH0645 mic (master clock) |
| P0.26 | I²C SDA | MAX30101, TMP117, LSM6DSO |
| P0.27 | I²C SCL | shared |
| P0.06 | MAX30101 INT1 | PPG interrupt |
| P0.07 | LSM6DSO INT1 | accel interrupt |
| P0.15 | Button (mark event) | tactile switch |
| P0.16 | Vibrator (LR motor) | PWM (haptic alert) |
| P0.13 | LED (green) | status |
| P0.04 | Battery ADC | LiPo voltage divider |
| VDD-nRF | TP4056 BAT | charge IC |

---

## Firmware

All firmware is written in C using the nRF Connect SDK (nRF52840 nodes) and ESP-IDF v5.1 (ESP32-S3 nodes). Shared protocol code lives in `firmware/common/`.

### Key firmware modules

| Node | File | Purpose |
|------|------|---------|
| Hub | `firmware/hub/main.c` | FreeRTOS task scheduler: mesh coordinator, BLE central, MQTT client, edge ML |
| Hub | `firmware/hub/edge_ml.c` | tflite-micro inference: 22-class wheeze CNN + actuation classifier |
| Hub | `firmware/hub/mqtt.c` | TLS MQTT client with offline queue (PSRAM ring buffer) |
| Air Sentinel | `firmware/air-sentinel/sensors.c` | I²C driver for PMSA003I, BME688, SGP40, SCD41 |
| Air Sentinel | `firmware/air-sentinel/main.c` | Sub-GHz TDMA slot manager, sensor polling |
| Inhaler Tag | `firmware/inhaler-tag/actuation.c` | LSM6DSO FIFO reader + accel signature feature extraction |
| Inhaler Tag | `firmware/inhaler-tag/main.c` | BLE GATT service, sleep management, dose counter |
| Wheeze Band | `firmware/wheeze-band/wheeze.c` | I²S mic capture + on-device 1D-CNN wheeze pre-classifier |
| Wheeze Band | `firmware/wheeze-band/vitals.c` | MAX30101 PPG processing (HR/HRV/SpO₂) |
| Common | `firmware/common/protocol.h` | Packet structure, message types, CRC |
| Common | `firmware/common/radio.c` | SX1262 driver wrapper (Semtech radio driver) |

### Edge ML Models (tflite-micro on Hub)

| Model | Input | Output | Size | Latency |
|-------|-------|--------|------|---------|
| Wheeze CNN | 2 s mel-spectrogram (40 × 32) | 22-class softmax | 180 KB | 35 ms |
| Actuation classifier | 3-axis accel (512 samples) | 4-class (actuation/shake/drop/static) | 24 KB | 8 ms |

---

## Cloud & ML Pipeline

### Backend (FastAPI)

- **MQTT subscriber** ingests all node telemetry into TimescaleDB hypertables
- **REST API** serves mobile app: `/risk`, `/triggers`, `/adherence`, `/events`, `/trends`
- **Celery worker** runs nightly model retraining and 7-day forecast generation
- Dockerized, deployable via `docker-compose.yml`

### ML Models

| # | Model | Framework | Purpose |
|---|-------|-----------|---------|
| 1 | Exacerbation LSTM | PyTorch | 7-day risk forecast (0-100%) from 30-day multivariate time series |
| 2 | Trigger Identifier | XGBoost + SHAP | Per-trigger variable attribution of exacerbation risk |
| 3 | Wheeze CNN | PyTorch (1D-CNN) | 22-class respiratory sound classification (retrained on-device) |
| 4 | Actuation Classifier | scikit-learn RF | Inhaler actuation vs. pocket-shake vs. drop vs. static |
| 5 | Lung-function trend | Bayesian change-point | Detects FEV₁-proxy decline (from wheeze pitch + rescue-use trend) |

### Training data

- **Wheeze**: U.K. lung sound database (RALE) + private annotated corpus → 50,000 clips
- **Actuation**: collected via scripted inhaler use across 5 inhaler brands (see `scripts/collect_actuation.py`)
- **Exacerbation**: derived from Asthma Control Test (ACT) scores + rescue-use logs

---

## Mobile App (React Native)

- **Dashboard**: real-time risk gauge, current air quality, last wheeze event, last rescue dose
- **Trigger Heatmap**: 7-day × 24-hour grid showing trigger variable correlation with symptoms
- **Medication**: adherence calendar, rescue/controller dose log, refill reminders
- **Action Plan**: zone-based (Green/Yellow/Red) following GINA guidelines, auto-populated
- **Reports**: PDF export for pulmonologist visits (30-day summary)

---

## Bill of Materials (Summary)

| Node | Est. Cost (qty 1) | Est. Cost (qty 1k) |
|------|-------------------|-------------------|
| Hub | $28.40 | $14.20 |
| Air Sentinel | $34.60 | $16.80 |
| Inhaler Tag | $9.80 | $4.50 |
| Wheeze Band | $22.10 | $11.30 |
| **System total** | **$94.90** | **$46.80** |

See `hardware/bom/*.csv` for full BOMs with manufacturer part numbers.

---

## Clinical Relevance

AsthmaSync follows **GINA (Global Initiative for Asthma) 2023** guidelines:

- **Rescue inhaler use > 2×/week** → Yellow Zone (partly controlled) — AsthmaSync auto-detects this
- **Nighttime wheeze** → risk marker — Wheeze Band catches this while patient is asleep
- **PM2.5 > 35 μg/m³** exposure → trigger — Air Sentinel logs cumulative exposure
- **SpO₂ < 92%** during wheeze event → Red Zone — triggers immediate alert
- **HRV reduction** (rmSSD) → stress/exertion correlation — used in LSTM features

The 7-day forecast gives patients and clinicians a window to **step up controller medication** before an exacerbation lands them in the ER.

---

## Regulatory

- **FDA Class II** (510(k)) pathway as a Clinical Decision Support Software (CDSS) — non-diagnostic, alert-only
- **CE Class IIa** under MDR as a medical device accessory
- **HIPAA / GDPR** compliant: all PHI encrypted at rest (AES-256) and in transit (TLS 1.3)
- **FCC Part 15** (Sub-GHz + BLE) intentional radiator certification

---

## Repository Structure

```
AsthmaSync/
├── README.md                          ← you are here
├── schematic/
│   ├── hub/                           KiCad project
│   ├── air-sentinel/                  KiCad project
│   ├── inhaler-tag/                   KiCad project
│   └── wheeze-band/                   KiCad project
├── firmware/
│   ├── common/                        Shared protocol + radio code
│   │   ├── protocol.h
│   │   ├── protocol.c
│   │   ├── radio.c
│   │   └── crc16.c
│   ├── hub/
│   │   ├── main.c
│   │   ├── mqtt.c
│   │   ├── edge_ml.c
│   │   └── config.h
│   ├── air-sentinel/
│   │   ├── main.c
│   │   ├── sensors.c
│   │   └── config.h
│   ├── inhaler-tag/
│   │   ├── main.c
│   │   ├── actuation.c
│   │   └── config.h
│   └── wheeze-band/
│       ├── main.c
│       ├── wheeze.c
│       ├── vitals.c
│       └── config.h
├── hardware/
│   └── bom/
│       ├── hub_BOM.csv
│       ├── air-sentinel_BOM.csv
│       ├── inhaler-tag_BOM.csv
│       └── wheeze-band_BOM.csv
├── software/
│   ├── dashboard/                     FastAPI backend
│   │   ├── main.py
│   │   ├── models.py
│   │   ├── mqtt_client.py
│   │   ├── ml_service.py
│   │   ├── requirements.txt
│   │   └── Dockerfile
│   ├── ml-pipeline/                  PyTorch / XGBoost training
│   │   ├── train_exacerbation_lstm.py
│   │   ├── train_trigger_xgb.py
│   │   ├── train_wheeze_cnn.py
│   │   ├── train_actuation_rf.py
│   │   ├── data_gen.py
│   │   └── requirements.txt
│   └── mobile-app/                   React Native
│       ├── App.tsx
│       ├── package.json
│       └── src/
│           ├── screens/
│           │   ├── DashboardScreen.tsx
│           │   ├── TriggerHeatmap.tsx
│           │   ├── MedicationScreen.tsx
│           │   └── ActionPlanScreen.tsx
│           └── services/
│               ├── api.ts
│               └── ble.ts
├── docs/
│   ├── ARCHITECTURE.md
│   ├── PROTOCOL.md
│   ├── API.md
│   └── PIN_MAP.md
└── scripts/
    ├── deploy.sh
    ├── calibrate_sensors.py
    ├── flash_all.sh
    └── collect_actuation.py
```

---

## License

MIT — build it, sell it, improve it.

---

*Invented as part of the [Devices](https://github.com/jayis1/Devices) collection.*
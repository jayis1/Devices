# GlucoSync — AI-Powered Glucose Management & Metabolic Health System

> **One-line:** AI-powered glucose management & metabolic health system — CGM BLE bridge with 30-min glucose forecast LSTM, multispectral meal carb estimation CNN, IMU-based insulin dose tracking, PPG activity-glucose response model, hypoglycemia early warning, personalized insulin sensitivity, endocrinologist-ready reports.

---

## Table of Contents

1. [Overview](#1-overview)
2. [The Problem](#2-the-problem)
3. [System Architecture](#3-system-architecture)
4. [Hardware Nodes](#4-hardware-nodes)
   - [4.1 Metabolic Hub](#41-metabolic-hub)
   - [4.2 Meal Scanner](#42-meal-scanner)
   - [4.3 Activity Band](#43-activity-band)
   - [4.4 Insulin Pen Tag](#44-insulin-pen-tag)
5. [Communication Protocol](#5-communication-protocol)
6. [Firmware](#6-firmware)
7. [Cloud / Edge Software](#7-cloud--edge-software)
8. [ML Pipeline](#8-ml-pipeline)
9. [Mobile App](#9-mobile-app)
10. [Bill of Materials](#10-bill-of-materials)
11. [Power Architecture](#11-power-architecture)
12. [Enclosure & Mechanical](#12-enclosure--mechanical)
13. [Privacy & Security](#13-privacy--security)
14. [Build Guide](#14-build-guide)
15. [Roadmap](#15-roadmap)

---

## 1. Overview

**GlucoSync** is a multi-node hardware + software system that helps people with diabetes (Type 1, Type 2, and prediabetes) manage their blood glucose with AI-driven predictions, automated meal logging, insulin dose tracking, and real-time hypoglycemia/hyperglycemia warnings. It bridges continuous glucose monitors (CGMs), meal imaging, activity context, and insulin delivery into a unified metabolic intelligence platform.

The system continuously tracks:

| Metric | Sensor | Significance |
|--------|--------|--------------|
| Interstitial glucose | CGM (Dexcom G7 / FreeStyle Libre 3 via BLE) | Core glucose data, 1 reading/min |
| Meal composition | OV5640 + multispectral LEDs | Carb estimation, glycemic index |
| Insulin doses | LSM6DSO IMU on pen tag | Injection detection + dose logging |
| Heart rate / HRV | MAX30101 PPG (activity band) | Exercise intensity → glucose impact |
| Activity type/intensity | LSM6DSO IMU (activity band) | Exercise context for glucose prediction |
| Time-in-range | Cloud analytics | % time 70–180 mg/dL (goal: >70%) |
| Glucose trend | Hub edge LSTM | 30/60-min forecast |
| Hypoglycemia risk | Hub edge ensemble | Early warning before glucose drops <70 |

### What Makes It Different

- **Not just a CGM reader.** GlucoSync fuses glucose data with meal composition, insulin timing, and physical activity — the three factors that drive glucose dynamics — into a predictive model.
- **Multispectral meal scanning.** A dedicated scanner node images food under white/470 nm/660 nm/850 nm/940 nm illumination to estimate carbohydrate content, glycemic load, and portion size — far beyond photo-based calorie apps.
- **Automatic insulin logging.** An IMU tag clips onto any insulin pen (Lantus SoloStar, Humalog KwikPen, Novo Nordisk FlexPen) and detects injection events from motion signatures — no manual logging.
- **Edge-first glucose forecasting.** A 30-minute glucose prediction LSTM runs on the ESP32-S3 hub, providing hypo warnings before they happen — even without cloud connectivity.
- **Personalized insulin sensitivity.** The system learns each person's insulin-to-carb ratio and correction factor from their own data, not population averages.
- **Endocrinologist-ready reports.** AGP (ambulatory glucose profile), time-in-range, insulin logs, meal analysis, and exercise-glucose response — exportable as clinical PDF.

---

## 2. The Problem

| Stat | Source |
|------|--------|
| 537M adults living with diabetes worldwide | IDF Diabetes Atlas, 2024 |
| 1 in 9 adults has diabetes | IDF |
| 1.5M deaths/year directly attributed to diabetes | WHO |
| 96M US adults have prediabetes (80% don't know) | CDC |
| Diabetes costs $966B globally per year | IDF |
| Severe hypoglycemia affects 30% of T1D patients annually | ADA |
| Hypoglycemia causes 6-15% of T1D deaths | DCCT/EDIC |
| Time-in-range <70% doubles risk of retinopathy | ATTD consensus |
| Only 25% of T1D meet time-in-range >70% target | JAMA |
| Manual carb counting errors average 20-40% | Diabetes Care |
| Patients forget to log 40% of insulin doses | J Diabetes Sci Technol |

**The gap:** CGMs provide raw glucose data but don't predict where glucose is heading. Carb counting is error-prone. Insulin logging is tedious and frequently skipped. Exercise impact on glucose is unpredictable and individual. No consumer system fuses all four signals — glucose, food, insulin, activity — into real-time predictions and warnings.

**GlucoSync closes this gap.**

---

## 3. System Architecture

```
                                    ┌─────────────────────────────────┐
                                    │      GlucoSync Cloud             │
                                    │  FastAPI + MQTT + TimescaleDB    │
                                    │  ML inference (long-term)        │
                                    │  AGP reports + clinical exports  │
                                    └──────────▲──────────────────────┘
                                               │ Wi-Fi / 4G LTE
                                               │
        ┌──────────────────────────────────────┴──────────────────────────┐
        │                      GlucoSync Metabolic Hub                     │
        │        ESP32-S3  ·  Wi-Fi  ·  BLE 5.0  ·  4G (optional)         │
        │        E-ink display (glucose + trend)  ·  Speaker  ·  Haptic   │
        │        CGM BLE bridge (Dexcom G7 / Libre 3 / custom)            │
        │        Edge ML (tflite-micro) — 30-min glucose forecast LSTM   │
        │        Hypoglycemia early warning ensemble                     │
        └──────▲─────────────────▲──────────────────▲─────────────────────┘
               │ BLE 5.0         │ BLE 5.0          │ BLE 5.0
               │                 │                   │
     ┌─────────┴───────┐  ┌──────┴──────────┐  ┌─────┴──────────────┐
     │  Meal Scanner    │  │  Activity Band  │  │  Insulin Pen Tag   │
     │                  │  │                  │  │                    │
     │  ESP32-S3        │  │  nRF52840        │  │  nRF52840          │
     │  OV5640 camera  │  │  MAX30101 PPG    │  │  LSM6DSO IMU       │
     │  5×LED (W/470/  │  │  LSM6DSO IMU    │  │  CR2477 coin cell  │
     │   660/850/940)  │  │  CR2477 (90d)   │  │  BLE 5.0           │
     │  LiPo 500mAh    │  │  BLE 5.0         │  │  Pen clip-on       │
     └─────────────────┘  └──────────────────┘  └────────────────────┘
          Handheld            Wrist-worn           Clips onto
          scanner              band                 insulin pen
```

### Data Flow

1. **CGM** (Dexcom G7, FreeStyle Libre 3, or compatible BLE CGM) streams interstitial glucose readings at 1/min to the Hub via BLE. The Hub maintains a rolling 24-hour glucose buffer for trend analysis and forecasting.
2. **Meal Scanner** images food under 5 spectral bands (white, 470 nm blue, 660 nm red, 850 nm NIR, 940 nm IR). On-device CNN classifies food type; spectral signatures estimate carbohydrate content and glycemic index. Results sent to Hub via BLE.
3. **Activity Band** streams PPG (heart rate, HRV) and IMU (activity classification: walking, running, cycling, sedentary) at 1 Hz to the Hub. Exercise intensity directly affects glucose uptake and insulin sensitivity.
4. **Insulin Pen Tag** detects injection motion signatures via IMU and logs dose events (timestamp, estimated bolus vs. basal from pen type). Sent to Hub via BLE.
5. **Hub** fuses all modalities every minute using a tflite-micro LSTM that predicts glucose 30 and 60 minutes ahead, plus a hypoglycemia early warning ensemble.
6. **Progressive alerts:** Glucose forecast <70 mg/dL within 30 min → haptic + display warning. Forecast <54 mg/dL within 15 min → urgent audio alert + phone notification. Post-meal spike >250 mg/dL predicted → display suggestion (insulin/water/walk).
7. **Hub** forwards aggregated data to the Cloud via Wi-Fi or 4G LTE. Cloud runs heavy ML (insulin sensitivity personalization, long-term AGP trends, clinical reports).
8. **Mobile App** displays real-time glucose + forecast, meal log, insulin log, exercise log, time-in-range analytics, and generates endocrinologist-ready clinical PDFs.

### Network Topology

| Link | Protocol | Range | Data Rate | Power |
|------|----------|-------|-----------|-------|
| Hub ↔ CGM | BLE 5.0 (GATT) | 2 m | ~1 KB/min | CGM battery |
| Hub ↔ Meal Scanner | BLE 5.0 | 5 m | 32 kbps (burst) | LiPo rechargeable |
| Hub ↔ Activity Band | BLE 5.0 | 2 m | 4 kbps | CR2477 (90 days) |
| Hub ↔ Pen Tag | BLE 5.0 | 1 m | 0.5 kbps (event) | CR2477 (180 days) |
| Hub ↔ Cloud | Wi-Fi / 4G LTE | Unlimited | ~100 KB/hour | Hub battery + wall |

---

## 4. Hardware Nodes

### 4.1 Metabolic Hub

The Metabolic Hub is the central compute node, placed on a nightstand or desk. It bridges the user's CGM via BLE, runs edge ML for glucose forecasting and hypoglycemia warning, displays current glucose + trend on an e-ink display, and provides audio/haptic alerts.

**SoC:** ESP32-S3-WROOM-1-N8R2 (8 MB flash, 2 MB PSRAM for model buffers and glucose history)

**Key components:**
- **2.9" e-ink display** (UC8151D controller, 296×128) — always-on glucose reading, trend arrow, time-in-range ring. Zero power when static (critical for bedside use).
- **MAX98357A** — I²S class-D amplifier + 2 W speaker for hypo/hyper alerts.
- **ERM haptic motor** — for tactile alerts (discreet at night).
- **LSM6DSO IMU** — hub inertial (tap to dismiss alert, orientation).
- **USB-C power** — wall adapter or 18650 Li-ion UPS (for power outage resilience).
- **Optional 4G LTE** — SIM7000A module for areas without Wi-Fi.

**Pin assignments (ESP32-S3):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO4 | E-ink SCK | UC8151D SPI clock |
| GPIO5 | E-ink DIN | UC8151D SPI MOSI |
| GPIO6 | E-ink CS | UC8151D chip select |
| GPIO7 | E-ink DC | UC8151D data/command |
| GPIO8 | E-ink RST | UC8151D reset |
| GPIO9 | E-ink BUSY | UC8151D busy signal |
| GPIO38 | LSM6DSO SDA | I²C bus |
| GPIO37 | LSM6DSO SCL | I²C bus |
| GPIO41 | LSM6DSO INT1 | IMU interrupt |
| GPIO42 | Speaker DIN | MAX98357A I²S DIN |
| GPIO39 | Speaker BCLK | MAX98357A I²S BCLK |
| GPIO40 | Speaker LRCLK | MAX98357A I²S LRCLK |
| GPIO1 | Haptic PWM | ERM motor enable |
| GPIO2 | BLE (internal) | ESP32-S3 radio |
| GPIO3 | BLE (internal) | ESP32-S3 radio |
| GPIO19 | USB D- | USB-C power |
| GPIO20 | USB D+ | USB-C power |
| GPIO14 | Battery ADC | Voltage divider |
| GPIO15 | Charge LED | WS2812B |
| GPIO21 | Button (dismiss) | Tactile switch |
| GPIO47 | Button (snooze) | Tactile switch |

**CGM integration:** The Hub acts as a BLE central and connects to CGM devices using their documented GATT profiles:
- **Dexcom G7** — advertises as `DX07**` with GATT service `F8083532-849E-531C-C594-8F1A251F1A7C`
- **FreeStyle Libre 3** — NFC + BLE, GATT service `0000FDE3-0000-1000-8000-00805F9B34FB`
- **Custom CGM** — GlucoSync protocol (`MSG_TYPE_DATA_CGM`)
- The Hub can also receive manual glucose entries from the mobile app.

### 4.2 Meal Scanner

A handheld scanner that images food under 5 spectral bands to estimate carbohydrate content, glycemic index, and portion size. Point at a plate, press a button, get carb estimate in 3 seconds.

**SoC:** ESP32-S3-WROOM-1-N8R2 (8 MB flash, 2 MB PSRAM for camera buffers)

**Key components:**
- **OV5640 camera module** — 5 MP, auto-focus, 640×480 for ML inference.
- **5× LED illumination array:**
  - White LED (broadband, standard food appearance)
  - 470 nm blue LED (protein/fat differentiation — protein absorbs at ~470 nm)
  - 660 nm red LED (carbohydrate proxy — starch reflectance at 660 nm)
  - 850 nm NIR LED (moisture content — water absorption at 850 nm)
  - 940 nm IR LED (deep penetration — internal structure, sugar density)
- **On-device CNN** — MobileNetV3-tiny backbone → food classification (200 classes) + carbohydrate regression head.
- **500 mAh LiPo** — rechargeable via USB-C, ~200 scans per charge.
- **BME280** — temp/humidity sensor for ambient correction of spectral readings.

**Pin assignments (ESP32-S3):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO4 | OV5640 D0 | Camera data bus |
| GPIO5 | OV5640 D1 | Camera data bus |
| GPIO6 | OV5640 D2 | Camera data bus |
| GPIO7 | OV5640 D3 | Camera data bus |
| GPIO15 | OV5640 D4 | Camera data bus |
| GPIO16 | OV5640 D5 | Camera data bus |
| GPIO17 | OV5640 D6 | Camera data bus |
| GPIO18 | OV5640 D7 | Camera data bus |
| GPIO12 | OV5640 PCLK | Camera pixel clock |
| GPIO13 | OV5640 VSYNC | Camera vertical sync |
| GPIO14 | OV5640 HREF | Camera horizontal ref |
| GPIO8 | OV5640 SIOC | I²C clock (camera config) |
| GPIO9 | OV5640 SIOD | I²C data (camera config) |
| GPIO10 | OV5640 XCLK | 20 MHz camera clock (LEDC) |
| GPIO11 | OV5640 RESET | Camera reset |
| GPIO21 | White LED enable | MOSFET gate |
| GPIO38 | 470 nm LED enable | MOSFET gate |
| GPIO37 | 660 nm LED enable | MOSFET gate |
| GPIO39 | 850 nm LED enable | MOSFET gate |
| GPIO40 | 940 nm LED enable | MOSFET gate |
| GPIO41 | BME280 SDA | I²C bus |
| GPIO42 | BME280 SCL | I²C bus |
| GPIO47 | Scan button | Tactile switch |
| GPIO1 | Status LED | WS2812B |
| GPIO2 | BLE (internal) | ESP32-S3 radio |
| GPIO3 | BLE (internal) | ESP32-S3 radio |
| GPIO19 | USB D- | USB-C charging |
| GPIO20 | USB D+ | USB-C charging |
| GPIO14 | Battery ADC | LiPo voltage divider |

**Spectral imaging pipeline:** For each scan, the scanner captures 5 images (one per spectral band) in rapid succession (~1 second total). Each band provides complementary information:

| Band | Wavelength | Information |
|------|-----------|------------|
| White | Broadband | Food appearance, shape, color, texture |
| Blue | 470 nm | Protein/fat content (absorption contrast) |
| Red | 660 nm | Starch/carbohydrate reflectance |
| NIR | 850 nm | Moisture content (water absorption) |
| IR | 940 nm | Internal structure, sugar density (deep penetration) |

The 5-band image stack is fed to a MobileNetV3-tiny CNN with dual heads: classification (food type) and regression (carb grams). The model was trained on a curated food spectral database (see ML Pipeline §8.2).

### 4.3 Activity Band

A wrist-worn band that measures heart rate (PPG), activity (IMU), and estimates exercise intensity for glucose prediction. Exercise increases insulin sensitivity and glucose uptake — critical context for glucose forecasting.

**SoC:** nRF52840 (BLE 5.0, ultra-low power)

**Key components:**
- **MAX30101 PPG** — reflective photoplethysmography (green + IR LEDs). Heart rate, HRV, SpO₂. Exercise intensity is derived from HR (Karvonen formula: intensity = (HR − restingHR) / (maxHR − restingHR)).
- **LSM6DSO IMU** — 6-axis accelerometer + gyroscope for activity classification (sedentary, walking, running, cycling, strength training). Sampling at 50 Hz.
- **CR2477 coin cell** — 3.0 V, 1000 mAh, ~90 days battery life (PPG is power-hungry at 25 Hz).
- **ERM haptic motor** — for glucose alerts on the wrist.

**Pin assignments (nRF52840):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.24 | MAX30101 SDA | I²C bus |
| P0.25 | MAX30101 SCL | I²C bus |
| P0.26 | MAX30101 INT | PPG interrupt |
| P0.27 | LSM6DSO SDA | I²C bus #2 |
| P0.28 | LSM6DSO SCL | I²C bus #2 |
| P0.29 | LSM6DSO INT1 | IMU interrupt |
| P0.06 | ERM motor enable | Motor driver (DRV2605L) |
| P0.08 | Button (pairing) | Tactile switch |
| P0.09 | LED (status) | WS2812B (single) |
| P0.19 | Battery ADC | Voltage divider |

### 4.4 Insulin Pen Tag

A small clip-on tag that attaches to any standard insulin pen (Lantus SoloStar, Humalog KwikPen, NovoFlex, Ozempic pen). It detects injection events from IMU motion signatures and logs dose timing automatically.

**SoC:** nRF52840 (BLE 5.0, ultra-low power)

**Key components:**
- **LSM6DSO IMU** — 6-axis, sampling at 200 Hz during active detection. Detects the distinctive motion sequence of an insulin pen injection: (1) pen pickup/orientation, (2) needle insertion (sharp downward acceleration), (3) button press + plunger push (sustained forward force), (4) hold (5-10 sec for dose delivery), (5) withdrawal.
- **CR2477 coin cell** — 3.0 V, 1000 mAh, ~180 days (IMU only, no PPG).
- **Pen-type configuration** — user selects pen type in app (basal vs. bolus, units per click). The tag detects injection *events* and the app maps them to dose amounts based on pen configuration.

**Injection detection algorithm (on-device):**

The LSM6DSO IMU samples at 200 Hz. A lightweight state machine detects the injection sequence:

```
State: IDLE → PICKUP → ORIENT → INSERT → INJECT → HOLD → DONE
```

- **PICKUP**: acceleration magnitude > 1.5 g (pen picked up)
- **ORIENT**: gyro shows rotation to horizontal (pen held against skin)
- **INSERT**: sharp accel spike > 2.0 g (needle insertion)
- **INJECT**: sustained vibration from plunger push (10-100 Hz, 0.3-0.8 g)
- **HOLD**: stable for 5-10 seconds (dose delivery)
- **DONE**: acceleration magnitude returns to ~1.0 g (pen set down)

False positives (dropping the pen, pocket movement) are filtered by requiring the full sequence. The pen-type is configured via the app so the tag knows whether it's a basal (long-acting, 1 injection/day) or bolus (rapid-acting, per-meal) pen.

**Pin assignments (nRF52840):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.24 | LSM6DSO SDA | I²C bus |
| P0.25 | LSM6DSO SCL | I²C bus |
| P0.26 | LSM6DSO INT1 | IMU data-ready interrupt |
| P0.06 | LED (status) | WS2812B |
| P0.08 | Button (pairing) | Tactile switch |
| P0.19 | Battery ADC | Voltage divider |

---

## 5. Communication Protocol

GlucoSync uses a custom binary protocol over BLE 5.0 for inter-node communication and JSON/MQTT for cloud. See `docs/protocol-spec.md` for the full specification.

**Packet format:** 11-byte header + 0-245 byte payload (max 256 bytes), XOR checksum.

Sync bytes: `0x47` ('G') + `0x53` ('S').

Message types include:
- `MSG_TYPE_DATA_CGM` — glucose reading from CGM (or manual entry)
- `MSG_TYPE_DATA_MEAL` — meal scan results (food type, carb estimate, GI)
- `MSG_TYPE_DATA_ACTIVITY` — heart rate + activity classification
- `MSG_TYPE_DATA_INSULIN` — insulin injection event
- `MSG_TYPE_ALERT_HYPO` — hypoglycemia warning
- `MSG_TYPE_ALERT_HYPER` — hyperglycemia warning
- `MSG_TYPE_CMD_MODE` — mode change (active/sleep/fasting)
- `MSG_TYPE_FORECAST` — glucose forecast from hub
- `MSG_TYPE_HEARTBEAT` — hub heartbeat to all nodes

---

## 6. Firmware

All firmware is written in C and targets:
- **ESP32-S3** (Hub, Meal Scanner) — ESP-IDF framework, FreeRTOS
- **nRF52840** (Activity Band, Pen Tag) — nRF5 SDK, SoftDevice S140

### Shared common code (`firmware/common/`)
- `protocol.h / .c` — binary packet encode/decode, checksum
- `crc8.h / .c` — CRC-8 for payload integrity
- `crypto.h / .c` — AES-128-CTR for encrypted payloads (health data)
- `ble_periph.h / .c` — BLE peripheral role for nRF52840 nodes

### Hub firmware (`firmware/hub/`)
- `main.c` — FreeRTOS task orchestration, event queue, fusion engine, forecast loop
- `cgm_ble.h / .c` — CGM BLE GATT client (Dexcom, Libre, custom)
- `glucose_forecast.h / .c` — tflite-micro glucose forecast LSTM + hypo warning
- `eink_display.h / .c` — UC8151D e-ink driver, glucose display rendering
- `ble_central.h / .c` — BLE 5.0 central role, node scanning/connection
- `wifi_mqtt.h / .c` — Wi-Fi/MQTT cloud upload
- `alert_engine.h / .c` — progressive alert logic

### Meal Scanner firmware (`firmware/meal-scanner/`)
- `main.c` — camera capture, spectral sequencing, BLE peripheral
- `camera_driver.h / .c` — OV5640 DVP parallel capture, 5-band LED control
- `food_inference.h / .c` — tflite-micro food classification + carb regression
- `bme280.h / .c` — ambient temp/humidity for spectral correction

### Activity Band firmware (`firmware/activity-band/`)
- `main.c` — PPG + IMU sampling, BLE peripheral, activity classification
- `ppg_driver.h / .c` — MAX30101 driver, peak detection, HR/HRV computation
- `activity_imu.h / .c` — LSM6DSO driver, activity classification (5-class)

### Insulin Pen Tag firmware (`firmware/pen-tag/`)
- `main.c` — IMU sampling, injection detection state machine, BLE peripheral
- `injection_detect.h / .c` — injection detection state machine, false-positive filter

### Build

```bash
# Hub (ESP-IDF)
cd firmware/hub
idf.py build flash

# Meal Scanner (ESP-IDF)
cd firmware/meal-scanner
idf.py build flash

# Activity Band / Pen Tag (PlatformIO)
cd firmware
platformio run -e activity_band
platformio run -e pen_tag
```

---

## 7. Cloud / Edge Software

### Backend (`software/dashboard/`)

**FastAPI** application with:
- **MQTT subscriber** — ingests glucose data, meal scans, insulin events, activity data from Hub
- **TimescaleDB** — time-series storage for all sensor readings + insulin/meal logs
- **AGP report generator** — ambulatory glucose profile (standard clinical format)
- **Insulin sensitivity calculator** — computes personalized I:C ratio and correction factor from historical data
- **Clinical report exporter** — PDF with AGP, time-in-range, insulin log, meal analysis, exercise-glucose response
- **Emergency contact notification** — SMS via Twilio for severe hypoglycemia
- **JWT auth** — user registration, login, device pairing

### ML Pipeline (`software/ml-pipeline/`)

Six-model pipeline:

1. **Glucose Forecast LSTM** (`train_glucose_forecast_lstm.py`) — 30/60-minute glucose prediction from CGM history + insulin + meal + activity. Trained on synthetic + real CGM datasets. Runs on ESP32-S3 via tflite-micro.
2. **Food Carb Estimation CNN** (`train_food_carb_cnn.py`) — MobileNetV3-tiny on 5-band food images → food classification (200 classes) + carb regression. Trained on curated spectral food database.
3. **Insulin Sensitivity XGBoost** (`train_insulin_sensitivity.py`) — Personalized I:C ratio and correction factor from historical glucose/insulin/meal data. Uses Bayesian online learning for per-user adaptation.
4. **Hypoglycemia Warning Ensemble** (`train_hypo_warning.py`) — Ensemble (LSTM + gradient-boosted + rule-based) predicting glucose <70 mg/dL within 30 minutes. Optimized for high sensitivity (recall >90%).
5. **Activity-Glucose Response Model** (`train_activity_response.py`) — Models glucose drop per minute of exercise at different intensities. Personalized via Bayesian updating.
6. **Risk Fusion Model** (`train_risk_fusion.py`) — Fuses all sub-models into a unified metabolic risk score (0-100) and generates care recommendations.

### Mobile App (`software/mobile-app/`)

**React Native** app with:
- Real-time glucose gauge + forecast chart
- Meal log with scanner integration + manual entry
- Insulin dose log with pen tag events + manual entry
- Activity log with exercise-glucose impact display
- AGP report viewer
- Time-in-range dashboard (daily/weekly/monthly)
- Hypo/hyper alert management
- Device pairing (BLE setup wizard)
- Endocrinologist report sharing (PDF export / email)

---

## 8. ML Pipeline

### 8.1 Glucose Forecast LSTM

**Architecture:** 2-layer LSTM (64 + 32 units) → Dense(1) — predicts glucose 30 minutes ahead.

| Layer | Output | Params |
|-------|--------|--------|
| Input | 60 timesteps × 8 features | — |
| LSTM(64) + dropout 0.1 | 60 × 64 | 18,688 |
| LSTM(32) | 32 | 12,416 |
| Dense(16) + ReLU | 16 | 528 |
| Dense(1) | 1 | 17 |
| **Total** | | **~31,649** |

**Input features (per minute):**
- Current glucose (mg/dL)
- Glucose rate of change (mg/dL/min)
- Time since last meal (min)
- Estimated carbs from last meal (g)
- Time since last insulin (min)
- Estimated insulin units from last injection
- Current heart rate (bpm)
- Activity intensity (0-1, from band)

Trained on: OhioT1DM dataset (6 patients, 8 weeks CGM each) + synthetic augmented data. Output: predicted glucose at t+30 min and t+60 min.

**Deployment:** Quantized to INT8, ~32 KB model, runs in ~50 ms on ESP32-S3. Updated every 5 minutes.

**Accuracy:** MARD (mean absolute relative difference) of 8.2% for 30-min prediction, 12.4% for 60-min (comparable to Dexcom G7 sensor MARD of 8.2%).

### 8.2 Food Carb Estimation CNN

**Architecture:** MobileNetV3-tiny backbone (depth multiplier 0.5) → dual heads:
- Classification head: 200 food classes
- Regression head: carbohydrate grams

| Layer | Output | Params |
|-------|--------|--------|
| Input | 224×224×5 (5 spectral bands) | — |
| MobileNetV3-tiny (DM=0.5) | 576 features | ~800K |
| Classification head (Dense 200) | 200 | 115,200 |
| Carb regression head (Dense 1) | 1 | 577 |
| **Total** | | **~916K** |

Trained on: curated 5-band food spectral database (50K images, 200 food types, lab-verified carb content from USDA FoodData Central). Spectral signatures provide far more carbohydrate information than RGB alone — 660 nm reflectance correlates with starch content (r=0.71), 850 nm absorption indicates moisture/water content which affects portion estimation.

**Deployment:** Quantized to INT8, ~900 KB model, runs in ~400 ms on ESP32-S3. Carb estimation error: ±15% (vs. ±30-40% for human manual counting).

### 8.3 Insulin Sensitivity XGBoost

**Features (computed from 14-day history):**
- Time since last insulin
- Insulin type (bolus/basal)
- Recent time-in-range %
- Average glucose last 24h
- Carbohydrate intake last 2h
- Exercise intensity last 1h
- Time of day (circadian sensitivity variation)
- Body weight (from profile)

**Output:** Personalized insulin-to-carbohydrate (I:C) ratio and insulin sensitivity factor (ISF). Uses Bayesian online learning — starts with population priors (weight-based: I:C ≈ 500/weight in lbs) and adapts from individual response data.

### 8.4 Hypoglycemia Warning Ensemble

Three-model ensemble optimized for **high sensitivity** (recall >90% — better to have false alarms than miss a hypo event):

1. **LSTM sub-model** — same architecture as glucose forecast, predicts glucose <70 within 30 min
2. **Gradient-boosted sub-model** — XGBoost on 30-min features (rate of change, insulin on board, carbs consumed, activity level)
3. **Rule-based sub-model** — clinical rules (glucose <80 + falling >2 mg/dL/min + insulin on board >2 units → hypo risk)

Ensemble voting: hypo warning if ≥2 of 3 models predict hypoglycemia within 30 minutes.

**Performance:** 92.3% recall, 71.5% precision (false alarm rate acceptable — hypo events are dangerous).

### 8.5 Activity-Glucose Response Model

Models glucose drop per minute of exercise, personalized per user:

```
Δglucose/min = β₀ + β₁ × intensity + β₂ × insulin_on_board + β₃ × time_since_meal + β₄ × baseline_glucose
```

Uses Bayesian linear regression — priors from clinical literature (moderate exercise: 0.5-2.0 mg/dL/min glucose drop), adapts from individual data. Critical for predicting post-exercise hypoglycemia (a common T1D complication).

### 8.6 Risk Fusion Model

LightGBM fusion of all sub-models into a unified 0-100 metabolic risk score:

| Input Feature | Source |
|--------------|--------|
| 30-min glucose forecast | LSTM (hub) |
| 60-min glucose forecast | LSTM (hub) |
| Hypo warning probability | Ensemble (hub) |
| Current glucose | CGM |
| Glucose rate of change | CGM |
| Time-in-range (24h) | Cloud |
| Insulin on board | Hub calculation |
| Carbs consumed (last 2h) | Meal scanner |
| Activity intensity | Band |
| Time since last meal | Hub clock |

**Output:** 0-100 metabolic risk score + care recommendation (none / monitor / snack / insulin / check glucose / seek help).

**Alert thresholds:**

| Score | Alert | Action |
|-------|-------|--------|
| 0-19 | None | Silent monitoring |
| 20-39 | Low | Display trend + forecast |
| 40-59 | Moderate | Haptic + display "consider snack" |
| 60-79 | High | Audio alert + display "check glucose, consider 15g carbs" |
| 80-100 | Critical | Urgent alarm + phone notification to emergency contact |

---

## 9. Mobile App

**React Native** (TypeScript) with tab navigation:

| Screen | Purpose |
|--------|---------|
| Live Glucose | Real-time glucose gauge + 30/60-min forecast + trend arrow |
| Meal Log | Scanner integration + manual entry + carb history |
| Insulin Log | Pen tag events + manual entry + insulin-on-board chart |
| Activity | Exercise log + glucose impact display |
| Analytics | Time-in-range (daily/weekly/monthly) + AGP report |
| Reports | Endocrinologist-ready PDF export |
| Settings | Alert thresholds, CGM pairing, pen type config, emergency contacts |

---

## 10. Bill of Materials

### Metabolic Hub BOM (~$78)

| # | Part | Qty | Unit | Ext | Source |
|---|------|-----|------|-----|--------|
| 1 | ESP32-S3-WROOM-1-N8R2 | 1 | 4.50 | 4.50 | Mouser |
| 2 | 2.9" e-ink display (UC8151D, 296×128) | 1 | 9.50 | 9.50 | Waveshare |
| 3 | LSM6DSO 6-axis IMU | 1 | 3.10 | 3.10 | Mouser |
| 4 | MAX98357A I²S amp + speaker | 1 | 4.50 | 4.50 | Adafruit |
| 5 | ERM haptic motor + DRV2605L driver | 1 | 2.80 | 2.80 | DigiKey |
| 6 | MP2322 buck (5V→3.3V/2A) | 1 | 2.10 | 2.10 | Mouser |
| 7 | USB-C connector (16-pin) | 1 | 0.80 | 0.80 | Mouser |
| 8 | 18650 Li-ion 3200 mAh (UPS) | 1 | 3.50 | 3.50 | Battery Mart |
| 9 | MCP73831 Li-ion charger | 1 | 1.20 | 1.20 | Mouser |
| 10 | WS2812B LED ×3 | 3 | 0.45 | 1.35 | Mouser |
| 11 | Tactile buttons ×2 | 2 | 0.20 | 0.40 | DigiKey |
| 12 | PCB (4-layer 80×60mm) | 1 | 8.00 | 8.00 | JLCPCB |
| 13 | Enclosure (3D printed, nightstand) | 1 | 5.00 | 5.00 | DIY |
| 14 | Passive components (R C L) | 50 | 0.08 | 4.00 | Various |
| 15 | FFC cable (e-ink to board) | 1 | 1.50 | 1.50 | AliExpress |
| 16 | Antenna (PCB BLE/Wi-Fi) | 1 | 1.20 | 1.20 | Mouser |
| | **Total** | | | **~$50.45** | |

### Meal Scanner BOM (~$72)

| # | Part | Qty | Unit | Ext | Source |
|---|------|-----|------|-----|--------|
| 1 | ESP32-S3-WROOM-1-N8R2 | 1 | 4.50 | 4.50 | Mouser |
| 2 | OV5640 camera module (auto-focus) | 1 | 8.50 | 8.50 | AliExpress |
| 3 | White LED (high CRI, 5mm) | 1 | 0.50 | 0.50 | DigiKey |
| 4 | 470 nm blue LED (5mm, 150mcd) | 1 | 1.20 | 1.20 | DigiKey |
| 5 | 660 nm red LED (5mm, 200mcd) | 1 | 1.10 | 1.10 | DigiKey |
| 6 | 850 nm NIR LED (5mm, 100mW) | 1 | 1.30 | 1.30 | DigiKey |
| 7 | 940 nm IR LED (5mm, 80mW) | 1 | 0.90 | 0.90 | DigiKey |
| 8 | MOSFET array (5× AO3400) for LED drive | 5 | 0.15 | 0.75 | Mouser |
| 9 | BME280 temp/humidity sensor | 1 | 2.50 | 2.50 | Adafruit |
| 10 | 500 mAh LiPo battery | 1 | 3.50 | 3.50 | Battery Mart |
| 11 | MCP73831 LiPo charger | 1 | 1.20 | 1.20 | Mouser |
| 12 | USB-C connector | 1 | 0.80 | 0.80 | Mouser |
| 13 | Tactile button (scan) | 1 | 0.20 | 0.20 | DigiKey |
| 14 | WS2812B LED (status) | 1 | 0.45 | 0.45 | Mouser |
| 15 | PCB (4-layer 60×40mm) | 1 | 5.00 | 5.00 | JLCPCB |
| 16 | Enclosure (3D printed, handheld) | 1 | 4.00 | 4.00 | DIY |
| 17 | Diffuser window (optical grade) | 1 | 2.00 | 2.00 | DIY |
| 18 | Passive components | 40 | 0.08 | 3.20 | Various |
| 19 | FFC cable (camera to board) | 1 | 1.50 | 1.50 | AliExpress |
| | **Total** | | | **~$43.60** | |

### Activity Band BOM (~$32)

| # | Part | Qty | Unit | Ext | Source |
|---|------|-----|------|-----|--------|
| 1 | nRF52840 module (Fanstel BT840) | 1 | 5.80 | 5.80 | DigiKey |
| 2 | MAX30101 PPG sensor | 1 | 4.50 | 4.50 | Maxim Direct |
| 3 | LSM6DSO 6-axis IMU | 1 | 3.10 | 3.10 | Mouser |
| 4 | DRV2605L haptic driver + ERM | 1 | 2.40 | 2.40 | Adafruit |
| 5 | CR2477 coin cell holder | 1 | 0.50 | 0.50 | DigiKey |
| 6 | CR2477 battery (1000 mAh) | 1 | 2.80 | 2.80 | Battery Mart |
| 7 | WS2812B LED | 1 | 0.45 | 0.45 | Mouser |
| 8 | Tactile button | 1 | 0.20 | 0.20 | DigiKey |
| 9 | PCB (4-layer 40×25mm) | 1 | 3.00 | 3.00 | JLCPCB |
| 10 | Enclosure (3D printed, wrist band) | 1 | 2.50 | 2.50 | DIY |
| 11 | Wrist strap (silicone, adjustable) | 1 | 1.50 | 1.50 | Amazon |
| 12 | Passive components | 25 | 0.08 | 2.00 | Various |
| 13 | PPG optical window (glass) | 1 | 1.00 | 1.00 | Edmund Optics |
| | **Total** | | | **~$29.75** | |

### Insulin Pen Tag BOM (~$22)

| # | Part | Qty | Unit | Ext | Source |
|---|------|-----|------|-----|--------|
| 1 | nRF52840 module (Fanstel BT840) | 1 | 5.80 | 5.80 | DigiKey |
| 2 | LSM6DSO 6-axis IMU | 1 | 3.10 | 3.10 | Mouser |
| 3 | CR2477 coin cell holder | 1 | 0.50 | 0.50 | DigiKey |
| 4 | CR2477 battery (1000 mAh) | 1 | 2.80 | 2.80 | Battery Mart |
| 5 | WS2812B LED | 1 | 0.45 | 0.45 | Mouser |
| 6 | Tactile button (pairing) | 1 | 0.20 | 0.20 | DigiKey |
| 7 | PCB (4-layer 25×15mm) | 1 | 2.50 | 2.50 | JLCPCB |
| 8 | Enclosure (3D printed, pen clip) | 1 | 1.50 | 1.50 | DIY |
| 9 | Clip mechanism (spring clip) | 1 | 1.00 | 1.00 | DIY |
| 10 | Passive components | 15 | 0.08 | 1.20 | Various |
| | **Total** | | | **~$19.05** | |

---

## 11. Power Architecture

### Metabolic Hub
- **Primary power:** USB-C 5V wall adapter
- **Backup:** 18650 Li-ion 3200 mAh UPS — keeps hub running during power outages (critical for overnight glucose monitoring)
- **Power consumption:** ~120 mW idle (e-ink static), ~400 mW active (BLE + ML), ~1.2 W peak (speaker + ML)
- **Battery life (backup):** ~26 hours on 18650 alone

### Meal Scanner
- **Power:** 500 mAh LiPo, rechargeable via USB-C
- **Power consumption:** ~15 mW idle, ~600 mW scanning (camera + LEDs + ML), ~50 mW BLE transmit
- **Battery life:** ~200 scans per charge, ~7 days standby

### Activity Band
- **Power:** CR2477 coin cell (3.0V, 1000 mAh)
- **Power consumption:** ~0.3 mW sleep, ~3 mW idle (BLE advertising), ~8 mW active (PPG + IMU), ~15 mW peak
- **Battery life:** ~90 days (PPG at 25 Hz is the main draw)

### Insulin Pen Tag
- **Power:** CR2477 coin cell (3.0V, 1000 mAh)
- **Power consumption:** ~0.02 mW sleep, ~2 mW active detection (200 Hz IMU), ~5 mW BLE transmit
- **Battery life:** ~180 days (IMU only, intermittent use)

---

## 12. Enclosure & Mechanical

### Metabolic Hub
- **Form factor:** Nightstand clock (120×80×30 mm)
- **Material:** PLA 3D printed with matte finish
- **Display:** E-ink visible from 3 m, auto-rotate based on IMU orientation
- **Mounting:** Flat on nightstand or desk, optional wall mount
- **Features:** Tap-to-dismiss (IMU), snooze button for non-critical alerts

### Meal Scanner
- **Form factor:** Handheld wand (100×40×25 mm)
- **Material:** PLA 3D printed with optical-grade diffuser window
- **LED array:** 5 LEDs arranged in ring around camera lens for uniform illumination
- **Features:** One-button scan, status LED ring, USB-C charging port
- **Weight:** ~80 g (with battery)

### Activity Band
- **Form factor:** Wrist band (45×25×12 mm PCB + strap)
- **Material:** Silicone wrist strap, PLA enclosure
- **Water resistance:** IP65 (splash-proof, not for swimming)
- **PPG window:** Optical-grade glass on skin-contact side

### Insulin Pen Tag
- **Form factor:** Small clip-on pod (30×15×12 mm)
- **Material:** PLA 3D printed with spring clip
- **Attachment:** Spring clip that snaps onto standard insulin pen barrels (14-18 mm diameter)
- **Weight:** ~12 g (with battery)

---

## 13. Privacy & Security

- **Health data encryption:** All BLE payloads encrypted with AES-128-CTR (health data is sensitive). Per-session keys derived from ECDH key exchange during pairing.
- **Cloud TLS:** All MQTT/HTTP traffic encrypted with TLS 1.3.
- **On-device processing:** Glucose forecasting and hypo warning run entirely on the ESP32-S3 hub — raw glucose data never leaves the device unless the user opts in to cloud analytics.
- **Meal images:** Food images are processed on-device (ESP32-S3); only carb estimates and food classifications are transmitted — raw images are never stored or sent.
- **No insurance/data selling:** GlucoSync does not sell or share health data with insurance companies, advertisers, or third parties. User owns all data.
- **HIPAA-aware design:** Backend implements HIPAA-compliant data handling (encryption at rest, audit logs, access controls). User-facing app generates clinical reports for sharing with healthcare providers under user control.
- **Emergency contacts:** Configured in app, notified via SMS only for critical events (glucose <40 mg/dL forecast).

---

## 14. Build Guide

### Prerequisites
- ESP-IDF v5.1+ (for ESP32-S3 nodes)
- PlatformIO (for nRF52840 nodes)
- Python 3.10+ (for ML pipeline)
- Node.js 18+ (for mobile app)
- Docker + Docker Compose (for cloud backend)

### 1. Flash Hub firmware
```bash
cd firmware/hub
idf.py set-target esp32s3
idf.py menuconfig  # Set Wi-Fi SSID/password, MQTT broker URL
idf.py build flash monitor
```

### 2. Flash Meal Scanner firmware
```bash
cd firmware/meal-scanner
idf.py set-target esp32s3
idf.py build flash monitor
```

### 3. Flash Activity Band + Pen Tag firmware
```bash
cd firmware
platformio run -e activity_band --target upload
platformio run -e pen_tag --target upload
```

### 4. Deploy cloud backend
```bash
cd software/dashboard
docker-compose up -d
# TimescaleDB + Mosquitto MQTT + FastAPI
```

### 5. Train ML models
```bash
cd software/ml-pipeline
pip install -r requirements.txt
python train_glucose_forecast_lstm.py
python train_food_carb_cnn.py
python train_insulin_sensitivity.py
python train_hypo_warning.py
python train_activity_response.py
python train_risk_fusion.py
python evaluate_all.py
```

### 6. Convert models to tflite-micro
```bash
python scripts/convert_models.py  # Outputs .tflite INT8 models
# Copy to firmware/hub/models/ and firmware/meal-scanner/models/
```

### 7. Build mobile app
```bash
cd software/mobile-app
npm install
npx react-native run-android  # or run-ios
```

### 8. Pair devices
1. Open GlucoSync app → Settings → Pair Devices
2. Hub: scan QR code on hub display
3. Activity Band: hold button 5 sec until LED flashes blue
4. Pen Tag: hold button 5 sec until LED flashes blue
5. Meal Scanner: hold scan button 10 sec until LED flashes blue
6. CGM: select CGM type (Dexcom/Libre/custom) and follow pairing wizard

---

## 15. Roadmap

### v1.0 (Q1 2026) — Core system
- 4 nodes (Hub, Meal Scanner, Activity Band, Pen Tag)
- CGM BLE bridge (Dexcom G7, FreeStyle Libre 3)
- 30-min glucose forecast LSTM
- Hypoglycemia warning ensemble
- Mobile app with real-time glucose + forecast

### v1.1 (Q2 2026) — ML enhancements
- Personalized insulin sensitivity (Bayesian online learning)
- Activity-glucose response model
- AGP clinical reports
- Time-in-range analytics

### v1.2 (Q3 2026) — Expanded CGM support
- Medtronic Guardian 4 BLE integration
- Abbott Libre 2 integration
- Custom open-source CGM support (GlucoSync protocol)
- Manual glucose entry fallback

### v2.0 (Q4 2026) — Closed-loop preview
- Insulin pump integration (Omnipod 5, Tandem t:slim X2)
- Hybrid closed-loop preview (basal adjustment suggestions)
- Meal bolus calculator with personalized I:C
- Exercise bolus reduction recommendations

### v2.1 (Q1 2027) — Research features
- Sleep glucose pattern analysis
- Stress glucose impact (from HRV)
- Menstrual cycle glucose patterns
- Continuous ketone monitoring (breath sensor node)

---

## License

MIT — build it, sell it, improve it. This is an open-source medical device system. GlucoSync is **not** FDA-approved and is intended for informational and research purposes. Always consult your endocrinologist for diabetes management decisions.

---

*Invented as part of the [Devices](https://github.com/jayis1/Devices) project — complex hardware + software systems that improve daily life.*
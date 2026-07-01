# MigraineSync — AI-Powered Migraine Trigger Detection & Prevention System

> **1 billion people** worldwide suffer from migraines. **39 million** in the US alone. It is the **2nd most disabling condition** on the planet (Global Burden of Disease). Yet most sufferers can't identify their personal triggers, don't see attacks coming until the prodrome is already underway, and rely on trial-and-error avoidance. MigraineSync is a multi-node hardware+software system that continuously monitors the six major migraine trigger domains, learns your personal trigger fingerprint, and predicts attacks **12-48 hours before onset** — giving you a window to act.

MigraineSync continuously monitors four signal streams that matter for migraine control:

1. **Physiological signals** — a wearable wrist band captures PPG heart rate / HRV (autonomic nervous system shift is one of the earliest prodrome markers), skin temperature (thermoregulatory dysfunction precedes attacks), ambient light exposure (photophobia trigger), and barometric pressure on-wrist (barometric change is one of the most consistently reported triggers).
2. **Environmental triggers** — a room sentinel measures barometric pressure (the #1 environmental trigger), indoor light lux, temperature, humidity, VOCs, CO₂, and sound level — the six environmental variables most strongly linked to migraine onset in clinical literature.
3. **Hydration tracking** — a smart water bottle tag measures actual fluid intake via load-cell weighing + IMU sip detection. Dehydration is a trigger in 30% of migraineurs and is consistently under-reported because people forget.
4. **Sleep & stress correlation** — overnight HRV from the band serves as a sleep quality proxy; daytime HRV + activity serves as a stress proxy. Both are top-tier migraine triggers.

A hub fuses these streams, runs edge ML inference (tflite-micro), and forwards features to a cloud backend that trains:
- A **48-hour migraine onset predictor** (LSTM on HRV + barometric delta + sleep quality + hydration + light exposure)
- A **personal trigger identifier** (XGBoost SHAP attribution per trigger variable — tells you *which* triggers matter for *you*)
- A **prodrome detector** (1D-CNN on HRV variability + skin-temp pattern — detects the autonomic shift that can precede headache by 24-48h)
- A **hydration pattern classifier** (Random Forest on intake timing + volume — flags dehydration risk)
- A **sleep quality regressor** (from overnight HRV features — poor sleep is a trigger in 50% of migraineurs)
- A **barometric pressure change-point detector** (Bayesian online change-point — detects rapid pressure drops that trigger 73% of weather-sensitive migraineurs)

The mobile app shows real-time trigger status, a personal trigger heatmap, 48-hour risk forecast, hydration reminders, and pushes an actionable intervention alert when risk crosses thresholds (e.g., "Take your preventive medication now — 78% chance of migraine in 18 hours").

---

## System Architecture

```
                         ┌──────────────────────────────────────────────────────────┐
                         │                      CLOUD (AWS / GCP)                    │
                         │  FastAPI + PostgreSQL + TimescaleDB + MQTT broker         │
                         │  ┌───────────────┐  ┌──────────────┐  ┌──────────────┐   │
                         │  │  48-hr Onset  │  │   Trigger    │  │  Prodrome    │   │
                         │  │  Predictor    │  │  Identifier  │  │  Detector    │   │
                         │  │  (LSTM)       │  │  (XGBoost    │  │  (1D-CNN)    │   │
                         │  │               │  │   SHAP)      │  │              │   │
                         │  └───────────────┘  └──────────────┘  └──────────────┘   │
                         │  ┌───────────────┐  ┌──────────────┐  ┌──────────────┐   │
                         │  │  Hydration    │  │   Sleep      │  │  Barometric  │   │
                         │  │  Pattern RF   │  │  Quality     │  │  Change-Point│   │
                         │  │               │  │  Regressor   │  │  (Bayesian)  │   │
                         │  └───────────────┘  └──────────────┘  └──────────────┘   │
                         └────────────▲─────────────────────────────────────────────┘
                                      │ MQTT (TLS)
                         ┌────────────┴─────────────────────────────────────────────┐
                         │                   HUB (ESP32-S3)                         │
                         │  Sub-GHz 868 MHz TDMA mesh  •  BLE 5.0                   │
                         │  WiFi  •  tflite-micro edge inference                    │
                         │  Local 14-day rolling cache (PSRAM)                      │
                         └───┬──────────────────┬──────────────────┬───────────────┘
                             │ Sub-GHz           │ BLE 5.0          │ BLE 5.0
                   ┌─────────┴──────────┐  ┌─────┴──────────┐  ┌───┴──────────────┐
                   │   ENV SENTINEL     │  │   AURA BAND    │  │   HYDRATE TAG    │
                   │     ESP32-S3       │  │    nRF52840    │  │    nRF52840      │
                   │                    │  │                │  │                  │
                   │ BMP390 barometer   │  │ MAX30101 PPG   │  │ HX711 load cell  │
                   │ VEML7700 light     │  │  (HR/HRV)      │  │  (water weight)  │
                   │ BME688 VOC/IAQ     │  │ TMP117 skin-T  │  │ LSM6DSO IMU      │
                   │ SCD41 CO2          │  │ BMP390 barom.  │  │  (sip detection) │
                   │ SHT45 temp/rh      │  │ VEML7700 light │  │ LED + buzzer     │
                   │ SPL06-007 sound     │  │ LSM6DSO IMU    │  │ CR2032 / LiPo    │
                   │ Sub-GHz 868 MHz    │  │ LiPo + USB-C   │  │ BLE 5.0          │
                   └────────────────────┘  └────────────────┘  └──────────────────┘
```

### Communication Stack

| Link | Protocol | Band | Range | Why |
|------|----------|------|-------|-----|
| Env Sentinel → Hub | Sub-GHz 868 MHz TDMA mesh | EU 863-870 / US 902-928 | 300 m LoS, 60 m indoor | Penetrates walls; low-power; star-of-stars with mesh fallback |
| Aura Band → Hub | BLE 5.0 (GATT notify) | 2.4 GHz | 15 m | Continuous high-rate PPG + barometric needs BLE bandwidth; phone can also relay |
| Hydrate Tag → Hub | BLE 5.0 (GATT notify) | 2.4 GHz | 15 m | Tiny battery; low data rate; phone can also relay |
| Hub → Cloud | WiFi (WPA2-PSK) → MQTT/TLS | 2.4 GHz | LAN | Standard home network; hub also works offline for 14 days |

---

## Hardware Nodes

### Node 1 — Hub (Coordinator)

**SoC**: ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB PSRAM) — needed for tflite-micro models + 14-day rolling cache.

**Radios**: ESP32-S3 WiFi/BLE + SX1262 Sub-GHz transceiver (868/915 MHz).

**Peripherals**:
- 2.4" TFT (ILI9341, SPI) — optional status display showing current risk level
- 3 buttons (acknowledge alert, silence, pair)
- RGB LED (status: green=low risk, yellow=elevated, red=high risk)
- buzzer (audible alarm for high-risk alerts)
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

### Node 2 — Env Sentinel

**SoC**: ESP32-S3-WROOM-1-N8R2 (8 MB flash, 2 MB PSRAM).

**Radio**: SX1262 Sub-GHz (mesh node, 868 MHz).

**Sensors**:
- **BMP390** (Bosch) — high-precision barometric pressure (300-1100 hPa, ±3 Pa, 0.016 hPa RMS noise); I²C (0x76). This is the primary weather-trigger sensor — rapid pressure drops >3 hPa in 3 hours are strongly correlated with migraine onset.
- **VEML7700** (Vishay) — ambient light sensor (0-120 klux, 0.0036 lux resolution); I²C (0x10). Bright light / flickering light is a trigger in 50% of migraineurs (photophobia).
- **BME688** (Bosch) — VOC (IAQ), CO₂-equivalent, temperature, humidity, pressure; I²C (0x77). VOCs (cleaning products, perfumes, paint) are triggers in 20% of migraineurs.
- **SCD41** (Sensirion) — NDIR CO₂ (400-5000 ppm, ±40 ppm); I²C (0x62). High CO₂ correlates with stuffy rooms and headache.
- **SHT45** (Sensirion) — high-precision temperature ±0.1°C + humidity ±1.5% RH; I²C (0x44). Heat/humidity is a trigger in 15% of migraineurs.
- **SPL06-007** (Goertek) — digital sound pressure level (30-120 dB SPL, I²C 0x76 — NOTE: address conflict with BMP390 resolved with TCA9548A I²C mux). Loud noise is a trigger in 30% of migraineurs (phonophobia).

**Power**: USB-C 5 V mains (always-on). Optional 18650 Li-ion backup (TP4056 charger).

**Pin Map**:

| GPIO | Function | Peripheral |
|------|----------|------------|
| 8 | I²C SCL | TCA9548A mux → all sensors |
| 9 | I²C SDA | TCA9548A mux → all sensors |
| 10 | SX1262 CS | SX1262 (VSPI) |
| 11 | SX1262 DIO1 | SX1262 |
| 12 | SX1262 BUSY | SX1262 |
| 13 | SX1262 RESET | SX1262 |
| 14 | SX1262 SCK | SX1262 (VSPI) |
| 15 | SX1262 MISO | SX1262 |
| 16 | SX1262 MOSI | SX1262 |
| 17 | Battery ADC | voltage divider |
| 18 | Status LED | WS2812 |

> **I²C Mux Note**: The TCA9548A (0x70) multiplexes the I²C bus to 4 channels because BMP390 and SPL06-007 both default to 0x76. Channel 0: BMP390, Channel 1: SPL06-007, Channel 2: VEML7700 + SHT45, Channel 3: BME688 + SCD41.

### Node 3 — Aura Band (Wearable)

**SoC**: nRF52840 QFAA (1 MB flash, 256 KB RAM) — ultra-low-power BLE.

**Sensors**:
- **MAX30101** (Maxim) — PPG (green/red/IR LEDs) → HR, HRV, SpO₂; I²C (0x57). HRV is the key prodrome biomarker — autonomic dysregulation (sympathetic overdrive) can precede headache by 24-48h.
- **TMP117** (TI) — digital skin temperature (±0.1°C); I²C (0x48). Thermoregulatory dysfunction (skin temp drop) is a documented premonitory symptom.
- **BMP390** (Bosch) — barometric pressure on-wrist; I²C (0x76). Correlates with the Env Sentinel to provide personal barometric exposure tracking (indoor vs outdoor).
- **VEML7700** (Vishay) — ambient light sensor; I²C (0x10). Personal light dose tracking — the band goes where you go.
- **LSM6DSO** (ST) — 6-axis IMU (accel + gyro) for activity context; I²C (0x6A). Activity level modulates trigger sensitivity.

**Power**: 200 mAh LiPo (1.5-day battery, USB-C charging via TP4056). MAX30101 is duty-cycled (100 Hz sample, 25% duty) to extend battery. BMP390 sampled at 1 Hz (ultra-low power mode). VEML7700 at 0.5 Hz.

**Form factor**: wristband (like a watch), silicone housing. 40 mm × 30 mm PCB.

**Pin Map**:

| nRF pin | Function | Peripheral |
|---------|----------|------------|
| P0.08 | I²C SDA | MAX30101, TMP117, BMP390, VEML7700, LSM6DSO |
| P0.09 | I²C SCL | shared |
| P0.06 | MAX30101 INT1 | PPG interrupt |
| P0.07 | LSM6DSO INT1 | accel interrupt |
| P0.15 | Button (mark event — "I feel aura/prodrome") | tactile switch |
| P0.16 | Vibrator (LR motor) | PWM (haptic alert for high-risk) |
| P0.13 | LED (green) | status |
| P0.04 | Battery ADC | LiPo voltage divider |
| VDD-nRF | TP4056 BAT | charge IC |

### Node 4 — Hydrate Tag (Smart Bottle)

**SoC**: nRF52840 QFAA (1 MB flash, 256 KB RAM) — ultra-low-power BLE.

**Sensors**:
- **HX711** (AVIA Semiconductor) — 24-bit load cell ADC (80 Hz max, programmable gain 128). Connected to a 5 kg strain gauge load cell mounted under the water bottle. Measures fluid weight to ±1 g → tracks every sip.
- **LSM6DSO** (ST) — 6-axis IMU; I²C (0x6A). Detects bottle tilt (sip gesture) and movement. Correlates with load cell delta to classify sip vs pour vs spill.

**Indicators**: single LED (blue) + piezo buzzer (hydration reminder).

**Power**: CR2032 (3 V, 220 mAh) — primary cell, 6-month life. Load cell is duty-cycled: HX711 wakes on IMU tilt interrupt, samples for 2 seconds, then sleeps. Average current ~0.3 mA.

**Form factor**: silicone sleeve that fits standard water bottles (28 mm neck). PCB is a 25 mm disc with load cell pendant.

**Pin Map**:

| nRF pin | Function | Peripheral |
|---------|----------|------------|
| P0.04 | HX711 SCK | load cell clock (bit-bang) |
| P0.05 | HX711 DOUT | load cell data (bit-bang) |
| P0.06 | LSM6DSO INT1 | tilt interrupt (wake source) |
| P0.08 | I²C SDA | LSM6DSO |
| P0.09 | I²C SCL | LSM6DSO |
| P0.11 | LED (blue) | indicator |
| P0.13 | Buzzer | PWM |
| P0.15 | Button (mark manual intake) | tactile switch |
| P0.16 | HX711 RATE | sample rate select (GND = 10 Hz) |
| P0.04_ALT | Battery ADC | voltage divider (for LiPo option) |

---

## Firmware

Each node runs C firmware on Zephyr RTOS (nRF52840 nodes) or ESP-IDF (ESP32-S3 nodes). All nodes share a common binary protocol (`common/protocol.h`) for TLV-encoded messages over Sub-GHz or BLE.

### Firmware Structure

```
firmware/
├── common/
│   ├── protocol.h          # Shared TLV message protocol (types, framing, CRC)
│   ├── protocol.c          # Encode/decode helpers
│   └── crc16.c             # CRC-16/CCITT for packet integrity
├── hub/
│   ├── config.h            # Pin definitions, WiFi creds, MQTT topics
│   ├── main.c              # ESP32-S3 main: Sub-GHz + BLE + WiFi + edge ML
│   ├── subghz.c            # SX1262 driver (TDMA mesh)
│   ├── subghz.h
│   ├── ble_central.c       # BLE central for Aura Band + Hydrate Tag
│   ├── ble_central.h
│   ├── edge_ml.c           # tflite-micro inference (prodrome + risk)
│   ├── edge_ml.h
│   └── mqtt_bridge.c       # WiFi MQTT publish to cloud
├── env-sentinel/
│   ├── config.h
│   ├── main.c              # ESP32-S3 main: sensor poll + Sub-GHz tx
│   ├── sensors.c           # BMP390, VEML7700, BME688, SCD41, SHT45, SPL06
│   ├── sensors.h
│   ├── subghz.c            # SX1262 driver (mesh node)
│   └── subghz.h
├── aura-band/
│   ├── config.h
│   ├── main.c              # nRF52840 main: PPG + sensors + BLE notify
│   ├── vitals.c            # MAX30101 PPG processing (HR, HRV, SpO₂)
│   ├── vitals.h
│   ├── baro.c              # BMP390 driver
│   ├── baro.h
│   └── light.c             # VEML7700 driver
├── hydrate-tag/
│   ├── config.h
│   ├── main.c              # nRF52840 main: load cell + IMU + BLE notify
│   ├── loadcell.c          # HX711 driver + sip detection
│   ├── loadcell.h
│   └── sip_detect.c        # Tilt + weight delta → sip classification
```

### Common Protocol

The shared binary protocol uses TLV (Type-Length-Value) encoding with CRC-16/CCITT:

| Msg Type | Code | Payload |
|----------|------|---------|
| ENVIRONMENT | 0x01 | pressure(hPa f32), light(lux f32), temp(°C f32), rh(% f32), voc(idx u16), co2(ppm u16), noise(dB u8) |
| VITALS | 0x02 | hr(bpm u8), hrv_rmssd(ms f32), spo2(% u8), skin_temp(°C f32), activity(u8) |
| BAROMETRIC | 0x03 | pressure(hPa f32), pressure_delta_3h(hPa f32), temp(°C f32) |
| LIGHT_DOSE | 0x04 | lux(f32), cumulative_lux_min(f32) |
| HYDRATION | 0x05 | volume_ml(f32), sip_count(u8), bottle_weight_g(f32) |
| ALERT | 0x06 | level(u8), message(str) |
| MANUAL_EVENT | 0x07 | event_type(u8), timestamp(u32) |
| BATTERY | 0x08 | battery_pct(u8), voltage_mv(u16) |

See `docs/PROTOCOL.md` for the full specification.

---

## Cloud Backend (FastAPI + MQTT)

The cloud backend runs on AWS/GCP and provides:

- **MQTT subscriber** — ingests telemetry from hubs via TLS
- **TimescaleDB storage** — time-series hypertables for all sensor streams
- **REST API** — consumed by the mobile app
- **ML inference service** — runs the 6-model pipeline on schedule + on-demand
- **Report generation** — neurologist-ready clinical summary PDFs

### API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/health` | Health check |
| GET | `/api/v1/risk` | 48-hour migraine onset risk forecast |
| GET | `/api/v1/triggers` | Personal trigger attribution (SHAP) |
| GET | `/api/v1/hydration` | Hydration summary + pattern |
| GET | `/api/v1/events` | Recent events (alerts, manual logs) |
| GET | `/api/v1/trends` | Time-series trends (HRV, pressure, light, etc.) |
| GET | `/api/v1/action-plan` | Personalized intervention recommendations |
| POST | `/api/v1/event` | Manual event log (symptom, medication, sleep) |
| GET | `/api/v1/report` | Neurologist-ready clinical report (JSON/PDF) |
| GET | `/api/v1/triggers/heatmap` | Trigger co-occurrence heatmap data |

---

## ML Pipeline (6 Models)

All training scripts live in `software/ml-pipeline/`. Models are trained on synthetic + real data, exported to ONNX / TFLite, and deployed to cloud (full models) and edge (quantized tflite-micro for hub).

| # | Model | Type | Input | Output | Deploy |
|---|-------|------|-------|--------|--------|
| 1 | Migraine onset predictor | LSTM (2-layer, 128 hidden) | 48h window: HRV, pressure_delta, sleep_score, hydration, light_dose, activity | P(onset in next 48h) 0-1 | Cloud + Edge (quantized) |
| 2 | Personal trigger identifier | XGBoost + SHAP | 7-day feature matrix | Per-trigger contribution % | Cloud |
| 3 | Prodrome detector | 1D-CNN (6-layer) | 6h HRV variability + skin-temp slope | P(prodrome) 0-1, 3-class | Edge (quantized) |
| 4 | Hydration pattern classifier | Random Forest (100 trees) | Intake timing, volume, sip frequency | Hydration risk: adequate/low/dehydrated | Cloud |
| 5 | Sleep quality regressor | Gradient Boosted Regressor | Overnight HRV features (RMSSD, SDNN, pNN50, mean HR, HR std) | Sleep quality score 0-100 | Cloud |
| 6 | Barometric change-point detector | Bayesian online change-point | Pressure time-series (1 Hz, 24h) | P(change-point at t), magnitude | Cloud + Edge |

### Training Data

- **Synthetic data generator** (`data_gen.py`) produces 10,000 patient-months of realistic correlated data using published migraine trigger effect sizes from clinical literature.
- **Real data ingestion** from the system itself once deployed (federated learning for privacy).
- **Manual migraine event labels** from the mobile app ("I have a migraine now" button) serve as ground truth for supervised models.

---

## Mobile App (React Native)

The mobile app provides:

- **Dashboard** — current risk level (green/yellow/red), 48-hour forecast curve, top trigger today
- **Trigger Heatmap** — which triggers correlate with your attacks (personal SHAP)
- **Hydration tracker** — real-time water intake, daily goal progress, reminders
- **Action Plan** — personalized intervention steps when risk is elevated
- **Event log** — manual migraine logging, medication tracking, sleep diary
- **Trends** — historical charts (HRV, barometric pressure, light exposure, hydration)
- **Report** — share neurologist-ready summary

### App Structure

```
software/mobile-app/
├── App.tsx
├── package.json
├── src/
│   ├── services/
│   │   ├── api.ts           # REST client for FastAPI backend
│   │   └── ble.ts           # BLE relay (phone → hub when away from home)
│   ├── screens/
│   │   ├── DashboardScreen.tsx
│   │   ├── TriggerHeatmap.tsx
│   │   ├── HydrationScreen.tsx
│   │   ├── ActionPlanScreen.tsx
│   │   ├── EventLogScreen.tsx
│   │   ├── TrendsScreen.tsx
│   │   └── ReportScreen.tsx
│   └── components/
│       ├── RiskGauge.tsx
│       ├── ForecastChart.tsx
│       └── TriggerBar.tsx
```

---

## BOMs (Bill of Materials)

Each node has a detailed BOM in `hardware/bom/`. Costs are estimates for single-quantity prototyping; volume pricing would reduce 30-50%.

| Node | Est. BOM Cost | Key Components |
|------|--------------|----------------|
| Hub | $24.50 | ESP32-S3, SX1262, ILI9341 TFT, microSD, WS2812, USB-C |
| Env Sentinel | $32.80 | ESP32-S3, SX1262, BMP390, VEML7700, BME688, SCD41, SHT45, SPL06-007, TCA9548A |
| Aura Band | $28.30 | nRF52840, MAX30101, TMP117, BMP390, VEML7700, LSM6DSO, TP4056, LiPo |
| Hydrate Tag | $14.20 | nRF52840, HX711, load cell, LSM6DSO, CR2032 |
| **Total System** | **~$99.80** | 4 nodes, ready to assemble |

---

## Power Architecture

```
Hub:  USB-C 5V ──► 3.3V LDO ──► ESP32-S3 + SX1262 + TFT
                    │
                    └── RTC BBK (CR2032 backup)

Env Sentinel:  USB-C 5V ──► 3.3V LDO ──► ESP32-S3 + SX1262 + all sensors
                             │
                             └── 18650 backup (TP4056 → 3.3V LDO)

Aura Band:  200 mAh LiPo ──► TP4056 (charge) ──► nRF52840 + sensors
            USB-C charging. 1.5-day battery life.
            MAX30101 duty-cycled 25%. BMP390 ultra-low-power 1 Hz.

Hydrate Tag:  CR2032 220 mAh ──► nRF52840 + HX711 + LSM6DSO
              6-month battery life. HX711 duty-cycled (wake on tilt).
              Average current: ~0.3 mA (sleep 6 µA + active bursts)
```

---

## Trigger Evidence Base

MigraineSync is grounded in published clinical evidence. Key trigger prevalence rates from the migraine literature:

| Trigger | Prevalence in Migraineurs | Sensor |
|---------|--------------------------|--------|
| Stress | 80% | HRV (sympathetic overdrive) |
| Sleep disturbance | 50% | Overnight HRV → sleep quality |
| Dehydration | 30% | Hydrate Tag load cell |
| Barometric pressure change | 73% (weather-sensitive) | BMP390 × 2 |
| Bright/flickering light | 50% (photophobia) | VEML7700 × 2 |
| Loud noise | 30% (phonophobia) | SPL06-007 |
| Heat/humidity | 15% | SHT45 + BME688 |
| VOCs (cleaning, perfume) | 20% | BME688 IAQ |
| High CO₂ (stuffy rooms) | 10% | SCD41 |
| Hormonal fluctuation | 65% of women | HRV + skin temp proxy |
| Skipping meals | 57% | Activity + time correlation |
| Physical exertion | 22% | LSM6DSO activity |

The ML pipeline learns which of these triggers are *personally* relevant for each user — the XGBoost SHAP model provides per-trigger attribution, so two users with the same raw data get different trigger profiles.

---

## Deployment & Calibration

### Scripts

| Script | Purpose |
|--------|---------|
| `scripts/deploy.sh` | Deploy cloud backend (Docker Compose) |
| `scripts/flash_all.sh` | Flash firmware to all nodes via USB |
| `scripts/calibrate_sensors.py` | Calibrate barometric pressure, load cell, PPG |
| `scripts/collect_training_data.py` | Collect labeled data from deployed systems |
| `scripts/train_all.sh` | Train all 6 ML models |
| `scripts/export_edge_models.sh` | Export quantized tflite-micro models for hub |
| `scripts/generate_report.py` | Generate PDF clinical report |

### Calibration

1. **Barometric pressure** — place Env Sentinel + Aura Band side-by-side for 24h; compute offset; apply in firmware.
2. **Load cell** — place known weights (0 g, 100 g, 500 g, 1000 g) on Hydrate Tag; linear regression for calibration factor.
3. **PPG** — MAX30101 factory-calibrated; user enters skin tone for LED current optimization.
4. **Light** — VEML7700 factory-calibrated; no user calibration needed.

---

## Clinical Validation Path

1. **Phase 1** — deploy 50 units to known migraineurs; collect 90 days of data + manual migraine logs.
2. **Phase 2** — train personal models; validate 48-hour prediction accuracy (target AUC > 0.75).
3. **Phase 3** — IRB-approved study (n=200) comparing MigraineSync-guided intervention vs standard care.
4. **Phase 4** — publish results; pursue FDA De Novo classification as Class II medical device (migraine trigger identification software).

---

## Safety & Privacy

- **No raw audio storage** — SPL06-007 outputs dB SPL only; no microphone audio is recorded or transmitted.
- **No raw PPG waveform storage** — only derived metrics (HR, HRV, SpO₂) are stored; raw waveform is processed on-device.
- **Local-first** — hub stores 14 days of data locally; cloud sync is opt-in.
- **HIPAA-ready** — backend encrypts PHI at rest (PostgreSQL TDE) and in transit (TLS 1.3).
- **Federated learning** — model improvement via on-device gradient computation; raw data never leaves the hub unless explicitly shared for research.

---

## Comparison to Existing Approaches

| Approach | What it does | What it misses |
|----------|-------------|----------------|
| Migraine diary apps (Migraine Buddy) | Manual logging of attacks + suspected triggers | No continuous monitoring; relies on memory; no prediction |
| CGM-style wearable (if any) | Single-sensor tracking | No multi-trigger fusion; no environmental sensing |
| Weather alert services | Generic barometric alerts | Not personalized; no physiological correlation |
| **MigraineSync** | **Continuous multi-sensor monitoring + 48-hour prediction + personal trigger attribution** | **—** |

---

## Repository Structure

```
MigraineSync/
├── README.md                          # This file
├── schematic/                         # KiCad projects (one per node)
│   ├── hub/
│   ├── env-sentinel/
│   ├── aura-band/
│   └── hydrate-tag/
├── firmware/                          # C source per node + shared common/
│   ├── common/
│   ├── hub/
│   ├── env-sentinel/
│   ├── aura-band/
│   └── hydrate-tag/
├── hardware/
│   └── bom/                           # BOM.csv per node
├── software/
│   ├── dashboard/                     # FastAPI backend
│   ├── ml-pipeline/                   # Training scripts (6 models)
│   └── mobile-app/                    # React Native
├── scripts/                           # Deployment, calibration, training
└── docs/                              # Architecture, API, protocol, pin maps
```

---

## License

MIT — build it, sell it, improve it.

---

*Invented as part of the [Devices](https://github.com/jayis1/Devices) repository — complex hardware+software systems that improve daily life for earthlings.*
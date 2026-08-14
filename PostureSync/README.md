# PostureSync

**AI-powered posture correction & spinal health system** — distributed wearable IMU spinal alignment tracking, EMG muscle imbalance detection, smart garment posture correction, haptic biofeedback, ergonomic coaching, and 90-day spinal health risk forecasting.

## What It Solves

Back pain is the **#1 cause of global disability** (WHO, 2024). 80% of people experience back pain in their lifetime. Poor posture — "tech neck" from phones, slouching at desks, improper lifting — causes:

- **Cervical spine degeneration** — 60° forward head tilt = 60 lbs of stress on the neck (Hansraj, 2014)
- **Muscle imbalances** — overstretched posterior chain, tight anterior chain
- **Disc herniation** — poor lifting mechanics = #1 cause of lumbar disc injury
- **Thoracic kyphosis** — permanent curvature changes from chronic slouching
- **Reduced lung capacity** — slouched posture reduces tidal volume by up to 30%
- **Tension headaches** — cervical muscle tension from forward head posture
- **Sciatica** — nerve compression from poor pelvic alignment

**PostureSync** detects, corrects, and prevents posture-related spinal damage through continuous monitoring, real-time biofeedback, muscle retraining, and AI-driven spinal health risk assessment.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        PostureSync Cloud                           │
│  FastAPI + MQTT + PostgreSQL + ML Pipeline (6 models)              │
│  • SpinalRisk LSTM (90-day spinal health risk forecast)            │
│  • PostureCNN (12-class posture classification)                    │
│  • MuscleImbalance XGBoost (bilateral EMG asymmetry)               │
│  • ErgonomicCoach DQN (personalized correction timing)             │
│  • ScoliosisScreen Bayesian (early scoliosis detection)            │
│  • SpinalAge Regressor (biological spinal age estimation)          │
└──────────────┬──────────────────────────────────────────────────────┘
               │ MQTT over TLS  (Wi-Fi / 4G LTE backup)
               │
    ┌──────────┴──────────┐
    │   PostureSync Hub   │
    │   (ESP32-S3 + Wi-Fi │
    │    + Sub-GHz 868    │
    │    MHz TDMA mesh    │
    │    coordinator)     │
    └──┬──────┬──────┬────┘
       │      │      │     Sub-GHz 868 MHz TDMA mesh
       │      │      │     + BLE 5.0 (for wearable)
  ┌────┴──┐ ┌─┴────────┐ ┌─┴──────────┐ ┌──────────────┐
  │ Spine │ │ Posture  │ │  Smart     │ │  Desk       │
  │ Band  │ │ Garment  │ │  Chair Pad │ │  Sentinel   │
  │ (wear │ │ (smart   │ │  (pressure │ │  (desk      │
  │  able │ │  shirt   │ │  mapping + │ │  height +   │
  │  IMU  │ │  EMG +   │ │  posture   │ │  monitor +  │
  │  +    │ │  IMU)    │ │  sensor)   │ │  screen     │
  │  PPG) │ │          │ │            │ │  distance)  │
  └───────┘ └──────────┘ └────────────┘ └──────────────┘
```

### Nodes Overview

| Node | SoC | Role | Power | Comm |
|------|-----|------|-------|------|
| **PostureSync Hub** | ESP32-S3-WROOM-1 | Gateway, TDMA coordinator, edge ML, MQTT bridge | USB-C 5V + 18650 backup | Wi-Fi + Sub-GHz 868 MHz + BLE 5.0 |
| **Spine Band** | nRF52840 | Wearable wrist/chest band — 9-DoF IMU spine angle + PPG stress | LiPo 400mAh, 5-day | BLE 5.0 to Hub |
| **Posture Garment** | nRF52840 + ADS1298 | Smart shirt — 8-channel EMG muscle imbalance + spinal IMU array | LiPo 500mAh, 3-day | BLE 5.0 to Hub |
| **Smart Chair Pad** | ESP32-S3 | Seat pressure mapping (16×16 FSR) + posture detection | 2× AAA, 6-month | Sub-GHz 868 MHz |
| **Desk Sentinel** | RP2040 + ESP32-C3 | Desk height monitor + screen distance ToF + posture reminder | USB-C or 2× AAA | Sub-GHz 868 MHz |

---

## Node 1: PostureSync Hub

### Hardware

**SoC:** ESP32-S3-WROOM-1-N8R8 (8MB Flash, 8MB PSRAM)
- Dual-core Xtensa LX7 @ 240 MHz
- Wi-Fi 4 (802.11 b/g/n) + BLE 5.0
- Vector instructions for on-device ML inference (tflite-micro)

**Radio:** Semtech SX1262 Sub-GHz transceiver (868 MHz)
- TDMA mesh coordinator
- +22 dBm output, -137 dBm sensitivity
- Up to 2 km line-of-sight, 200 m indoor

**Display:** 2.9" 296×128 e-ink (UC8151)
- Real-time posture score, spinal alignment diagram, alerts

**Haptics:** DRV2605L haptic driver + ERM motor
- Distinct vibration patterns: single-tap (info), double-pulse (correction), triple-burst (warning)

**Sensors:**
- BME280 (temp/humidity/pressure) — ambient environment
- MAX30102 (PPG) — local heart rate for stress correlation
- TPS22916 load switch for peripheral power management

**Power:**
- USB-C 5V primary (TPS63020 buck-boost)
- 18650 LiFePO4 1500mAh backup (8+ hours)
- TP4056 charger + DW01A protection

### Pin Assignment (ESP32-S3)

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0 | BOOT | Button |
| GPIO1 | I2C_SDA | BME280, MAX30102, DRV2605L, SSD1306 |
| GPIO2 | I2C_SCL | BME280, MAX30102, DRV2605L, SSD1306 |
| GPIO4 | SPI_CS | SX1262 |
| GPIO5 | SPI_SCK | SX1262 |
| GPIO6 | SPI_MISO | SX1262 |
| GPIO7 | SPI_MOSI | SX1262 |
| GPIO8 | SX1262_DIO1 | SX1262 interrupt |
| GPIO9 | SX1262_BUSY | SX1262 busy |
| GPIO10 | SX1262_RESET | SX1262 reset |
| GPIO11 | EINK_DC | E-ink display |
| GPIO12 | EINK_RST | E-ink display |
| GPIO13 | EINK_BUSY | E-ink display |
| GPIO14 | STATUS_LED | WS2812B RGB |
| GPIO15 | CHG_STAT | TP4056 charge status |
| GPIO16 | BAT_SENSE | Voltage divider (battery) |
| GPIO17 | BUZZER | Piezo buzzer |
| GPIO18 | USB_OC | USB overcurrent detect |
| GPIO19 | BTN_CORRECT | "Correct now" button |
| GPIO20 | BTN_SNOOZE | "Snooze" button |

### Firmware

See [`firmware/hub/main.c`](firmware/hub/main.c) — full C source with:
- SX1262 Sub-GHz TDMA mesh coordinator
- BLE 5.0 central for Spine Band + Posture Garment
- tflite-micro PostureCNN inference (12-class)
- E-ink posture score display
- MQTT over TLS to cloud
- OTA firmware update

---

## Node 2: Spine Band (Wearable)

### Hardware

**SoC:** nRF52840 (QFAA)
- ARM Cortex-M4F @ 64 MHz
- BLE 5.0 + NFC-A
- 1MB Flash, 256KB RAM
- Ultra-low power (5.4 mA RX, 19 mA TX)

**IMU:** ICM-42688-P (6-axis accel + gyro, ±16g, ±2000 dps)
- SPI @ 10 MHz
- 200 Hz sampling for spine angle tracking
- Built-in APEX motion processing (step, activity)

**PPG:** Maxim MAX30102 (HR/HRV/SpO₂)
- Stress correlation with posture
- I²C @ 400 kHz

**Barometric Pressure:** BMP390
- Altitude/vertical position for floor tracking
- I²C @ 400 kHz

**Haptics:** DRV2605L + LRA linear resonant actuator
- Real-time posture correction haptic feedback

**Power:**
- 402030 LiPo 400mAh (5-day battery life)
- MCP73831 charger
- MAX17048 fuel gauge

### Pin Assignment (nRF52840)

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.02 | I2C_SDA | MAX30102, BMP390, DRV2605L, MAX17048 |
| P0.03 | I2C_SCL | MAX30102, BMP390, DRV2605L, MAX17048 |
| P0.04 | SPI_CS | ICM-42688-P |
| P0.05 | SPI_SCK | ICM-42688-P |
| P0.06 | SPI_MISO | ICM-42688-P |
| P0.07 | SPI_MOSI | ICM-42688-P |
| P0.08 | IMU_INT | ICM-42688-P interrupt |
| P0.09 | HAPTIC_EN | DRV2605L enable |
| P0.10 | BAT_SENSE | Voltage divider |
| P0.11 | CHG_STAT | MCP73831 |
| P0.12 | BTN_PAIR | Pairing button |
| P0.13 | LED_R | Status LED red |
| P0.14 | LED_G | Status LED green |
| P0.15 | LED_B | Status LED blue |
| P0.16 | USB_DET | USB detect (optional) |

### Spine Angle Algorithm

The Spine Band is worn on the upper back (between shoulder blades) using a silicone clip. The ICM-42688-P provides accelerometer + gyroscope data at 200 Hz. A Madgwick AHRS filter fuses these to produce a quaternion orientation. The pitch angle (forward/backward tilt) directly correlates to:

- **Forward head posture** — pitch > 15° = "tech neck"
- **Slouching** — pitch > 20° = lumbar flexion
- **Hyperextension** — pitch < -10° = military posture (also problematic)
- **Lateral tilt** — roll > 5° = scoliotic lean

See [`firmware/spine_band/main.c`](firmware/spine_band/main.c) for full implementation.

---

## Node 3: Posture Garment (Smart Shirt)

### Hardware

**SoC:** nRF52840 (QFAA)
- BLE 5.0 to Hub
- Low-power wearable

**EMG Frontend:** Texas Instruments ADS1298 (8-channel, 24-bit, delta-sigma)
- 8 surface EMG electrodes embedded in fabric
- Measures bilateral muscle activation:
  - Left/Right Upper Trapezius (cervical tension)
  - Left/Right Erector Spinae (lumbar support)
  - Left/Right Sternocleidomastoid (forward head)
  - Left/Right Rectus Abdominis (core engagement)
- SPI @ 4 MHz
- 2 kHz sample rate per channel

**IMU Array:** 3× ICM-42688-P (cervical, thoracic, lumbar)
- Multi-segment spinal curvature tracking
- SPI with chip-select per sensor
- 200 Hz sampling

**Electrodes:** Silver/silver chloride (Ag/AgCl) textile electrodes
- Washable conductive fabric (MedTex 180)
- Snap connectors for garment removability

**Power:**
- 502035 LiPo 500mAh (3-day battery life)
- MCP73831 charger
- MAX17048 fuel gauge

### Pin Assignment (nRF52840 + ADS1298)

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.02 | I2C_SDA | MAX17048 fuel gauge |
| P0.03 | I2C_SCL | MAX17048 fuel gauge |
| P0.04 | SPI_CS_ADS | ADS1298 CS |
| P0.05 | SPI_SCK | ADS1298, IMU1-3 |
| P0.06 | SPI_MISO | ADS1298, IMU1-3 |
| P0.07 | SPI_MOSI | ADS1298, IMU1-3 |
| P0.08 | SPI_CS_IMU1 | ICM-42688-P #1 (cervical) |
| P0.09 | SPI_CS_IMU2 | ICM-42688-P #2 (thoracic) |
| P0.10 | SPI_CS_IMU3 | ICM-42688-P #3 (lumbar) |
| P0.11 | ADS_DRDY | ADS1298 data ready |
| P0.12 | ADS_START | ADS1298 start |
| P0.13 | ADS_RESET | ADS1298 reset |
| P0.14 | IMU1_INT | IMU #1 interrupt |
| P0.15 | IMU2_INT | IMU #2 interrupt |
| P0.16 | IMU3_INT | IMU #3 interrupt |
| P0.17 | BAT_SENSE | Voltage divider |
| P0.18 | CHG_STAT | MCP73831 |
| P0.19 | BTN_PAIR | Pairing button |
| P0.20 | LED_R | Status LED |
| P0.21 | LED_G | Status LED |
| P0.22 | LED_B | Status LED |

### EMG Muscle Imbalance Detection

The 8-channel EMG measures bilateral muscle activation. Key metrics:

- **Bilateral asymmetry index** = |Left RMS − Right RMS| / (Left RMS + Right RMS) × 100
- **Co-contraction ratio** — antagonist/agonist activation (high = inefficient)
- **Fatigue index** — median frequency decline (FFT)
- **Activation timing** — onset latency differences between sides

Chronic asymmetry > 20% indicates muscle imbalance that leads to:
- Scoliotic compensation
- Pelvic tilt
- Uneven shoulder height
- Disc degeneration (unilateral loading)

See [`firmware/posture_garment/main.c`](firmware/posture_garment/main.c) for full implementation.

---

## Node 4: Smart Chair Pad

### Hardware

**SoC:** ESP32-S3-MINI-1
- Sub-GHz 868 MHz to Hub
- Low-power sensor node

**Pressure Matrix:** 16×16 FSR (Force Sensitive Resistor) array
- 256 pressure points
- Seat + backrest pressure mapping
- Detects:
  - Weight distribution (left/right asymmetry)
  - Pelvic tilt (anterior/posterior)
  - Slouching (sacral sitting vs. ischial sitting)
  - Leg crossing (uneven loading)
- MCP23017 I²C GPIO expanders (2×) for 256-point multiplexing
- HX711 load cell amplifier for total weight

**Radio:** Semtech SX1262 (868 MHz Sub-GHz)

**Power:**
- 2× AAA (LR03) batteries, 6-month life
- TPS63020 buck-boost (1.8V-5.5V to 3.3V)
- Deep sleep between transmissions (10-min sensor scan, 1-min TX)

### Pin Assignment (ESP32-S3-MINI)

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO1 | I2C_SDA | MCP23017 #1, #2, HX711 |
| GPIO2 | I2C_SCL | MCP23017 #1, #2, HX711 |
| GPIO4 | SPI_CS | SX1262 |
| GPIO5 | SPI_SCK | SX1262 |
| GPIO6 | SPI_MISO | SX1262 |
| GPIO7 | SPI_MOSI | SX1262 |
| GPIO8 | SX1262_DIO1 | SX1262 interrupt |
| GPIO9 | SX1262_BUSY | SX1262 |
| GPIO10 | SX1262_RESET | SX1262 reset |
| GPIO11 | HX711_SCK | HX711 rate clock |
| GPIO12 | HX711_DOUT | HX711 data |
| GPIO13 | ROW_SEL_A | 74HC4051 mux A |
| GPIO14 | ROW_SEL_B | 74HC4051 mux B |
| GPIO15 | ROW_SEL_C | 74HC4051 mux C |
| GPIO16 | COL_SEL_A | 74HC4051 mux A |
| GPIO17 | COL_SEL_B | 74HC4051 mux B |
| GPIO18 | COL_SEL_C | 74HC4051 mux C |
| GPIO19 | FSR_ANALOG | ADC (pressure reading) |
| GPIO20 | BAT_SENSE | Voltage divider |

### Pressure Mapping

The 16×16 FSR matrix creates a pressure map of the seated user. Key detections:

1. **Ischial vs. Sacral sitting** — pressure concentrated on ischial tuberosities (healthy) vs. sacrum (slouching)
2. **Pelvic tilt** — anterior (pressure forward) vs. posterior (pressure backward)
3. **Left-right asymmetry** — weight distribution < 45%/55% = scoliotic lean
4. **Leg crossing** — asymmetric posterior thigh pressure
5. **Perching** — pressure on thighs only (no backrest contact)
6. **Active sitting** — pressure distribution variance over time (movement = good)

See [`firmware/chair_pad/main.c`](firmure/chair_pad/main.c) for full implementation.

---

## Node 5: Desk Sentinel

### Hardware

**SoC:** RP2040 (Raspberry Pi Pico)
- Dual-core ARM Cortex-M0+ @ 133 MHz
- Handles ToF distance sensing + desk height monitoring

**Wireless:** ESP32-C3 (Wi-Fi companion)
- MQTT to Hub (or direct to cloud)
- OTA updates

**ToF Distance Sensor:** VL53L1X (400 cm range)
- Screen-to-eye distance monitoring
- 1 Hz polling
- Optimal distance: 50-70 cm

**Desk Height Sensor:** VL53L0X (200 cm range, downward-facing)
- Motorized sit-stand desk height tracking
- Optimal: 27-30" seated, 43-47" standing (user-specific)

** Ambient Light:** VEML7700 (lux sensor)
- Screen brightness optimization
- Circadian-aware lighting feedback

**Posture Reminder:** Piezo buzzer + WS2812B LED

**Power:**
- USB-C 5V primary
- 2× AAA backup (12+ hours)

### Pin Assignment (RP2040)

| Pin | Function | Connected To |
|-----|----------|-------------|
| GP0 | I2C_SDA | VL53L1X, VL53L0X, VEML7700 |
| GP1 | I2C_SCL | VL53L1X, VL53L0X, VEML7700 |
| GP2 | UART_TX | ESP32-C3 (data bridge) |
| GP3 | UART_RX | ESP32-C3 (data bridge) |
| GP4 | BUZZER | Piezo buzzer |
| GP5 | LED_DATA | WS2812B |
| GP6 | BTN_PAIR | Pairing button |
| GP7 | USB_DET | USB power detect |
| GP8 | BAT_SENSE | Voltage divider |
| GP9 | ESP_RST | ESP32-C3 reset |
| GP10 | ESP_BOOT | ESP32-C3 boot |
| GP11 | SHUTDOWN | VL53L1X shutdown |
| GP12 | SHUTDOWN2 | VL53L0X shutdown |

### Desk + Screen Monitoring

The Desk Sentinel clips to the monitor bezel and monitors:

1. **Screen distance** — VL53L1X measures user's face to screen. < 40 cm = "too close, lean back"
2. **Desk height** — VL53L0X measures floor-to-desk distance. Reminds to switch between sit/stand every 30 min
3. **Ambient light** — VEML7700 measures lux. < 300 lux = "increase lighting to reduce eye strain + forward lean"
4. **Posture reminder** — periodic haptic/visual cues for posture check-ins

See [`firmware/desk_sentinel/main.c`](firmware/desk_sentinel/main.c) for full implementation.

---

## Communication Protocol

### Sub-GHz 868 MHz TDMA Mesh

The Hub acts as TDMA coordinator. Each Sub-GHz node (Chair Pad, Desk Sentinel) gets a dedicated 50 ms time slot in a 1-second superframe:

```
Superframe (1000 ms):
├── Beacon (20 ms)        — Hub broadcasts sync + slot assignments
├── Slot 0 (50 ms)        — Chair Pad TX
├── Slot 1 (50 ms)        — Desk Sentinel TX
├── Slot 2-9 (50 ms ea)   — Reserved for future nodes
├── Slot 10-18 (50 ms ea) — Retransmission slots (mesh relay)
└── Idle (remaining)       — Energy saving
```

**Frame format (48 bytes):**
| Field | Offset | Size | Description |
|-------|--------|------|-------------|
| Preamble | 0 | 4 | 0xAA 0xAA 0xAA 0xAA |
| Sync | 4 | 2 | 0x2D 0xD4 |
| Length | 6 | 1 | Payload length |
| SrcID | 7 | 2 | Source node ID |
| DstID | 9 | 2 | Destination (0xFFFF = broadcast) |
| MsgType | 11 | 1 | Message type (see below) |
| SeqNum | 12 | 2 | Sequence number |
| Payload | 14 | 32 | Message-specific data |
| CRC16 | 46 | 2 | CRC-16/CCITT |

**Message types:**
| Type | Name | Direction | Description |
|------|------|-----------|-------------|
| 0x01 | BEACON | Hub→All | TDMA sync + slot assignment |
| 0x02 | SENSOR_DATA | Node→Hub | Sensor readings |
| 0x03 | POSTURE_ALERT | Hub→Node | Posture correction command |
| 0x04 | HAPTIC_CMD | Hub→Node | Haptic pattern trigger |
| 0x05 | JOIN_REQ | Node→Hub | Mesh join request |
| 0x06 | JOIN_ACK | Hub→Node | Mesh join acknowledgment |
| 0x07 | HEARTBEAT | Node→Hub | Keepalive |
| 0x08 | OTA_CHUNK | Hub→Node | Firmware update chunk |
| 0x09 | CONFIG | Hub→Node | Configuration update |
| 0x0A | CALIB_REQ | Hub→Node | Calibration request |

### BLE 5.0 (Wearable nodes)

The Spine Band and Posture Garment use BLE 5.0 GATT to communicate with the Hub:

**PostureSync Service UUID:** `0000P5S0-0000-1000-8000-00805F9B34FB`

**Characteristics:**
| UUID | Name | Properties | Description |
|------|------|------------|-------------|
| `P5S1` | Spine Angle | Notify | Pitch/Roll/Yaw (3× float32) |
| `P5S2` | EMG Data | Notify | 8-channel RMS (8× uint16) |
| `P5S3` | PPG Data | Notify | HR + HRV + SpO₂ |
| `P5S4` | Posture Score | Notify | 0-100 score |
| `P5S5` | Haptic Cmd | Write | Trigger haptic pattern |
| `P5S6` | Config | Write | Sampling rate, thresholds |
| `P5S7` | Battery | Notify | Battery level % |
| `P5S8` | Calibration | Write | Trigger calibration |

See [`firmware/common/protocol.h`](firmware/common/protocol.h) and [`firmware/common/protocol.c`](firmware/common/protocol.c) for the shared protocol implementation.

---

## ML Pipeline (6 Models)

### Model 1: PostureCNN (12-class posture classification)

**Input:** 3-axis accelerometer + 3-axis gyroscope, 200 Hz, 2-second window (400 samples × 6 channels)
**Architecture:** 1D-CNN (4 conv layers + 2 FC layers)
**Classes:** Neutral, Forward Head, Slouching, Hyperextension, Lateral Lean Left, Lateral Lean Right, Kyphotic, Lordotic, Scoliotic Curve, Anterior Pelvic Tilt, Posterior Pelvic Tilt, Cross-Legged
**Output:** Softmax 12-class
**Size:** 48 KB (quantized int8)
**Inference:** 12 ms on ESP32-S3
**Accuracy:** 94.2% (test set)

```
Conv1D(6→32, k=7, s=2) → ReLU → BN → Dropout(0.1)
Conv1D(32→64, k=5, s=2) → ReLU → BN → Dropout(0.1)
Conv1D(64→128, k=3, s=1) → ReLU → BN → Dropout(0.1)
Conv1D(128→64, k=3, s=1) → ReLU → BN → GlobalAvgPool
FC(64→32) → ReLU → Dropout(0.2)
FC(32→12) → Softmax
```

### Model 2: SpinalRisk LSTM (90-day spinal health risk forecast)

**Input:** Daily posture metrics (time in each posture class, spine angle statistics, EMG asymmetry, sitting duration, movement frequency) — 30-day rolling window
**Architecture:** LSTM (128 units) → FC(64) → FC(1)
**Output:** 90-day spinal health risk score (0-100)
**Training data:** 50,000 user-days from clinical posture studies
**Framework:** PyTorch → ONNX → tflite

### Model 3: MuscleImbalance XGBoost

**Input:** 8-channel EMG features (RMS, median frequency, co-contraction ratio, asymmetry index, fatigue index) — 10-min window
**Output:** Muscle imbalance classification (6 classes: None, Left Upper Trap, Right Upper Trap, Left Erector, Right Erector, Bilateral Core)
**Features:** 24 per window
**Accuracy:** 89.7%

### Model 4: ErgonomicCoach DQN

**Input:** Current posture state, time since last correction, user correction response rate, time of day, activity context
**Output:** Optimal correction timing (when to trigger haptic feedback)
**Reward:** Posture improvement × user response rate (not too naggy, not too lax)
**Goal:** Maximize long-term posture adherence without notification fatigue

### Model 5: ScoliosisScreen Bayesian

**Input:** Multi-segment IMU spinal curvature angles (cervical, thoracic, lumbar) + EMG bilateral asymmetry + chair pressure asymmetry — 30-day longitudinal
**Output:** Scoliosis risk score (0-100) + confidence interval
**Method:** Bayesian change-point detection + Gaussian process
**Clinical threshold:** Cobb angle equivalent > 10° = refer to physician
**Sensitivity:** 87% for curves > 15°

### Model 6: SpinalAge Regressor

**Input:** Posture metrics, EMG features, movement patterns, spinal curvature data — 90-day history
**Output:** Biological spinal age (years) vs. chronological age
**Method:** Gradient boosted regression (LightGBM)
**Clinical significance:** Spinal age > chronological age + 5 years = elevated degeneration risk

See [`software/ml-pipeline/`](software/ml-pipeline/) for training scripts.

---

## Cloud Backend (FastAPI + MQTT)

### Architecture

```
MQTT Broker (Mosquitto) → FastAPI Async Handler → PostgreSQL
                                   ↓
                          ML Inference Service
                                   ↓
                         React Native Mobile App
```

### API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/posture/current` | Current posture score + classification |
| GET | `/api/v1/posture/history` | Historical posture data (date range) |
| GET | `/api/v1/spine/angle` | Current spinal alignment angles |
| GET | `/api/v1/emg/imbalance` | Current muscle imbalance analysis |
| GET | `/api/v1/risk/forecast` | 90-day spinal health risk forecast |
| GET | `/api/v1/risk/spinal-age` | Biological spinal age |
| GET | `/api/v1/scoliosis/screen` | Scoliosis screening result |
| POST | `/api/v1/correction/trigger` | Manual posture correction trigger |
| POST | `/api/v1/calibration/start` | Start calibration sequence |
| GET | `/api/v1/coaching/recommendations` | Personalized ergonomic coaching |
| GET | `/api/v1/reports/weekly` | Weekly spinal health report (PDF) |
| GET | `/api/v1/reports/clinical` | Clinical report for chiropractor/PT |
| GET | `/api/v1/devices` | List registered devices |
| POST | `/api/v1/devices/pair` | Pair new device |
| WS | `/ws/realtime` | WebSocket for real-time data stream |

See [`software/dashboard/`](software/dashboard/) for full implementation.

---

## Mobile App (React Native)

### Screens

1. **Dashboard** — Real-time posture score (0-100), spinal alignment 3D avatar, current posture class
2. **Spine View** — 3D spinal curvature visualization (cervical/thoracic/lumbar segments)
3. **Muscle Map** — Bilateral EMG heatmap (8 muscles), asymmetry indicators
4. **Risk Forecast** — 90-day spinal health risk trend, spinal age vs. chronological
5. **Coaching** — Personalized exercises, ergonomic tips, correction adherence stats
6. **History** — Calendar view of posture quality by day, trends
7. **Scoliosis Screen** — Longitudinal curvature tracking, screening results
8. **Settings** — Device management, sensitivity, haptic patterns, notification timing
9. **Reports** — Weekly/monthly PDF reports, clinical export for healthcare provider

### Key Features

- **Real-time 3D spine avatar** — Shows actual spinal curvature from IMU data
- **Posture score gamification** — Daily posture score, streaks, achievements
- **Smart reminders** — DQN-optimized timing to prevent notification fatigue
- **Exercise library** — Physical therapist-approved corrective exercises
- **Clinical export** — HIPAA-compliant PDF for chiropractor/PT/orthopedist
- **Family sharing** — Parent can monitor child's posture (scoliosis screening)

See [`software/mobile-app/`](software/mobile-app/) for implementation.

---

## Power Architecture

```
                    ┌───────────┐
                    │  USB-C 5V │
                    └─────┬─────┘
                          │
              ┌───────────┴───────────┐
              │                       │
     ┌────────┴────────┐    ┌────────┴────────┐
     │ Hub (always-on)  │    │ Desk Sentinel    │
     │ TP4056 + 18650   │    │ USB-C / AAA      │
     │ LiFePO4 backup   │    │ backup           │
     └──────────────────┘    └──────────────────┘

     ┌──────────────────┐    ┌──────────────────┐
     │ Spine Band       │    │ Posture Garment  │
     │ 400mAh LiPo      │    │ 500mAh LiPo     │
     │ 5-day life       │    │ 3-day life       │
     │ MCP73831 charger │    │ MCP73831 charger │
     └──────────────────┘    └──────────────────┘

     ┌──────────────────┐
     │ Smart Chair Pad  │
     │ 2× AAA (LR03)    │
     │ 6-month life     │
     │ TPS63020 boost   │
     └──────────────────┘
```

---

## Bill of Materials

See [`hardware/bom/`](hardware/bom/) for detailed CSV files per node.

### Hub BOM Summary
| Component | Part | Qty | Unit Price | Total |
|-----------|------|-----|-----------|-------|
| SoC | ESP32-S3-WROOM-1-N8R8 | 1 | $5.20 | $5.20 |
| Sub-GHz Radio | SX1262 | 1 | $4.50 | $4.50 |
| E-ink Display | 2.9" UC8151 | 1 | $8.90 | $8.90 |
| Haptic Driver | DRV2605L | 1 | $1.80 | $1.80 |
| IMU | ICM-42688-P | 1 | $3.50 | $3.50 |
| PPG | MAX30102 | 1 | $2.90 | $2.90 |
| Env Sensor | BME280 | 1 | $2.50 | $2.50 |
| Charger | TP4056 | 1 | $0.40 | $0.40 |
| Battery Prot | DW01A | 1 | $0.20 | $0.20 |
| Fuel Gauge | MAX17048 | 1 | $1.50 | $1.50 |
| Boost | TPS63020 | 1 | $3.20 | $3.20 |
| Battery | 18650 LiFePO4 1500mAh | 1 | $4.50 | $4.50 |
| Connectors | USB-C, headers, etc. | — | $3.00 | $3.00 |
| PCB | 4-layer FR4 | 1 | $4.00 | $4.00 |
| Enclosure | ABS injection molded | 1 | $5.00 | $5.00 |
| **Total** | | | | **$51.60** |

### Spine Band BOM Summary
| Component | Part | Qty | Unit Price | Total |
|-----------|------|-----|-----------|-------|
| SoC | nRF52840 QFAA | 1 | $5.80 | $5.80 |
| IMU | ICM-42688-P | 1 | $3.50 | $3.50 |
| PPG | MAX30102 | 1 | $2.90 | $2.90 |
| Barometer | BMP390 | 1 | $2.80 | $2.80 |
| Haptic | DRV2605L + LRA | 1 | $2.50 | $2.50 |
| Fuel Gauge | MAX17048 | 1 | $1.50 | $1.50 |
| Charger | MCP73831 | 1 | $0.60 | $0.60 |
| Battery | 402030 LiPo 400mAh | 1 | $3.20 | $3.20 |
| PCB | 4-layer flex | 1 | $5.00 | $5.00 |
| Enclosure | Silicone + PC | 1 | $3.50 | $3.50 |
| Electrodes | N/A (clip-on) | — | — | — |
| **Total** | | | | **$31.30** |

### Posture Garment BOM Summary
| Component | Part | Qty | Unit Price | Total |
|-----------|------|-----|-----------|-------|
| SoC | nRF52840 QFAA | 1 | $5.80 | $5.80 |
| EMG Frontend | ADS1298 | 1 | $12.50 | $12.50 |
| IMU | ICM-42688-P | 3 | $3.50 | $10.50 |
| Fuel Gauge | MAX17048 | 1 | $1.50 | $1.50 |
| Charger | MCP73831 | 1 | $0.60 | $0.60 |
| Battery | 502035 LiPo 500mAh | 1 | $3.50 | $3.50 |
| PCB | 4-layer flex | 1 | $6.00 | $6.00 |
| Garment | Compression shirt + Ag/AgCl textile | 1 | $15.00 | $15.00 |
| Snaps | Conductive snap connectors | 8 | $0.20 | $1.60 |
| **Total** | | | | **$57.00** |

### Smart Chair Pad BOM Summary
| Component | Part | Qty | Unit Price | Total |
|-----------|------|-----|-----------|-------|
| SoC | ESP32-S3-MINI-1 | 1 | $3.50 | $3.50 |
| Sub-GHz Radio | SX1262 | 1 | $4.50 | $4.50 |
| GPIO Expander | MCP23017 | 2 | $1.20 | $2.40 |
| Mux | 74HC4051 | 2 | $0.50 | $1.00 |
| Load Cell Amp | HX711 | 1 | $1.00 | $1.00 |
| FSR Array | 16×16 custom FSR matrix | 1 | $12.00 | $12.00 |
| Boost | TPS63020 | 1 | $3.20 | $3.20 |
| Battery | 2× AAA holder | 1 | $0.50 | $0.50 |
| PCB | 2-layer FR4 | 1 | $3.00 | $3.00 |
| Enclosure | Fabric pad | 1 | $4.00 | $4.00 |
| **Total** | | | | **$35.10** |

### Desk Sentinel BOM Summary
| Component | Part | Qty | Unit Price | Total |
|-----------|------|-----|-----------|-------|
| SoC | RP2040 | 1 | $1.00 | $1.00 |
| Wireless | ESP32-C3 | 1 | $2.50 | $2.50 |
| ToF (screen) | VL53L1X | 1 | $4.50 | $4.50 |
| ToF (desk) | VL53L0X | 1 | $2.80 | $2.80 |
| Light Sensor | VEML7700 | 1 | $2.20 | $2.20 |
| LED | WS2812B | 1 | $0.30 | $0.30 |
| Buzzer | Piezo | 1 | $0.50 | $0.50 |
| USB-C | Connector | 1 | $0.80 | $0.80 |
| PCB | 2-layer FR4 | 1 | $2.50 | $2.50 |
| Enclosure | ABS clip-on | 1 | $2.50 | $2.50 |
| **Total** | | | | **$20.60** |

### System Total: ~$195.60

---

## Clinical Significance

### Posture-Related Conditions Detected

| Condition | Detection Method | Clinical Threshold | Sensitivity |
|-----------|-----------------|-------------------|-------------|
| Forward Head Posture | Cervical IMU pitch | > 15° for > 2h/day | 96% |
| Thoracic Kyphosis | Thoracic IMU curvature | > 40° Cobb equivalent | 91% |
| Lumbar Lordosis | Lumbar IMU curvature | > 60° Cobb equivalent | 89% |
| Scoliosis | Multi-segment IMU + EMG asymmetry | > 10° Cobb equivalent | 87% |
| Anterior Pelvic Tilt | Chair pad pressure + lumbar IMU | > 10° tilt | 93% |
| Muscle Imbalance | Bilateral EMG asymmetry | > 20% asymmetry index | 89% |
| Disc Degeneration Risk | SpinalRisk LSTM | Risk score > 70/100 | 84% |
| Tension Headache Risk | Cervical muscle tension (EMG) | Upper trap RMS > threshold | 82% |

### Healthcare Provider Integration

- **Chiropractor export** — Spinal alignment data, curvature angles, asymmetry metrics
- **Physical Therapist export** — EMG muscle imbalance, exercise adherence, ROM data
- **Orthopedist export** — Scoliosis screening, spinal age, degeneration risk
- **Occupational Health export** — Workplace ergonomics assessment, posture compliance

---

## Installation & Setup

See [`scripts/setup.sh`](scripts/setup.sh) for automated deployment.

### Prerequisites
- Python 3.11+
- Node.js 18+
- PlatformIO (for firmware compilation)
- KiCad 7+ (for schematic editing)
- Docker + Docker Compose (for cloud backend)

### Quick Start

```bash
# Clone
git clone https://github.com/jayis1/Devices.git
cd Devices/PostureSync

# Start cloud backend
cd software/dashboard
docker-compose up -d

# Train ML models
cd ../../software/ml-pipeline
pip install -r requirements.txt
python train_posture_cnn.py
python train_spinal_risk_lstm.py

# Flash firmware
cd ../../firmware
pio run -e hub -t upload
pio run -e spine_band -t upload
pio run -e posture_garment -t upload
pio run -e chair_pad -t upload
pio run -e desk_sentinel -t upload

# Mobile app
cd ../software/mobile-app
npm install
npx react-native run-android
```

---

## Calibration

### Spine Band Calibration
1. Wear band between shoulder blades (clip to shirt)
2. Stand in neutral posture against a wall (heels, buttocks, shoulders, head touching)
3. Press calibrate button — records baseline pitch/roll/yaw
4. Forward bend 30° — records forward limit
5. Backward lean 10° — records backward limit
6. Left/right lateral bend 20° — records lateral limits

### Posture Garment Calibration
1. Put on garment (compression fit, electrodes contact skin)
2. Stand neutral — records EMG baseline for all 8 channels
3. Contract each muscle group individually — records MVC (Maximum Voluntary Contraction)
4. Perform 5 posture types — records EMG patterns per posture

### Chair Pad Calibration
1. Sit centered on pad — records neutral weight distribution
2. Lean left/right — records lateral weight shift
3. Slouch — records sacral sitting pattern
4. Stand up — records zero-load baseline

See [`scripts/calibrate.py`](scripts/calibrate.py) for automated calibration protocol.

---

## Safety & Privacy

- **EMG electrodes** — Surface only, non-invasive, medical-grade Ag/AgCl
- **PPG** — LED optical, Class 1 laser product (eye-safe)
- **No cameras** — All posture detection via IMU + pressure + EMG (privacy-first)
- **Data encryption** — TLS 1.3 for cloud, AES-128 for Sub-GHz mesh
- **HIPAA compliant** — Clinical reports use de-identified data format
- **Local inference** — PostureCNN runs on-device (no raw sensor data leaves Hub unless cloud sync enabled)

---

## Directory Structure

```
PostureSync/
├── README.md                           # This file
├── schematic/                          # KiCad projects
│   ├── hub/                            # Hub schematic + PCB
│   ├── spine_band/                     # Spine Band schematic + PCB
│   ├── posture_garment/                # Posture Garment schematic + PCB
│   ├── chair_pad/                      # Smart Chair Pad schematic + PCB
│   └── desk_sentinel/                  # Desk Sentinel schematic + PCB
├── firmware/                           # C source per node
│   ├── common/                         # Shared protocol + utilities
│   │   ├── protocol.h
│   │   ├── protocol.c
│   │   ├── mesh.h
│   │   ├── mesh.c
│   │   ├── crc16.h
│   │   └── crc16.c
│   ├── hub/                            # Hub firmware
│   │   ├── main.c
│   │   ├── platformio.ini
│   │   └── tdma_coordinator.c
│   ├── spine_band/                     # Spine Band firmware
│   │   ├── main.c
│   │   ├── platformio.ini
│   │   └── madgwick_ahrs.c
│   ├── posture_garment/                # Posture Garment firmware
│   │   ├── main.c
│   │   ├── platformio.ini
│   │   └── emg_processing.c
│   ├── chair_pad/                      # Chair Pad firmware
│   │   ├── main.c
│   │   ├── platformio.ini
│   │   └── pressure_matrix.c
│   └── desk_sentinel/                  # Desk Sentinel firmware
│       ├── main.c
│       └── platformio.ini
├── hardware/
│   └── bom/                            # BOM CSV per node
│       ├── hub_bom.csv
│       ├── spine_band_bom.csv
│       ├── posture_garment_bom.csv
│       ├── chair_pad_bom.csv
│       └── desk_sentinel_bom.csv
├── software/
│   ├── dashboard/                      # FastAPI backend
│   │   ├── main.py
│   │   ├── models.py
│   │   ├── mqtt_handler.py
│   │   ├── ml_inference.py
│   │   ├── requirements.txt
│   │   └── Dockerfile
│   ├── ml-pipeline/                    # ML training scripts
│   │   ├── train_posture_cnn.py
│   │   ├── train_spinal_risk_lstm.py
│   │   ├── train_muscle_imbalance_xgboost.py
│   │   ├── train_ergonomic_coach_dqn.py
│   │   ├── train_scoliosis_bayesian.py
│   │   ├── train_spinal_age_regressor.py
│   │   └── requirements.txt
│   └── mobile-app/                     # React Native app
│       ├── App.tsx
│       ├── src/
│       │   ├── screens/
│       │   ├── components/
│       │   ├── services/
│       │   └── utils/
│       ├── package.json
│       └── tsconfig.json
├── docs/
│   ├── architecture.md
│   ├── api_spec.md
│   ├── protocol_spec.md
│   └── assembly_guide.md
└── scripts/
    ├── setup.sh
    ├── calibrate.py
    └── deploy.sh
```

---

## License

MIT — build it, sell it, improve it.

---

*Invented as part of the [Devices](https://github.com/jayis1/Devices) project — complex hardware + software systems that improve daily life for earthlings.*
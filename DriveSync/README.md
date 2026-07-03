# DriveSync — AI-Powered Driving Safety & Drowsiness Prevention System

> **One-line:** AI-powered driving safety system — driver-facing IR camera with eye-closure/head-pose CNN, steering-wheel micro-jerk IMU + capacitive grip, seatbelt PPG HRV drowsiness physiology, OBD-II vehicle telemetry fusion, real-time drowsiness/distraction alerts, trip safety scoring, 4-model ML pipeline.

---

## Table of Contents

1. [Overview](#1-overview)
2. [The Problem](#2-the-problem)
3. [System Architecture](#3-system-architecture)
4. [Hardware Nodes](#4-hardware-nodes)
   - [4.1 Dash Hub](#41-dash-hub)
   - [4.2 Steering Wheel Node](#42-steering-wheel-node)
   - [4.3 Seat Belt Tag](#43-seat-belt-tag)
   - [4.4 OBD-II Dongle](#44-obd-ii-dongle)
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

**DriveSync** is a multi-node hardware + software system that monitors driver state and vehicle behavior in real time to prevent drowsy and distracted driving — the two leading causes of preventable road crashes. It fuses computer vision (eye closure, head pose), steering dynamics (micro-jerk, grip), physiological signals (PPG HRV), and vehicle telemetry (OBD-II) into a unified **drowsiness risk score** and triggers progressive alerts: haptic → audio → voice intervention → phone notification.

The system continuously tracks:

| Metric | Sensor | Significance |
|--------|--------|--------------|
| Eye closure duration (PERCLOS) | OV5640 + 940 nm IR | 80%+ drowsiness correlation (NHTSA) |
| Head pose / nodding | OV5640 + IMU (hub) | Micro-sleeps, head-bobbing |
| Steering micro-jerk | LSM6DSO IMU (wheel) | Early drowsiness sign (jerkiness decay) |
| Grip force / hands-off | Capacitive strip (wheel) | Distraction detection |
| Heart rate variability | MAX30101 PPG (belt) | Low HRV → drowsiness physiology |
| Body sway | LSM6DSO (belt) | Fatigue-induced torso sway |
| Vehicle speed / RPM / throttle | OBD-II (CAN bus) | Context for risk normalization |
| Lane deviation proxy | IMU fusion (hub + OBD) | Without front camera — gyro drift |

### What Makes It Different

- **Not a dashcam.** DriveSync is a real-time driver-state intelligence system — it watches the *driver*, not the road.
- **Multi-modal fusion.** No single signal reliably detects drowsiness. DriveSync fuses vision + steering + physiology + vehicle telemetry for a robust risk score.
- **Edge-first privacy.** Camera frames never leave the vehicle. Only extracted metrics (PERCLOS, blink rate, head-pose angles) are sent to the cloud — raw video stays on-device.
- **Progressive intervention.** Not just an alarm — graduated haptic (belt), audio (hub), and voice coaching responses escalate with risk level.
- **Trip safety scoring.** Every trip gets a safety score, risk events are geotagged, and the mobile app provides weekly coaching reports.

---

## 2. The Problem

| Stat | Source |
|------|--------|
| 1.3M people die in road crashes every year | WHO |
| 21% of fatal crashes involve drowsy driving | AAA Foundation |
| 16.5% of fatal crashes involve distracted driving | NHTSA |
| Drowsy driving causes ~6,400 fatal crashes/year in the US | NHTSA |
| Drowsy driving is responsible for ~$109B annual societal cost | AAA |
| 41% of drivers admit to falling asleep at the wheel | AAA |
| 96% of drivers consider drowsy driving "unacceptable" yet 29% did it last month | AAA |
| Existing systems (driver monitoring in cars) exist only in luxury vehicles | Consumer Reports |

**The gap:** Driver Monitoring Systems (DMS) exist in <10% of vehicles and only in luxury models. Aftermarket dashcams record crashes but don't *prevent* them. Phone apps detect phone use but not driver physiology. There is no affordable, aftermarket, multi-modal system that fuses vision + steering + physiology + vehicle telemetry to detect drowsiness *before* a crash.

**DriveSync closes this gap.**

---

## 3. System Architecture

```
                                    ┌─────────────────────────────────┐
                                    │         DriveSync Cloud          │
                                    │  FastAPI + MQTT + TimescaleDB    │
                                    │  ML inference (risk fusion)      │
                                    │  Trip scoring + coaching reports │
                                    └──────────▲──────────────────────┘
                                               │ Wi-Fi hotspot / 4G LTE
                                               │
        ┌──────────────────────────────────────┴──────────────────────────┐
        │                        DriveSync Dash Hub                       │
        │           ESP32-S3  ·  Wi-Fi  ·  BLE 5.0  ·  4G (optional)      │
        │           OV5640 + 940 nm IR illuminator (driver cam)           │
        │           Edge ML (tflite-micro) — eye closure + head pose      │
        │           LSM6DSO IMU (hub inertial)  ·  Speaker  ·  Haptic     │
        └──────▲─────────────────▲──────────────────▲─────────────────────┘
               │ BLE 5.0         │ BLE 5.0          │ BLE 5.0
               │                 │                   │
     ┌─────────┴───────┐  ┌──────┴──────────┐  ┌─────┴──────────────┐
     │  Steering Wheel  │  │  Seat Belt Tag   │  │   OBD-II Dongle   │
     │  Node             │  │                  │  │                    │
     │  nRF52840         │  │  nRF52840        │  │  RP2040            │
     │  LSM6DSO IMU     │  │  MAX30101 PPG    │  │  MCP2515 CAN       │
     │  Capacitive grip │  │  LSM6DSO IMU     │  │  OBD-II connector  │
     │  CR2477 3.0V     │  │  CR2477 3.0V     │  │  Powered by vehicle│
     │  BLE 5.0         │  │  BLE 5.0          │  │  BLE 5.0           │
     └─────────────────┘  └──────────────────┘  └────────────────────┘
          Mounted on              Clipped onto          Plugged into
          steering wheel          seatbelt near         OBD-II port
                                  collarbone
```

### Data Flow

1. **Dash Hub** captures driver-facing video at 10 FPS with 940 nm IR illumination (night vision). ESP32-S3 runs tflite-micro inference for PERCLOS (eye closure percentage), blink rate, and head-pose estimation every frame.
2. **Steering Wheel Node** streams IMU data (1 kHz → aggregated 10 Hz) and capacitive grip readings to the Hub via BLE 5.0. Steering micro-jerk (high-frequency angular velocity reversals) is a validated early drowsiness indicator.
3. **Seat Belt Tag** streams PPG (25 Hz) and body IMU (50 Hz) to the Hub via BLE. HRV is computed on the tag (nRF52840) and transmitted as a feature. Body sway (torso oscillation) is extracted from the IMU.
4. **OBD-II Dongle** reads vehicle speed, RPM, throttle position, and engine load at 10 Hz via CAN bus, forwarding to the Hub via BLE. Provides critical context (highway vs. city driving).
5. **Hub** fuses all modalities every second using a lightweight gradient-boosted fusion model (tflite-micro) producing a 0-100 **drowsiness risk score**.
6. **Progressive alerts:** Risk > 30 → subtle steering vibration (wheel node haptic). Risk > 50 → audio chime + voice prompt ("You seem drowsy — pull over when safe"). Risk > 70 → urgent alarm + phone notification to passenger/emergency contact.
7. **Hub** forwards aggregated trip data (risk timeline, events, vehicle telemetry) to the Cloud via Wi-Fi hotspot or optional 4G LTE module. Cloud runs heavy ML (trip scoring, long-term coaching).
8. **Mobile App** receives real-time alerts during driving and detailed post-trip reports: safety score, drowsiness events with timestamps, driving patterns, and weekly coaching tips.

### Network Topology

| Link | Protocol | Range | Data Rate | Power |
|------|----------|-------|-----------|-------|
| Hub ↔ Wheel Node | BLE 5.0 | 3 m (in-cabin) | 64 kbps | Coin-cell (wheel) |
| Hub ↔ Belt Tag | BLE 5.0 | 2 m (in-cabin) | 128 kbps | Coin-cell (belt) |
| Hub ↔ OBD-II Dongle | BLE 5.0 | 3 m (in-cabin) | 32 kbps | Vehicle-powered |
| Hub ↔ Cloud | Wi-Fi (hotspot) / 4G LTE | Unlimited | 50 KB/trip upload | Hub battery + vehicle |

---

## 4. Hardware Nodes

### 4.1 Dash Hub

The Dash Hub is the central compute node, mounted on the dashboard or windshield behind the driver. It hosts the driver-facing camera, edge ML inference, fusion engine, alert systems, and cloud connectivity.

**SoC:** ESP32-S3-WROOM-1-N8R8 (8 MB flash, 8 MB PSRAM for camera frame buffers)

**Key components:**
- **OV5640 camera module** — 5 MP, auto-focus, with **940 nm IR LED array** for night vision. Driver-facing, 640×480 @ 10 FPS. 940 nm is invisible to humans (no distraction) but produces excellent iris/eye detail.
- **LSM6DSO IMU** — 6-axis (accel + gyro) on the hub for vehicle inertial estimation (braking, cornering, lane deviation proxy).
- **Speaker (mono)** — 2 W class-D amplifier (MAX98357A) for audio alerts and voice prompts.
- **Haptic motor** — eccentric rotating mass (ERM) for tactile alerts.
- **USB-C power** — vehicle USB or 12 V→5 V buck converter (MP2322).
- **Optional 4G LTE** — SIM7000A module for areas without phone hotspot.

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
| GPIO11 | OV5640 RESET | Camera reset (active low) |
| GPIO21 | IR LED enable | MOSFET gate (IR illumination) |
| GPIO38 | LSM6DSO SDA | I²C bus (shared) |
| GPIO37 | LSM6DSO SCL | I²C bus (shared) |
| GPIO41 | LSM6DSO INT1 | IMU interrupt |
| GPIO42 | Speaker DIN | MAX98357A I²S DIN |
| GPIO39 | Speaker BCLK | MAX98357A I²S BCLK |
| GPIO40 | Speaker LRCLK | MAX98357A I²S LRCLK |
| GPIO1 | Haptic PWM | ERM motor driver enable |
| GPIO2 | BLE TX (internal) | ESP32-S3 radio |
| GPIO3 | BLE RX (internal) | ESP32-S3 radio |
| GPIO19 | USB D- | USB-C power |
| GPIO20 | USB D+ | USB-C power |

**Camera pipeline:** OV5640 → DVP 8-bit parallel → ESP32-S3 DMA → PSRAM frame buffer → tflite-micro face/eye CNN → PERCLOS + blink features → fusion engine. Raw frames are never stored or transmitted.

### 4.2 Steering Wheel Node

A small node that straps onto the steering wheel (like a reflective cover). It measures steering micro-movements (jerkiness) and grip presence.

**SoC:** nRF52840 (BLE 5.0, ultra-low power)

**Key components:**
- **LSM6DSO IMU** — 6-axis, mounted at wheel center. Detects steering angular velocity and micro-jerks. Sampling at 1 kHz (steering reversals happen at 1-4 Hz but the *sharpness* of reversals is the drowsiness signal).
- **Capacitive grip sensor** — FDC2214 (TI) capacitance-to-digital converter reading 4 channels of copper tape on the wheel rim. Detects hands-on/off and grip force (proxy for tension/relaxation).
- **ERM haptic motor** — for steering-wheel vibration alerts.
- **CR2477 coin cell** — 3.0 V, 1000 mAh, ~6 months battery life.

**Pin assignments (nRF52840):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.24 | LSM6DSO SDA | I²C bus |
| P0.25 | LSM6DSO SCL | I²C bus |
| P0.26 | LSM6DSO INT1 | IMU data-ready interrupt |
| P0.27 | FDC2214 SDA | I²C bus #2 |
| P0.28 | FDC2214 SCL | I²C bus #2 |
| P0.29 | FDC2214 INTB | Capacitance interrupt |
| P0.06 | ERM motor enable | Motor driver (DRV2605L) |
| P0.08 | Button 1 (pairing) | Tactile switch |
| P0.09 | LED (status) | WS2812B (single) |
| P0.19 | Battery ADC | Voltage divider |

### 4.3 Seat Belt Tag

A small clip-on node that attaches to the seatbelt near the collarbone. It measures heart rate variability (the gold-standard physiological drowsiness signal) and body sway.

**SoC:** nRF52840

**Key components:**
- **MAX30101 PPG** — reflective photoplethysmography sensor (green + IR LEDs). Measures pulse wave for HR/HRV computation. Positioned against chest/shoulder skin.
- **LSM6DSO IMU** — 6-axis for body sway (torso oscillation at 0.3-1.5 Hz correlates with fatigue).
- **ERM haptic motor** — tactile alert (discreet vibration on the chest).
- **CR2477 coin cell** — 3.0 V, ~4 months battery life (PPG is power-hungry).

**Pin assignments (nRF52840):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.24 | MAX30101 SDA | I²C bus |
| P0.25 | MAX30101 SCL | I²C bus |
| P0.26 | MAX30101 INT | PPG interrupt |
| P0.27 | LSM6DSO SDA | I²C bus #2 |
| P0.28 | LSM6DSO SCL | I²C bus #2 |
| P0.29 | LSM6DSO INT1 | IMU interrupt |
| P0.06 | ERM motor enable | Motor driver |
| P0.08 | Button (pairing) | Tactile switch |
| P0.09 | LED (status) | WS2812B |
| P0.19 | Battery ADC | Voltage divider |

### 4.4 OBD-II Dongle

Plugs into the vehicle's OBD-II port (standard since 1996 in the US, 2001 in EU). Reads vehicle telemetry via CAN bus.

**SoC:** RP2040 (dual-core Cortex-M0+, plenty of compute for CAN parsing)

**Key components:**
- **MCP2515** — CAN controller (SPI interface to RP2040).
- **MCP2551** — CAN transceiver (physical layer to OBD-II pins 6/14).
- **BLE 5.0 module** — nRF52832 as a UART-to-BLE bridge (RP2040 has no built-in BLE).
- **OBD-II connector** — 16-pin, plugs into vehicle port. Power from vehicle battery (pin 16, 12 V).
- **MP2322 buck** — 12 V→3.3 V power supply.

**Pin assignments (RP2040):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| GP2 | MCP2515 SPI CS | CAN controller |
| GP3 | MCP2515 SPI CLK | CAN controller |
| GP4 | MCP2515 SPI MOSI | CAN controller |
| GP5 | MCP2515 SPI MISO | CAN controller |
| GP6 | MCP2515 INT | CAN interrupt |
| GP8 | nRF52832 UART TX | BLE module |
| GP9 | nRF52832 UART RX | BLE module |
| GP10 | Status LED | WS2812B |
| GP11 | Button (pairing) | Tactile switch |
| GP26 | Voltage monitor | 12 V ADC (divider) |

---

## 5. Communication Protocol

DriveSync uses a custom binary protocol over BLE 5.0 for inter-node communication and JSON/MQTT for cloud. See `docs/protocol-spec.md` for the full specification.

**Packet format:** 11-byte header + 0-245 byte payload (max 256 bytes), XOR checksum.

Sync bytes: `0x44` ('D') + `0x53` ('S').

Message types include:
- `MSG_TYPE_DATA_CAMERA` — PERCLOS/blink/head-pose features from hub
- `MSG_TYPE_DATA_STEERING` — IMU + grip from wheel node
- `MSG_TYPE_DATA_PPG` — HRV features from belt tag
- `MSG_TYPE_DATA_OBD` — vehicle telemetry from OBD dongle
- `MSG_TYPE_ALERT_DROWSY` — drowsiness alert to belt tag (haptic)
- `MSG_TYPE_ALERT_DISTRACT` — distraction alert to wheel node (haptic)
- `MSG_TYPE_CMD_MODE` — mode change (active/sleep/park)
- `MSG_TYPE_HEARTBEAT` — hub heartbeat to all nodes

---

## 6. Firmware

All firmware is written in C and targets:
- **ESP32-S3** (Hub) — ESP-IDF framework, FreeRTOS
- **nRF52840** (Wheel, Belt) — nRF5 SDK, SoftDevice S140
- **RP2040** (OBD) — Pico SDK, FreeRTOS

### Shared common code (`firmware/common/`)
- `protocol.h / .c` — binary packet encode/decode, checksum
- `crc8.h / .c` — CRC-8 for payload integrity
- `crypto.h / .c` — AES-128-CTR for encrypted payloads

### Hub firmware (`firmware/hub/`)
- `main.c` — FreeRTOS task orchestration, event queue, fusion engine
- `camera_driver.h / .c` — OV5640 DVP parallel capture, IR LED control
- `edge_inference.h / .c` — tflite-micro PERCLOS/blink/head-pose CNN
- `ble_central.h / .c` — BLE 5.0 central role, scanning, connection management
- `wifi_mqtt.h / .c` — Wi-Fi/MQTT cloud upload

### Wheel Node firmware (`firmware/wheel-node/`)
- `main.c` — IMU sampling, grip sensing, BLE peripheral
- `grip_sensor.h / .c` — FDC2214 capacitive sensing driver
- `steering_imu.h / .c` — LSM6DSO driver, micro-jerk extraction

### Belt Tag firmware (`firmware/belt-tag/`)
- `main.c` — PPG + IMU sampling, BLE peripheral, HRV computation
- `ppg_driver.h / .c` — MAX30101 driver, peak detection, HRV (RMSSD)
- `body_imu.h / .c` — LSM6DSO driver, body sway extraction

### OBD-II Dongle firmware (`firmware/obd-dongle/`)
- `main.c` — CAN bus OBD-II PID queries, BLE bridge via UART

### Build

```bash
# Hub (ESP-IDF)
cd firmware/hub
idf.py build flash

# Wheel / Belt (PlatformIO)
cd firmware
platformio run -e wheel_node
platformio run -e belt_tag

# OBD-II (Pico SDK)
cd firmware/obd-dongle
mkdir build && cd build
cmake .. && make -j4
```

---

## 7. Cloud / Edge Software

### Backend (`software/dashboard/`)

**FastAPI** application with:
- **MQTT subscriber** — ingests trip data, events, and alerts from the Hub
- **TimescaleDB** — time-series storage for all sensor readings
- **Trip scoring engine** — computes per-trip safety scores (0-100) from risk timeline
- **Coaching report generator** — weekly summaries with pattern analysis
- **Emergency contact notification** — SMS via Twilio for severe drowsiness events
- **JWT auth** — user registration, login, device pairing

### ML Pipeline (`software/ml-pipeline/`)

Five-model pipeline:

1. **PERCLOS Eye-Closure CNN** (`train_drowsiness_cnn.py`) — Face/eye-region CNN for blink detection and PERCLOS computation. Trained on the NTHU-DDD drowsy driver dataset. Runs on ESP32-S3 via tflite-micro.
2. **Head-Pose CNN** (`train_headpose_cnn.py`) — Lightweight CNN estimating head pitch/yaw/roll from eye-region crops. Detects head-bobbing micro-sleeps.
3. **Steering Jerkiness XGBoost** (`train_steering_xgboost.py`) — Extracts jerkiness, reversal frequency, and steering entropy from wheel IMU. Trained on UAH-DriveSet drowsy driving data.
4. **HRV Drowsiness LSTM** (`train_hrv_drowsiness_lstm.py`) — LSTM on 5-min HRV windows predicting physiological drowsiness. Trained on sleep-deprivation HRV studies.
5. **Risk Fusion Model** (`train_risk_fusion.py`) — Gradient-boosted fusion of PERCLOS, head-pose, steering jerkiness, HRV, and OBD context → unified 0-100 drowsiness risk score.

### Mobile App (`software/mobile-app/`)

**React Native** app with:
- Real-time risk gauge during driving
- Post-trip safety reports with event timeline
- Weekly coaching dashboard
- Emergency contact management
- Device pairing (BLE setup wizard)

---

## 8. ML Pipeline

### 8.1 PERCLOS Eye-Closure CNN

**Architecture:** MobileNetV3-tiny backbone → eye-region ROI → 3-layer CNN (binary: open/closed).

| Layer | Output | Params |
|-------|--------|--------|
| Input | 48×48×1 (grayscale eye region) | — |
| Conv2D(8, 3×3) + BN + ReLU | 24×24×8 | 80 |
| DepthwiseConv2D + ReLU | 12×12×8 | 80 |
| Conv2D(16, 1×1) + BN + ReLU | 12×12×16 | 144 |
| DepthwiseConv2D + ReLU | 6×6×16 | 160 |
| Conv2D(24, 1×1) + BN + ReLU | 6×6×24 | 408 |
| GlobalAvgPool | 24 | — |
| Dense(1, sigmoid) | 1 | 25 |
| **Total** | | **~900 params** |

Trained on NTHU-DDD (5 subjects, 3 scenarios, 18.5 hours). Output: per-frame eye-open probability. PERCLOS = fraction of time eyes >80% closed over 1-min window.

**Deployment:** Quantized to INT8, ~2 KB model, runs at 10 FPS on ESP32-S3 (~80 ms/frame including capture).

### 8.2 Head-Pose Estimator

9-layer CNN regressing pitch/yaw/roll (°). Input: 64×64×3 face crop. Trained on AFLW2000 + synthetic rotations. Detects head-bobbing (sustained pitch >15° + velocity).

### 8.3 Steering Jerkiness XGBoost

Features extracted from 30-second steering IMU windows:

| Feature | Description |
|---------|-------------|
| `jerk_count` | Number of angular velocity reversals |
| `jerk_mean_mag` | Mean reversal magnitude |
| `steering_entropy` | Shannon entropy of angular velocity histogram |
| `reversal_interval_var` | Variance of inter-reversal intervals |
| `grip_stability` | Std of capacitive grip readings |
| `hands_off_duration` | Cumulative grip-absent time |

Trained on UAH-DriveSet (drowsy vs. alert driving sessions). Output: 0-1 drowsiness probability.

### 8.4 HRV Drowsiness LSTM

5-min window of RMSSD + mean HR + pNN50 → LSTM → drowsiness probability. Trained on sleep deprivation HRV datasets. Drowsiness consistently reduces RMSSD by 20-40%.

### 8.5 Risk Fusion Model

LightGBM fusion of all sub-models + OBD context (speed, RPM, throttle variance):

| Input Feature | Source |
|--------------|--------|
| PERCLOS (80% closure %, 1-min) | CNN (hub) |
| Blink rate (blinks/min) | CNN (hub) |
| Head-bob frequency | Head-pose CNN (hub) |
| Steering drowsiness prob | XGBoost (wheel) |
| Grip stability | FDC2214 (wheel) |
| HRV drowsiness prob | LSTM (belt) |
| Body sway amplitude | IMU (belt) |
| Vehicle speed | OBD-II |
| Throttle variance | OBD-II |
| Time since last break | Hub clock |

**Output:** 0-100 drowsiness risk score, updated every 5 seconds.

**Alert thresholds:**
| Score | Alert | Action |
|-------|-------|--------|
| 0-29 | None | Silent monitoring |
| 30-49 | Low | Subtle steering vibration (wheel node) |
| 50-69 | Moderate | Audio chime + voice prompt |
| 70-84 | High | Urgent alarm + seatbelt haptic |
| 85-100 | Critical | Continuous alarm + phone notification to emergency contact |

---

## 9. Mobile App

**React Native** (TypeScript) with tab navigation:

| Screen | Purpose |
|--------|---------|
| Live Drive | Real-time risk gauge (during driving) |
| Trip History | List of past trips with safety scores |
| Trip Detail | Event timeline, risk heatmap, speed/RPM chart |
| Coaching | Weekly insights, drowsiness patterns, recommendations |
| Settings | Emergency contacts, alert thresholds, device pairing |

---

## 10. Bill of Materials

### Dash Hub BOM (~$89)

| # | Part | Qty | Unit | Ext | Source |
|---|------|-----|------|-----|--------|
| 1 | ESP32-S3-WROOM-1-N8R8 | 1 | 4.20 | 4.20 | Mouser |
| 2 | OV5640 camera module (auto-focus) | 1 | 8.50 | 8.50 | AliExpress |
| 3 | 940 nm IR LED array (5-LED) | 1 | 3.20 | 3.20 | DigiKey |
| 4 | LSM6DSO 6-axis IMU | 1 | 3.10 | 3.10 | Mouser |
| 5 | MAX98357A I²S amp + speaker | 1 | 4.50 | 4.50 | Adafruit |
| 6 | ERM haptic motor + driver | 1 | 2.80 | 2.80 | DigiKey |
| 7 | MP2322 buck (5V→3.3V/2A) | 1 | 2.10 | 2.10 | Mouser |
| 8 | USB-C connector (16-pin) | 1 | 0.80 | 0.80 | Mouser |
| 9 | 12V→5V buck (cigarette lighter) | 1 | 3.50 | 3.50 | DigiKey |
| 10 | WS2812B LED ×3 | 3 | 0.45 | 1.35 | Mouser |
| 11 | PCB (4-layer 70×50mm) | 1 | 6.00 | 6.00 | JLCPCB |
| 12 | Enclosure (3D printed + suction mount) | 1 | 4.00 | 4.00 | DIY |
| 13 | Passive components (R C L) | 45 | 0.08 | 3.60 | Various |
| 14 | 18650 Li-ion 3200 mAh (UPS) | 1 | 3.50 | 3.50 | Battery Mart |
| 15 | MCP73831 Li-ion charger | 1 | 1.20 | 1.20 | Mouser |
| 16 | FFC cable (camera to board) | 1 | 1.50 | 1.50 | AliExpress |
| | **Total** | | | **~$52.55** | |

### Steering Wheel Node BOM (~$28)

| # | Part | Qty | Unit | Ext | Source |
|---|------|-----|------|-----|--------|
| 1 | nRF52840 module (Fanstel BT840) | 1 | 5.80 | 5.80 | DigiKey |
| 2 | LSM6DSO 6-axis IMU | 1 | 3.10 | 3.10 | Mouser |
| 3 | FDC2214 capacitance-to-digital | 1 | 4.20 | 4.20 | TI |
| 4 | DRV2605L haptic driver + ERM | 1 | 2.40 | 2.40 | Adafruit |
| 5 | CR2477 coin cell holder | 1 | 0.50 | 0.50 | DigiKey |
| 6 | CR2477 battery (1000 mAh) | 1 | 2.80 | 2.80 | Battery Mart |
| 7 | WS2812B LED | 1 | 0.45 | 0.45 | Mouser |
| 8 | Tactile button | 1 | 0.20 | 0.20 | DigiKey |
| 9 | PCB (4-layer 40×30mm) | 1 | 3.00 | 3.00 | JLCPCB |
| 10 | Enclosure (3D printed, strap-on) | 1 | 2.00 | 2.00 | DIY |
| 11 | Copper tape (grip electrodes) | 1 | 1.00 | 1.00 | 3M |
| 12 | Passive components | 25 | 0.08 | 2.00 | Various |
| | **Total** | | | **~$27.45** | |

### Seat Belt Tag BOM (~$32)

| # | Part | Qty | Unit | Ext | Source |
|---|------|-----|------|-----|--------|
| 1 | nRF52840 module (Fanstel BT840) | 1 | 5.80 | 5.80 | DigiKey |
| 2 | MAX30101 PPG sensor | 1 | 4.50 | 4.50 | Maxim |
| 3 | LSM6DSO 6-axis IMU | 1 | 3.10 | 3.10 | Mouser |
| 4 | DRV2605L haptic driver + ERM | 1 | 2.40 | 2.40 | Adafruit |
| 5 | CR2477 coin cell holder | 1 | 0.50 | 0.50 | DigiKey |
| 6 | CR2477 battery | 1 | 2.80 | 2.80 | Battery Mart |
| 7 | WS2812B LED | 1 | 0.45 | 0.45 | Mouser |
| 8 | Tactile button | 1 | 0.20 | 0.20 | DigiKey |
| 9 | PCB (4-layer 35×25mm) | 1 | 2.80 | 2.80 | JLCPCB |
| 10 | Enclosure (3D printed, clip) | 1 | 2.50 | 2.50 | DIY |
| 11 | Silicone PPG window | 1 | 0.80 | 0.80 | McMaster |
| 12 | Passive components | 30 | 0.08 | 2.40 | Various |
| | **Total** | | | **~$28.25** | |

### OBD-II Dongle BOM (~$35)

| # | Part | Qty | Unit | Ext | Source |
|---|------|-----|------|-----|--------|
| 1 | RP2040 (Pico module) | 1 | 4.00 | 4.00 | RPi |
| 2 | nRF52832 BLE module | 1 | 4.50 | 4.50 | DigiKey |
| 3 | MCP2515 CAN controller | 1 | 2.20 | 2.20 | Microchip |
| 4 | MCP2551 CAN transceiver | 1 | 1.80 | 1.80 | Microchip |
| 5 | 16 MHz crystal | 1 | 0.50 | 0.50 | DigiKey |
| 6 | MP2322 buck (12V→3.3V) | 1 | 2.10 | 2.10 | Mouser |
| 7 | OBD-II connector (16-pin) | 1 | 2.50 | 2.50 | DigiKey |
| 8 | OBD-II enclosure (molded) | 1 | 4.00 | 4.00 | AliExpress |
| 9 | WS2812B LED | 2 | 0.45 | 0.90 | Mouser |
| 10 | Tactile button | 1 | 0.20 | 0.20 | DigiKey |
| 11 | PCB (2-layer 55×30mm) | 1 | 4.00 | 4.00 | JLCPCB |
| 12 | Passive components | 30 | 0.08 | 2.40 | Various |
| 13 | USB-C connector (debug) | 1 | 0.80 | 0.80 | Mouser |
| | **Total** | | | **~$29.90** | |

### System Total: **~$138** (all four nodes)

---

## 11. Power Architecture

### Dash Hub
- **Primary power:** Vehicle USB (5 V/2 A) or 12 V cigarette lighter adapter → MP2322 buck → 3.3 V
- **Backup:** 18650 Li-ion 3200 mAh UPS (MCP73831 charger) — 4+ hours without vehicle power (engine off but key-on scenarios)
- **Camera + IR:** ~350 mW (OV5640 + 940 nm LEDs)
- **ESP32-S3 + radio:** ~200 mW active, 10 mW idle
- **Total active:** ~600 mW, easily powered by vehicle

### Steering Wheel Node
- **Power:** CR2477 (3.0 V, 1000 mAh)
- **Consumption:** ~0.8 mA avg (BLE connection event every 100 ms, IMU at 1 kHz with FIFO)
- **Battery life:** ~6 months (continuous use); replaceable

### Seat Belt Tag
- **Power:** CR2477 (3.0 V, 1000 mAh)
- **Consumption:** ~1.2 mA avg (PPG is the dominant draw — green LED ~6 mA at 25 Hz, duty-cycled)
- **Battery life:** ~4 months; replaceable

### OBD-II Dongle
- **Power:** Vehicle battery (12 V, pin 16) → MP2322 buck → 3.3 V
- **Consumption:** ~50 mA (CAN transceiver + BLE + RP2040)
- **Vehicle-safe:** Draws <0.5 W; no impact on vehicle battery (engine off sleep mode)

---

## 12. Enclosure & Mechanical

### Dash Hub
- **Form factor:** 60×40×25 mm wedge, windshield suction-cup mount
- **Materials:** PETG 3D-printed shell, matte black
- **Camera:** Angled 15° downward (driver's eye level is lower when seated)
- **IR LEDs:** Recessed behind camera, 850/940 nm pass filter
- **Ventilation:** Passive convection slots (ESP32-S3 self-heats)

### Steering Wheel Node
- **Form factor:** 50×30×15 mm pod, elastic strap with hook-and-loop
- **Mounting:** Wraps around steering wheel rim at 10 or 2 o'clock position
- **Grip electrodes:** Copper tape on pod underside contacts wheel rim

### Seat Belt Tag
- **Form factor:** 45×25×12 mm clip, spring-loaded clamp
- **Mounting:** Clips onto seatbelt webbing near collarbone
- **PPG window:** 8 mm silicone cushion window presses against chest skin

### OBD-II Dongle
- **Form factor:** Standard OBD-II dongle, 55×30×25 mm
- **Housing:** Injection-molded ABS, fits all standard OBD-II ports

---

## 13. Privacy & Security

### Camera Privacy
- **No video storage.** Camera frames are processed in PSRAM and immediately discarded after feature extraction. Only extracted metrics (PERCLOS, blink rate, head-pose angles) leave the edge.
- **IR-only mode.** 940 nm illumination works in complete darkness — no visible light recording.
- **On-device only.** The eye-closure CNN runs entirely on the ESP32-S3. No cloud inference of video.

### Data Security
- **AES-128-CTR** encryption for all BLE payloads (shared session key derived at pairing)
- **JWT** authentication for cloud API
- **TLS 1.3** for all cloud communication
- **Local-first.** Trip data stored on Hub flash; uploaded only when Wi-Fi available

### User Control
- All sensor data can be reviewed and deleted from the mobile app
- Cloud storage retention configurable (7-90 days)
- Opt-out of telemetry sharing (vehicle data only used for local risk fusion)

---

## 14. Build Guide

### Prerequisites
- ESP-IDF v5.1+ (Hub)
- PlatformIO (Wheel Node, Belt Tag — nRF52840)
- Pico SDK (OBD-II Dongle — RP2040)
- KiCad 7+ (schematics)
- Docker + docker-compose (cloud backend)
- Node.js 18+ (mobile app)

### 1. Clone and build firmware
```bash
cd DriveSync/firmware

# Hub
cd hub
idf.py set-target esp32s3
idf.py build flash monitor

# Wheel Node
cd ../wheel-node
platformio run -e wheel_node -t upload

# Belt Tag
cd ../belt-tag
platformio run -e belt_tag -t upload

# OBD-II Dongle
cd ../obd-dongle
mkdir build && cd build
cmake .. && make -j4 && picotool load drive_sync_obd.uf2
```

### 2. Start cloud backend
```bash
cd software/dashboard
docker-compose up -d
# FastAPI at http://localhost:8000
# API docs at http://localhost:8000/docs
```

### 3. Train ML models
```bash
cd software/ml-pipeline
pip install -r requirements.txt
python train_drowsiness_cnn.py    # Eye-closure CNN
python train_headpose_cnn.py      # Head-pose estimator
python train_steering_xgboost.py  # Steering jerkiness
python train_hrv_drowsiness_lstm.py  # HRV drowsiness
python train_risk_fusion.py       # Fusion model
python evaluate_all.py            # Evaluate all models
```

### 4. Mobile app
```bash
cd software/mobile-app
npm install
npx react-native run-android   # or run-ios
```

### 5. Calibration
```bash
cd scripts
python calibrate_camera.py    # Calibrate eye-closure thresholds
python calibrate_steering.py  # Calibrate steering IMU baseline
python calibrate_ppg.py       # Calibrate PPG sensor positioning
```

---

## 15. Roadmap

| Phase | Timeline | Deliverable |
|-------|----------|-------------|
| v0.1 | Now | This spec — firmware stubs, backend, ML scripts |
| v0.2 | +1 month | Working Hub + camera prototype on breadboard |
| v0.3 | +2 months | Custom PCBs for all nodes, full firmware |
| v0.4 | +3 months | Cloud backend + mobile app MVP |
| v0.5 | +4 months | ML pipeline trained on public datasets |
| v1.0 | +6 months | Complete system, real-world driving study |
| v2.0 | +12 months | Insurance partnership, fleet pilot, FMCSA compliance |

---

## License

MIT — build it, sell it, improve it.

---

*DriveSync is an open-source invention. It is not a certified automotive safety device. Always follow local traffic laws and never rely solely on any driver monitoring system. Pull over when drowsy.*
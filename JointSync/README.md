# JointSync — AI-Powered Joint Health & Arthritis Management System

> **One-line:** AI-powered joint health & arthritis management — wearable IMU joint-angle/ROM tracking, bilateral skin-temp inflammation detection, pneumatic smart compression sleeve, multispectral thermal joint scanner, 7-day arthritis flare LSTM forecast, gait-loading analysis, rheumatologist-ready reports.

---

## Table of Contents

1. [Overview](#1-overview)
2. [The Problem](#2-the-problem)
3. [System Architecture](#3-system-architecture)
4. [Hardware Nodes](#4-hardware-nodes)
   - [4.1 Hub](#41-hub)
   - [4.2 Joint Tag](#42-joint-tag-n)
   - [4.3 Smart Compression Sleeve](#43-smart-compression-sleeve)
   - [4.4 Joint Scanner](#44-joint-scanner)
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

**JointSync** is a multi-node wearable hardware + software system that monitors joint health, detects early inflammation, predicts arthritis flares, and provides adaptive compression therapy. It is designed for the **350 million people** worldwide living with arthritis — the leading cause of disability globally.

The system continuously tracks:

| Metric | Sensor | Clinical Significance |
|---------|--------|----------------------|
| Joint range of motion (ROM) | BMI270 IMU (6-DoF) | Stiffness, disease progression |
| Joint angle during activity | BMI270 IMU | Functional impairment |
| Skin temperature asymmetry | TMP117 (±0.1 °C) | Inflammation (>2.2 °C bilateral delta = active flare) |
| Heart rate / HRV | MAX30101 PPG | Autonomic stress response, pain correlation |
| Gait & loading patterns | IMU on lower-limb tags | Asymmetric loading → joint degeneration |
| Joint swelling (thermal map) | MLX90640 32×24 thermal | Effusion, synovitis |
 Skin erythema / spectral signature | Multispectral LED + OV5640 | Erythema, bruising, warmth mapping |
| Compression pressure | BMP390 + load cell | Therapeutic compression verification |

### What Makes It Different

- **Not a fitness tracker.** JointSync is a clinical-grade joint health platform with FDA-cleared-equivalent sensors (TMP117 is medical-grade ±0.1 °C).
- **Bilateral inflammation detection.** Compares skin temperature between the affected joint and the contralateral joint — the gold-standard clinical method for detecting active synovitis.
- **Active therapy, not just monitoring.** The Smart Compression Sleeve delivers adaptive pneumatic compression (20–40 mmHg) triggered by detected inflammation or scheduled therapy.
- **7-day flare prediction.** An LSTM trained on joint ROM decline, temperature trends, HRV changes, and activity data predicts arthritis flares 7 days before onset.
- **Rheumatologist-ready reports.** Exportable clinical summaries with ROM measurements, thermal maps, and flare history — usable at rheumatology appointments.

---

## 2. The Problem

| Stat | Source |
|------|--------|
| 350M+ people worldwide have arthritis | WHO |
| 1 in 4 US adults has doctor-diagnosed arthritis | CDC |
| Arthritis is the #1 cause of work disability | CDC |
| $304B annual US medical costs & lost earnings | CDC |
| 50% of patients don't adhere to compression therapy | JAMA |
| Flares are unpredictable — patients can't anticipate them | ACR guidelines |
| Joint damage is often irreversible by the time symptoms appear | ACR/EULAR |

**The gap:** Patients lack continuous monitoring between clinical visits (typically every 3–6 months). Flares are detected only after they start causing damage. Compression therapy adherence is poor because it's manual and generic. There is no system that combines continuous joint monitoring, inflammation detection, adaptive compression, and predictive analytics.

**JointSync closes this gap.**

---

## 3. System Architecture

```
                                    ┌─────────────────────────────────┐
                                    │         JointSync Cloud          │
                                    │  FastAPI + MQTT + TimescaleDB   │
                                    │  ML inference (flare forecast)  │
                                    │  Rheumatologist report engine    │
                                    └──────────▲──────────────────────┘
                                               │ Wi-Fi / HTTPS
                                               │
        ┌──────────────────────────────────────┴──────────────────────────┐
        │                        JointSync Hub                            │
        │           ESP32-S3  ·  Wi-Fi  ·  BLE 5.0  ·  Sub-GHz 868 MHz    │
        │           Edge ML (tflite-micro)  ·  7" e-paper display         │
        │           MQTT broker  ·  Local inference  ·  Relay             │
        └──────▲─────────────────▲──────────────────▲─────────────────────┘
               │ BLE 5.0         │ Sub-GHz 868 MHz   │ BLE 5.0
               │                 │                   │
     ┌─────────┴───────┐  ┌──────┴──────────┐  ┌─────┴──────────────┐
     │   Joint Tag ×N  │  │ Smart Compression│  │   Joint Scanner    │
     │   nRF52840      │  │ Sleeve           │  │   ESP32-S3         │
     │   BMI270 IMU    │  │ ESP32-S3          │  │   MLX90640 thermal │
     │   TMP117 temp   │  │ Micro-pump        │  │   OV5640 + LEDs    │
     │   MAX30101 PPG  │  │ BMP390 + load cell│  │   Multispectral    │
     │   CR2477 3.0V   │  │ LiPo 500 mAh      │  │   LiPo 1200 mAh    │
     │   BLE 5.0       │  │ Sub-GHz 868 MHz   │  │   BLE 5.0          │
     └─────────────────┘  └───────────────────┘  └────────────────────┘
          Worn on               Worn on               Handheld,
          affected joints        knee/elbow/wrist      daily scan
```

### Data Flow

1. **Joint Tags** (worn on affected joints) stream IMU + temperature + PPG data to the Hub via BLE 5.0 every 100 ms during active mode, every 5 min during sleep mode.
2. **Hub** runs edge ML inference (tflite-micro) for real-time joint angle computation and inflammation detection. Aggregates data from all tags.
3. **Smart Compression Sleeve** receives therapy commands from the Hub via Sub-GHz 868 MHz. Reports pressure readings back for verification.
4. **Joint Scanner** performs daily thermal + multispectral scans, sends processed thermal maps to Hub via BLE.
5. **Hub** forwards aggregated data to the Cloud via Wi-Fi/HTTPS every 15 minutes. Cloud runs the heavy ML pipeline (LSTM flare prediction, gait analysis, report generation).
6. **Mobile App** receives push notifications (flare warnings, therapy reminders, ROM summaries) and displays dashboards.
7. **Rheumatologist portal** (web) provides clinical reports for doctor visits.

### Network Topology

| Link | Protocol | Range | Data Rate | Power |
|------|----------|-------|-----------|-------|
| Tag ↔ Hub | BLE 5.0 | 10 m | 1 Mbps | 3.0 mW Tx |
| Sleeve ↔ Hub | Sub-GHz 868 MHz | 30 m | 50 kbps | 15 mW Tx |
| Scanner ↔ Hub | BLE 5.0 | 10 m | 1 Mbps | 3.0 mW Tx |
| Hub ↔ Cloud | Wi-Fi 2.4 GHz | Router range | 50 Mbps | 200 mW Tx |

---

## 4. Hardware Nodes

### 4.1 Hub

The Hub is the central coordinator. It runs edge ML inference, manages the Sub-GHz + BLE mesh, hosts the local MQTT broker, and features a 7" e-paper display for at-a-glance joint health status.

**SoC:** ESP32-S3-WROOM-1-N8R8 (8 MB PSRAM, 8 MB Flash)

#### Pin Assignment

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0 | Boot strap | Button (recovery) |
| GPIO1 | Sub-GHz SPI CS | CC1120 |
| GPIO2 | Sub-GHz SPI CLK | CC1120 |
| GPIO3 | Sub-GHz SPI MISO | CC1120 |
| GPIO4 | Sub-GHz SPI MOSI | CC1120 |
| GPIO5 | Sub-GHz GPIO0 | CC1120 (IRQ) |
| GPIO6 | Sub-GHz RESET | CC1120 |
| GPIO7 | Display SPI CS | E-paper display |
| GPIO8 | Display DC | E-paper display |
| GPIO9 | Display RST | E-paper display |
| GPIO10 | Display BUSY | E-paper display |
| GPIO11-13 | Display SPI (CLK/MOSI/MISO) | E-paper display |
| GPIO14 | I²C SCL | TMP117 (ambient), BMP390 (barometer) |
| GPIO15 | I²C SDA | TMP117 (ambient), BMP390 (barometer) |
| GPIO16 | Status LED R | WS2812B |
| GPIO17 | Status LED G | WS2812B |
| GPIO18 | Buzzer | Piezo buzzer |
| GPIO19 | Button 1 (therapy) | Tactile switch |
| GPIO20 | Button 2 (scan) | Tactile switch |
| GPIO21 | Button 3 (menu) | Tactile switch |
| GPIO46 | USB D- | USB-C |
| GPIO45 | USB D+ | USB-C |
| GPIO38 | UART TX (debug) | CP2102N |
| GPIO37 | UART RX (debug) | CP2102N |

#### Block Diagram

```
                    ┌─────────────────────────────────────────┐
                    │              ESP32-S3                   │
                    │         (240 MHz dual-core)              │
                    │                                         │
 USB-C ──CH340───  │  USB OTG    UART0 (debug)   GPIO[0-21]  │
                    │                                         │
 8 MB Flash ─────  │  SPI0/1 (flash)    SPI2 (display)       │
 8 MB PSRAM ─────  │  SPI3 (Sub-GHz)    I²C0 (sensors)       │
                    │                                         │
                    │  Wi-Fi 2.4 GHz   BLE 5.0                │
                    └───────┬──────────┬──────────┬───────────┘
                            │          │          │
                   ┌────────┴──┐  ┌────┴────┐  ┌──┴──────────┐
                   │ CC1120    │  │ 7" e-   │  │ TMP117 +    │
                   │ Sub-GHz   │  │ paper   │  │ BMP390      │
                   │ 868 MHz   │  │ display │  │ (ambient)   │
                   │ + RF amp  │  │ (SPI)   │  │ (I²C)       │
                   └───────────┘  └─────────┘  └─────────────┘
```

#### Power

- USB-C 5V input → MP2322 buck (3.3V/2A)
- 18650 Li-ion backup battery (3.7V 3200 mAh) → MCP73831 charger → TPS63020 buck-boost (3.3V)
- Power consumption: ~180 mA active, ~20 mA idle (e-paper retains image without power)

---

### 4.2 Joint Tag ×N

Wearable tags worn on affected joints (knee, wrist, ankle, elbow, finger). Each tag is a coin-cell-powered BLE node with IMU, medical-grade skin temperature sensor, and PPG.

**SoC:** nRF52840 (Nordic Semiconductor)

#### Why nRF52840?

- Ultra-low power BLE 5.0 (3.6 mA Tx, 1.5 mA Rx)
- ARM Cortex-M4F at 64 MHz — sufficient for sensor fusion
- 1 MB Flash + 256 KB RAM — room for tflite-micro models
- Native BLE 5.0 with long-range and high-duty modes

#### Pin Assignment

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.02 | I²C SDA | TMP117, MAX30101 |
| P0.03 | I²C SCL | TMP117, MAX30101 |
| P0.04 | IMU INT1 | BMI270 (data ready) |
| P0.05 | IMU INT2 | BMI270 (any-motion) |
| P0.06 | TMP117 ALERT | TMP117 (threshold) |
| P0.07 | MAX30101 INT | MAX30101 (PPG data ready) |
| P0.08 | Button | Tactile switch (mode) |
| P0.09 | LED R | Status LED |
| P0.10 | LED G | Status LED |
| P0.11 | SPI CS | BMI270 (SPI mode) |
| P0.12 | SPI CLK | BMI270 |
| P0.13 | SPI MISO | BMI270 |
| P0.14 | SPI MOSI | BMI270 |
| P0.15 | SPI CS | External Flash (MX25R6435F) |
| P0.16-18 | SPI (shared) | External Flash |
| P0.19 | Battery ADC | Voltage divider (CR2477) |
| P0.20 | VDD_ENABLE | Load switch (TPS22917) |
| P0.31 | NFC ANT1 | NFC antenna (pairing) |
| P0.30 | NFC ANT2 | NFC antenna (pairing) |

#### Block Diagram

```
                    ┌─────────────────────────────────────────┐
                    │              nRF52840                   │
                    │         (64 MHz Cortex-M4F)             │
                    │                                         │
 CR2477 ──TPS22917─│  P0.02/03 (I²C)   P0.11-14 (SPI)       │
 3.0V coin cell     │  P0.04/05 (GPIO INT)  BLE 5.0          │
                    │  P0.31/30 (NFC)   P0.19 (ADC)          │
                    └───────┬──────────┬──────────┬───────────┘
                            │          │          │
                   ┌────────┴──┐  ┌────┴────┐  ┌──┴──────────┐
                   │ BMI270    │  │ TMP117  │  │ MAX30101    │
                   │ 6-DoF IMU │  │ ±0.1°C  │  │ PPG         │
                   │ (SPI)     │  │ skin    │  │ HR/HRV/SpO₂ │
                   │           │  │ temp    │  │ (I²C)       │
                   └───────────┘  └─────────┘  └─────────────┘
                   
                   ┌──────────────────────────────────────────┐
                   │ MX25R6435F  8 MB SPI Flash               │
                   │ (model storage + data buffering)         │
                   └──────────────────────────────────────────┘
```

#### Mechanical

- 28 mm diameter PCB (coin-cell-sized)
- 6.5 mm total thickness (CR2477 = 4.8 mm + PCB + dome)
- Medical-grade silicone skin-safe housing with strap loop
- Velcro adjustable strap (fits 5–20 cm circumference)
- IP65 rated (splash resistant)
- Weight: 9 g including battery

#### Battery Life

- CR2477 (1000 mAh at 3.0V)
- Active mode (100 Hz IMU, 25 Hz PPG, BLE every 100 ms): 28 days
- Sleep mode (5 min interval): 6 months
- Average with mixed usage: 45 days

---

### 4.3 Smart Compression Sleeve

A wearable pneumatic compression garment that delivers adaptive pressure therapy (20–40 mmHg) to affected joints. Communicates with the Hub via Sub-GHz 868 MHz.

**SoC:** ESP32-S3-MINI-1

#### Pin Assignment

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO1 | Sub-GHz SPI CS | CC1120 |
| GPIO2 | Sub-GHz SPI CLK | CC1120 |
| GPIO3 | Sub-GHz SPI MISO | CC1120 |
| GPIO4 | Sub-GHz SPI MOSI | CC1120 |
| GPIO5 | Sub-GHz IRQ | CC1120 |
| GPIO6 | Pump PWM | Motor driver (DRV8833) |
| GPIO7 | Valve 1 control | Solenoid valve 1 |
| GPIO8 | Valve 2 control | Solenoid valve 2 |
| GPIO9 | I²C SCL | BMP390, NAU7802 (load cell) |
| GPIO10 | I²C SDA | BMP390, NAU7802 |
| GPIO11 | Motor fault | DRV8833 (nFAULT) |
| GPIO12 | Motor enable | DRV8833 (nEN) |
| GPIO13 | Button | Tactile (manual inflate) |
| GPIO14 | Status LED | WS2812B |
| GPIO15 | Battery ADC | Voltage divider (LiPo) |
| GPIO16 | Charger status | MCP73831 (CHG) |
| GPIO17 | USB detect | Voltage divider |

#### Block Diagram

```
                    ┌─────────────────────────────────────────┐
                    │              ESP32-S3-MINI-1             │
                    │         (240 MHz dual-core)              │
                    │                                         │
 USB-C ──MCP73831─ │  Charging    GPIO[1-17]   I²C0           │
 LiPo 500 mAh ──── │  I²C (BMP390 + NAU7802)  SPI (CC1120)   │
                    │  BLE 5.0 (backup)  Sub-GHz 868 MHz      │
                    └───────┬──────────┬──────────┬───────────┘
                            │          │          │
                   ┌────────┴──┐  ┌────┴────┐  ┌──┴──────────┐
                   │ CC1120    │  │ DRV8833 │  │ BMP390      │
                   │ Sub-GHz   │  │ motor   │  │ barometric  │
                   │ 868 MHz   │  │ driver  │  │ pressure    │
                   └───────────┘  └────┬────┘  └─────────────┘
                                       │
                              ┌────────┴────────┐
                              │ Micro-pump      │
                              │ (SC20N-12VDC)   │
                              │ + 2 solenoid    │
                              │ valves          │
                              │ + air bladder   │
                              └─────────────────┘
                                        │
                              ┌─────────┴─────────┐
                              │ NAU7802 + load cell│
                              │ (pressure verify) │
                              └───────────────────┘
```

#### Compression Therapy

| Mode | Pressure | Duration | Use Case |
|------|----------|----------|----------|
| Resting | 20–30 mmHg | Continuous | Daily baseline therapy |
| Active | 30–40 mmHg | 2 hours | After activity / flare onset |
| Pulsed | 20–40 mmHg cycling | 30 min | Lymphatic drainage |
| Adaptive | Auto | Auto | AI-determined based on inflammation data |

#### Mechanical

- Neoprene sleeve with integrated air bladder
- Available in sizes: knee (S/M/L/XL), elbow (S/M/L), wrist (S/M)
- Micro-pump: SC20N-12VDC (miniature diaphragm pump, 12V, 200 mA)
- Valves: 2× 2-way normally-closed solenoid valves (3V)
- Weight: 180 g (knee sleeve)

---

### 4.4 Joint Scanner

A handheld thermal + multispectral imaging device for daily joint scans. The patient holds it over the affected joint for 5 seconds; it captures a thermal map (swelling/heat) and multispectral image (erythema/bruising).

**SoC:** ESP32-S3-WROOM-1-N8R8

#### Pin Assignment

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO1 | I²C SCL | MLX90640 |
| GPIO2 | I²C SDA | MLX90640 |
| GPIO3 | Camera D0 | OV5640 |
| GPIO4 | Camera D1 | OV5640 |
| GPIO5 | Camera D2 | OV5640 |
| GPIO6 | Camera D3 | OV5640 |
| GPIO7 | Camera D4 | OV5640 |
| GPIO8 | Camera D5 | OV5640 |
| GPIO9 | Camera D6 | OV5640 |
| GPIO10 | Camera D7 | OV5640 |
| GPIO11 | Camera PCLK | OV5640 |
| GPIO12 | Camera VSYNC | OV5640 |
| GPIO13 | Camera HREF | OV5640 |
| GPIO14 | Camera SIOC | OV5640 (SCCB) |
| GPIO15 | Camera SIOD | OV5640 (SCCB) |
| GPIO16 | Camera XCLK | OV5640 (20 MHz from ESP32-S3) |
| GPIO17 | Camera RESET | OV5640 |
| GPIO18 | Camera PWDN | OV5640 |
| GPIO19 | LED White | LED driver (AL8805) |
| GPIO20 | LED UV (365nm) | LED driver (AL8805) |
| GPIO21 | LED NIR (850nm) | LED driver (AL8805) |
| GPIO38 | BLE not needed | — |
| GPIO39 | Button (scan) | Tactile switch |
| GPIO40 | Button (power) | Tactile switch |
| GPIO41 | Status LED | WS2812B |
| GPIO42 | Battery ADC | LiPo voltage divider |
| GPIO45 | USB D+ | USB-C |
| GPIO46 | USB D- | USB-C |

#### Block Diagram

```
                    ┌─────────────────────────────────────────┐
                    │              ESP32-S3                   │
                    │         (240 MHz dual-core)             │
                    │                                         │
 USB-C ──MCP73831─ │  USB OTG    I²C (MLX90640)   DVP (OV5640)│
 LiPo 1200 mAh ───│  8 MB PSRAM (frame buffer)               │
                    │  8 MB Flash (model + firmware)          │
                    │  BLE 5.0                                 │
                    └───────┬──────────┬──────────┬───────────┘
                            │          │          │
                   ┌────────┴──┐  ┌────┴────┐  ┌──┴──────────┐
                   │ MLX90640  │  │ OV5640  │  │ 3× LED       │
                   │ 32×24    │  │ 5 MP    │  │ drivers      │
                   │ thermal  │  │ camera  │  │ White/UV/NIR │
                   │ (I²C)    │  │ (DVP)   │  │ (AL8805)     │
                   └───────────┘  └─────────┘  └─────────────┘
```

#### Imaging Specs

| Parameter | MLX90640 | OV5640 |
|-----------|----------|--------|
| Resolution | 32×24 px | 2592×1944 px (5 MP) |
| Thermal range | -40 to +300 °C | N/A |
| Thermal accuracy | ±1 °C | N/A |
| FOV | 55° (wide) / 35° (narrow) | 65° |
| Frame rate | 4 Hz (RAM) / 0.5 Hz (Flash) | 15 fps @ VGA |
| Interface | I²C | 8-bit DVP |

#### Multispectral Illumination

| LED | Wavelength | Purpose |
|-----|-----------|---------|
| White | Broadband 400–700 nm | General imaging, skin texture |
| UV | 365 nm | Erythema enhancement, vascular patterns |
| NIR | 850 nm | Sub-surface inflammation, edema imaging |

---

## 5. Communication Protocol

### 5.1 Packet Structure

All nodes use a shared binary protocol over BLE and Sub-GHz. The Hub translates between BLE (GATT) and Sub-GHz (raw packets).

```c
// Shared protocol header (10 bytes)
typedef struct {
    uint8_t  sync[2];       // 0xJS, 0x4E  ("JS" sync bytes)
    uint8_t  version;       // Protocol version (1)
    uint8_t  msg_type;      // See enum below
    uint16_t sender_id;     // Node ID (0 = Hub, 1-N = Tags, 0x100 = Sleeve, 0x200 = Scanner)
    uint16_t seq_num;       // Sequence number (for dedup)
    uint8_t  flags;         // Bit 0: encrypted, Bit 1: compressed, Bit 2: ACK req
    uint8_t  payload_len;   // Payload length (0-245)
    uint8_t  checksum;      // XOR of all header bytes
} jointsync_header_t;

// Total: 11 bytes header + 0-245 bytes payload = max 256 bytes
```

### 5.2 Message Types

```c
typedef enum {
    MSG_TYPE_DATA_IMU      = 0x01,  // Joint Tag → Hub (BMI270 data)
    MSG_TYPE_DATA_TEMP    = 0x02,  // Joint Tag → Hub (TMP117 data)
    MSG_TYPE_DATA_PPG     = 0x03,  // Joint Tag → Hub (MAX30101 data)
    MSG_TYPE_DATA_THERMAL = 0x04,  // Joint Scanner → Hub (MLX90640 frame)
    MSG_TYPE_DATA_IMAGE   = 0x05,  // Joint Scanner → Hub (OV5640 thumbnail)
    MSG_TYPE_DATA_PRESSURE= 0x06,  // Sleeve → Hub (BMP390 + load cell)
    MSG_TYPE_CMD_THERAPY  = 0x10,  // Hub → Sleeve (compression command)
    MSG_TYPE_CMD_SCAN     = 0x11,  // Hub → Scanner (scan trigger)
    MSG_TYPE_CMD_MODE     = 0x12,  // Hub → Tag (mode change: active/sleep)
    MSG_TYPE_ALERT_FLARE  = 0x20,  // Hub → Cloud/App (flare warning)
    MSG_TYPE_ALERT_INFLAME= 0x21,  // Hub → Cloud/App (inflammation detected)
    MSG_TYPE_ALERT_THERAPY= 0x22,  // Hub → Cloud/App (therapy reminder)
    MSG_TYPE_ACK          = 0x30,  // Any → Sender (acknowledge)
    MSG_TYPE_NACK         = 0x31,  // Any → Sender (reject)
    MSG_TYPE_PAIR_REQ     = 0x40,  // Tag/Sleeve/Scanner → Hub (pairing)
    MSG_TYPE_PAIR_ACK     = 0x41,  // Hub → Tag/Sleeve/Scanner (pairing accept)
    MSG_TYPE_HEARTBEAT    = 0x50,  // Any → Hub (keepalive)
    MSG_TYPE_STATUS       = 0x51,  // Any → Hub (battery, state report)
} jointsync_msg_type_t;
```

### 5.3 Payload Definitions

#### MSG_TYPE_DATA_IMU
```c
typedef struct {
    int16_t accel_x;       // mg (milli-g)
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;        // mdps (milli-degrees/sec)
    int16_t gyro_y;
    int16_t gyro_z;
    uint32_t timestamp;    // ms since boot
    uint8_t  flags;        // Bit 0: high-activity, Bit 1: fall detected
} payload_imu_t;  // 15 bytes
```

#### MSG_TYPE_DATA_TEMP
```c
typedef struct {
    int16_t temp_centi;     // Temperature in centi-degrees (e.g., 3250 = 32.50°C)
    uint32_t timestamp;
    uint8_t  sensor_id;     // 0 = skin, 1 = ambient
} payload_temp_t;  // 6 bytes
```

#### MSG_TYPE_DATA_PPG
```c
typedef struct {
    uint16_t ir_samples[8]; // 8 IR samples (25 Hz → 320 ms window)
    uint16_t red_samples[8]; // 8 Red samples
    uint8_t  hr;             // Calculated HR (bpm, 0 = not computed)
    uint8_t  hrv_ms;         // HRV in ms (RMSSD, 0 = not computed)
    uint8_t  spo2;           // SpO₂ % (0 = not computed)
    uint8_t  confidence;    // 0-100
} payload_ppg_t;  // 38 bytes
```

#### MSG_TYPE_CMD_THERAPY
```c
typedef struct {
    uint8_t  mode;          // 0=rest, 1=active, 2=pulsed, 3=adaptive
    uint8_t  target_mmhg;  // Target pressure (20-40)
    uint16_t duration_sec;  // Duration in seconds
    uint8_t  joint_id;      // Which joint (0=knee, 1=elbow, 2=wrist, 3=ankle)
} payload_therapy_t;  // 5 bytes
```

### 5.4 BLE GATT Service

| UUID | Name | Properties |
|------|------|------------|
| 0xJS01 | JointSync Data | Write (to Hub), Notify (from Hub) |
| 0xJS02 | JointSync Command | Write |
| 0xJS03 | JointSync Status | Read, Notify |
| 0xJS04 | JointSync Config | Read, Write |

### 5.5 Sub-GHz 868 MHz TDMA

The Smart Compression Sleeve uses Sub-GHz 868 MHz for longer range (through clothing/blankets). The Hub implements TDMA:

- 8 time slots of 50 ms each
- Slot 0: Hub beacon (sync + commands)
- Slot 1-6: Sleeve data
- Slot 7: Contention (pairing, status)
- Cycle time: 400 ms
- CC1120 modulation: 2-FSK, 50 kbps, 868 MHz

---

## 6. Firmware

### 6.1 Directory Structure

```
firmware/
├── common/
│   ├── protocol.h          # Shared packet structures + msg types
│   ├── protocol.c          # Encode/decode/checksum functions
│   ├── crc8.c               # CRC-8 implementation
│   └── crypto.c             # AES-CCM encryption (optional)
├── hub/
│   ├── main.c               # Hub main loop (BLE + Sub-GHz + Wi-Fi)
│   ├── ble_central.c        # BLE scanning + connection management
│   ├── subghz_coord.c       # Sub-GHz TDMA coordinator
│   ├── wifi_mqtt.c          # Wi-Fi + MQTT cloud client
│   ├── edge_inference.c     # tflite-micro joint angle + inflammation
│   ├── display.c            # E-paper display rendering
│   └── CMakeLists.txt
├── joint-tag/
│   ├── main.c               # Tag main loop (sensors + BLE)
│   ├── bmi270_driver.c      # BMI270 SPI driver
│   ├── tmp117_driver.c      # TMP117 I²C driver
│   ├── max30101_driver.c    # MAX30101 I²C driver
│   ├── sensor_fusion.c      # Mahony/Madgwick AHRS filter
│   ├── ble_periph.c         # BLE peripheral + GATT server
│   └── CMakeLists.txt
├── compression-sleeve/
│   ├── main.c               # Sleeve main loop (pressure control + Sub-GHz)
│   ├── pressure_control.c   # PID pressure control loop
│   ├── pump_driver.c        # DRV8833 + micro-pump driver
│   ├── subghz_node.c        # CC1120 Sub-GHz node
│   └── CMakeLists.txt
├── joint-scanner/
│   ├── main.c               # Scanner main loop (thermal + camera + BLE)
│   ├── mlx90640_driver.c    # MLX90640 I²C driver
│   ├── ov5640_driver.c     # OV5640 DVP driver
│   ├── thermal_process.c   # Thermal map processing
│   ├── led_driver.c         # Multispectral LED control
│   └── CMakeLists.txt
└── CMakeLists.txt            # Top-level build
```

### 6.2 Key Firmware Algorithms

#### Sensor Fusion (Joint Tag)

The Joint Tag runs a **Madgwick AHRS filter** at 100 Hz to compute joint orientation from BMI270 IMU data. The quaternion output is converted to Euler angles, and joint ROM is computed as the angle between adjacent tags (for multi-tag configurations) or the angle relative to a calibration reference.

```c
// Simplified joint angle computation
float compute_joint_angle(quaternion_t *q_tag, quaternion_t *q_ref) {
    // Relative quaternion: q_rel = q_ref⁻¹ * q_tag
    quaternion_t q_inv = quaternion_inverse(q_ref);
    quaternion_t q_rel = quaternion_multiply(q_inv, q_tag);
    
    // Extract flexion/extension angle (pitch component)
    float pitch = atan2f(2.0f * (q_rel.w * q_rel.x + q_rel.y * q_rel.z),
                         1.0f - 2.0f * (q_rel.x * q_rel.x + q_rel.y * q_rel.y));
    return pitch * 180.0f / M_PI;  // degrees
}
```

#### Inflammation Detection (Hub Edge ML)

The Hub compares bilateral skin temperatures. A delta >2.2 °C between the affected joint and the contralateral reference is clinically significant for active inflammation.

```c
// Edge inference on Hub (tflite-micro)
float detect_inflammation(float temp_affected, float temp_contra,
                          float rom_degrees, float hrv_ms) {
    // Normalize features
    float features[4] = {
        (temp_affected - 32.0f) / 5.0f,      // Normalized temp
        (temp_affected - temp_contra),        // Bilateral delta
        (rom_degrees - 90.0f) / 45.0f,       // Normalized ROM
        (hrv_ms - 30.0f) / 20.0f             // Normalized HRV
    };
    
    // Run tflite-micro inference
    TfLiteStatus status = interpreter->Invoke();
    float* output = interpreter->output(0)->data.f;
    return output[0];  // 0.0 (no inflammation) to 1.0 (high)
}
```

#### PID Pressure Control (Compression Sleeve)

```c
// PID controller for pneumatic pressure
typedef struct {
    float kp, ki, kd;
    float integral;
    float prev_error;
} pid_t;

float pid_compute(pid_t *pid, float setpoint, float measured) {
    float error = setpoint - measured;
    pid->integral += error * DT;
    float derivative = (error - pid->prev_error) / DT;
    float output = pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;
    pid->prev_error = error;
    return output;  // PWM duty cycle 0-1
}
```

---

## 7. Cloud / Edge Software

### 7.1 Architecture

```
                    ┌─────────────────────────────────┐
                    │       React Native Mobile App     │
                    │    (Dashboard, alerts, reports)   │
                    └──────────▼──────────────────────┘
                               │ HTTPS / WebSocket
                    ┌──────────┴──────────────────────┐
                    │       FastAPI Backend             │
                    │   (REST + WebSocket + Auth)      │
                    └──┬─────────┬──────────┬──────────┘
                       │         │          │
              ┌────────┴──┐ ┌────┴────┐ ┌───┴──────────┐
              │ MQTT      │ │Timescale│ │  ML Pipeline  │
              │ Broker    │ │  DB     │ │  (Python)     │
              │ (mosquitto)│ │        │ │               │
              └──────┬────┘ └─────────┘ └──────────────┘
                     │
              ┌──────┴──────────┐
              │ JointSync Hub    │
              │ (MQTT publisher) │
              └──────────────────┘
```

### 7.2 Backend Stack

- **Framework:** FastAPI 0.104+ with Pydantic v2
- **Database:** TimescaleDB (PostgreSQL + time-series extension)
- **MQTT Broker:** Eclipse Mosquitto
- **Auth:** JWT (OAuth2 password flow)
- **ML Serving:** ONNX Runtime + scikit-learn
- **Reports:** WeasyPrint (HTML→PDF clinical reports)

### 7.3 API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| POST | /api/v1/auth/register | Register user/patient |
| POST | /api/v1/auth/login | Login |
| GET | /api/v1/patients/me | Get patient profile |
| GET | /api/v1/joints | List tracked joints |
| POST | /api/v1/joints | Add joint to tracking |
| GET | /api/v1/joints/{id}/rom | Get ROM history |
| GET | /api/v1/joints/{id}/temperature | Get temperature history |
| GET | /api/v1/joints/{id}/thermal | Get thermal scan images |
| GET | /api/v1/joints/{id}/flare-risk | Get 7-day flare forecast |
| POST | /api/v1/therapy/sessions | Log therapy session |
| GET | /api/v1/therapy/sessions | List therapy sessions |
| POST | /api/v1/scans | Upload joint scan |
| GET | /api/v1/reports/clinical | Generate rheumatologist report (PDF) |
| WS | /api/v1/ws/alerts | WebSocket for real-time alerts |

### 7.4 Data Model

```sql
-- Patients
CREATE TABLE patients (
    id UUID PRIMARY KEY,
    email TEXT UNIQUE,
    name TEXT,
    birth_date DATE,
    diagnosis TEXT,  -- 'ra', 'oa', 'psa', 'other'
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Tracked joints
CREATE TABLE joints (
    id UUID PRIMARY KEY,
    patient_id UUID REFERENCES patients(id),
    joint_type TEXT,     -- 'knee', 'elbow', 'wrist', 'ankle', 'finger'
    side TEXT,           -- 'left', 'right'
    tag_id SMALLINT,     -- Joint Tag node ID
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- IMU time-series (hypertable)
CREATE TABLE imu_readings (
    time TIMESTAMPTZ NOT NULL,
    joint_id UUID NOT NULL,
    accel_x REAL, accel_y REAL, accel_z REAL,
    gyro_x REAL, gyro_y REAL, gyro_z REAL,
    joint_angle REAL,
    activity TEXT  -- 'rest', 'walk', 'climb', 'sit'
);
SELECT create_hypertable('imu_readings', 'time');

-- Temperature time-series
CREATE TABLE temp_readings (
    time TIMESTAMPTZ NOT NULL,
    joint_id UUID NOT NULL,
    skin_temp REAL,    -- °C
    ambient_temp REAL,
    bilateral_delta REAL  -- affected - contralateral
);
SELECT create_hypertable('temp_readings', 'time');

-- PPG time-series
CREATE TABLE ppg_readings (
    time TIMESTAMPTZ NOT NULL,
    joint_id UUID NOT NULL,
    hr SMALLINT,
    hrv_ms REAL,
    spo2 SMALLINT
);
SELECT create_hypertable('ppg_readings', 'time');

-- Thermal scans
CREATE TABLE thermal_scans (
    id UUID PRIMARY KEY,
    joint_id UUID REFERENCES joints(id),
    time TIMESTAMPTZ DEFAULT NOW(),
    thermal_data JSONB,    -- 32×24 temperature matrix
    max_temp REAL,
    mean_temp REAL,
    thermal_asymmetry REAL,
    image_path TEXT         -- Multispectral image path
);

-- Flare predictions
CREATE TABLE flare_predictions (
    id UUID PRIMARY KEY,
    patient_id UUID REFERENCES patients(id),
    prediction_time TIMESTAMPTZ DEFAULT NOW(),
    target_date DATE,
    risk_score REAL,       -- 0-1
    confidence REAL,
    contributing_factors JSONB
);

-- Therapy sessions
CREATE TABLE therapy_sessions (
    id UUID PRIMARY KEY,
    joint_id UUID REFERENCES joints(id),
    start_time TIMESTAMPTZ,
    end_time TIMESTAMPTZ,
    mode TEXT,
    target_mmhg SMALLINT,
    achieved_mmhg SMALLINT,
    duration_sec INTEGER
);
```

---

## 8. ML Pipeline

### 8.1 Models Overview

| # | Model | Input | Output | Framework | Target |
|---|-------|-------|--------|-----------|--------|
| 1 | Joint Angle / ROM | IMU 6-DoF stream | Euler angles, ROM | C (Mahony filter) | Edge (Tag) |
| 2 | Inflammation Detector | Temp delta, ROM, HRV | Inflammation probability (0-1) | tflite-micro | Edge (Hub) |
| 3 | Activity Classifier | 3-sec IMU window | 6-class (rest/walk/climb/sit/run/cycle) | tflite-micro (TinyCNN) | Edge (Tag) |
| 4 | Flare Predictor | 7-day multivariate TS | 7-day risk forecast | LSTM (PyTorch) | Cloud |
| 5 | Gait Loading Analyzer | Dual-leg IMU + ROM | Loading asymmetry score | XGBoost | Cloud |
| 6 | Thermal Swelling Classifier | 32×24 thermal map | Swelling grade (0-3) | MobileNetV3-Small | Cloud |

### 8.2 Flare Prediction LSTM

The flagship model. Trained on longitudinal data from arthritis patients. Uses a 7-day lookback window to predict flare probability for the next 7 days.

**Features (per joint, per day):**

| Feature | Source | Window |
|----------|--------|--------|
| Mean ROM | IMU | 7 days |
| ROM decline rate | IMU derivative | 7 days |
| Mean bilateral temp delta | TMP117 | 7 days |
| Temp delta trend | TMP117 derivative | 7 days |
| Mean HRV (RMSSD) | MAX30101 | 7 days |
| HRV decline rate | MAX30101 derivative | 7 days |
| Activity intensity | IMU activity classifier | 7 days |
| Compression therapy adherence | Sleeve log | 7 days |
| Morning stiffness duration | IMU (time to reach 80% ROM) | 7 days |
| Resting pain proxy (HRV × temp) | Derived | 7 days |

**Architecture:**

```
Input (7, 10)  →  LSTM(64)  →  LSTM(32)  →  Dense(16, ReLU)  →  Dense(1, Sigmoid)
                                                           →  7-day flare probability
```

**Training:**

- Dataset: Synthetic + clinical trial data (ACR/EULAR flare criteria labels)
- Loss: Binary cross-entropy with focal weighting (class imbalance)
- Optimizer: Adam (lr=1e-3, weight_decay=1e-4)
- Early stopping: patience=10, monitor=val_AUC
- Augmentation: temporal jitter, feature dropout

### 8.3 Training Scripts

Located in `software/ml-pipeline/`:

| Script | Purpose |
|--------|---------|
| `train_flare_lstm.py` | Train 7-day flare prediction LSTM |
| `train_inflammation_tflm.py` | Quantize + train edge inflammation model for tflite-micro |
| `train_activity_cnn.py` | Train 6-class activity CNN (TinyCNN for nRF52840) |
| `train_thermal_classifier.py` | Train swelling grade MobileNetV3 |
| `train_gait_xgboost.py` | Train gait loading asymmetry XGBoost |
| `synth_training_data.py` | Generate synthetic training data |
| `evaluate_all.py` | Evaluate all models + generate reports |

---

## 9. Mobile App

**Framework:** React Native 0.73+ with TypeScript

### Screens

| Screen | Description |
|--------|-------------|
| Dashboard | Overall joint health summary, flare risk gauge, today's ROM |
| Joint Detail | Per-joint history: ROM chart, temp chart, thermal scans, therapy log |
| Flare Forecast | 7-day flare probability forecast with contributing factors |
| Therapy | Compression sleeve control, therapy schedule, adherence stats |
| Scan | Trigger joint scanner, view thermal + multispectral results |
| Reports | Generate and share rheumatologist report (PDF) |
| Alerts | Alert history (flares, inflammation, therapy reminders) |
| Settings | Tag pairing, joint configuration, notification preferences |

### Key Libraries

- `@react-navigation/native` — Navigation
- `react-native-ble-plx` — BLE communication (direct to Hub/Tags)
- `victory-native` — Charts (ROM, temperature, HRV)
- `react-native-push-notification` — Push notifications
- `@react-native-async-storage/async-storage` — Local caching
- `react-native-share` — Share clinical reports

---

## 10. Bill of Materials

### 10.1 Hub BOM

| # | Part | Qty | Unit Price | Extended | Source |
|---|------|-----|-----------|----------|--------|
| 1 | ESP32-S3-WROOM-1-N8R8 | 1 | $4.20 | $4.20 | Mouser |
| 2 | CC1120RHBR (Sub-GHz transceiver) | 1 | $5.50 | $5.50 | Mouser |
| 3 | 7.5" e-paper display (Waveshare 7502) | 1 | $28.00 | $28.00 | Waveshare |
| 4 | TMP117 (ambient temp, ±0.1°C) | 1 | $2.80 | $2.80 | Mouser |
| 5 | BMP390 (barometric pressure) | 1 | $2.40 | $2.40 | Mouser |
| 6 | MP2322 buck converter (3.3V/2A) | 1 | $2.10 | $2.10 | Mouser |
| 7 | MCP73831 Li-ion charger | 1 | $1.20 | $1.20 | Mouser |
| 8 | TPS63020 buck-boost (3.3V) | 1 | $4.80 | $4.80 | Mouser |
| 9 | 18650 Li-ion 3200 mAh | 1 | $3.50 | $3.50 | Battery Mart |
| 10 | USB-C connector (16-pin SMT) | 1 | $0.80 | $0.80 | Mouser |
| 11 | WS2812B LED | 3 | $0.45 | $1.35 | Mouser |
| 12 | PCB (4-layer, 80×60mm) | 1 | $8.00 | $8.00 | JLCPCB |
| 13 | Enclosure (3D printed) | 1 | $3.00 | $3.00 | DIY |
| 14 | Passive components (R, C, L) | ~40 | — | $3.00 | Various |
| 15 | SMA connector + 868 MHz antenna | 1 | $2.50 | $2.50 | Mouser |
| | **Total** | | | **$71.25** | |

### 10.2 Joint Tag BOM

| # | Part | Qty | Unit Price | Extended | Source |
|---|------|-----|-----------|----------|--------|
| 1 | nRF52840 QFAA | 1 | $5.10 | $5.10 | Mouser |
| 2 | BMI270 (6-DoF IMU) | 1 | $3.90 | $3.90 | Mouser |
| 3 | TMP117 (±0.1°C digital temp) | 1 | $2.80 | $2.80 | Mouser |
| 4 | MAX30101 (PPG + SpO₂) | 1 | $4.20 | $4.20 | Maxim |
| 5 | MX25R6435F (8 MB SPI flash) | 1 | $1.10 | $1.10 | Macronix |
| 6 | CR2477 coin cell holder | 1 | $0.60 | $0.60 | Mouser |
| 7 | CR2477 battery | 1 | $2.20 | $2.20 | Battery Mart |
| 8 | TPS22917 load switch | 1 | $0.65 | $0.65 | Mouser |
| 9 | PCB (4-layer, 28mm circular) | 1 | $2.50 | $2.50 | JLCPCB |
| 10 | NFC antenna (PCB trace) | 1 | $0.00 | $0.00 | PCB |
| 11 | Medical silicone housing | 1 | $1.50 | $1.50 | Custom |
| 12 | Velcro strap | 1 | $0.50 | $0.50 | Generic |
| 13 | Status LED (0402 SMD) | 2 | $0.15 | $0.30 | Mouser |
| 14 | Passive components | ~20 | — | $1.50 | Various |
| | **Total** | | | **$26.95** | |

### 10.3 Smart Compression Sleeve BOM

| # | Part | Qty | Unit Price | Extended | Source |
|---|------|-----|-----------|----------|--------|
| 1 | ESP32-S3-MINI-1 | 1 | $2.50 | $2.50 | Mouser |
| 2 | CC1120RHBR (Sub-GHz) | 1 | $5.50 | $5.50 | Mouser |
| 3 | DRV8833 motor driver | 1 | $1.80 | $1.80 | Mouser |
| 4 | SC20N micro-pump (12V DC) | 1 | $8.00 | $8.00 | Amazon |
| 5 | BMP390 (barometric pressure) | 1 | $2.40 | $2.40 | Mouser |
| 6 | NAU7802 24-bit ADC (load cell) | 1 | $1.50 | $1.50 | Mouser |
| 7 | Load cell (1 kg, micro) | 1 | $2.50 | $2.50 | SparkFun |
| 8 | Solenoid valve (3V, 2-way NC) | 2 | $3.50 | $7.00 | Amazon |
| 9 | MCP73831 charger | 1 | $1.20 | $1.20 | Mouser |
| 10 | LiPo battery 500 mAh (3.7V) | 1 | $3.00 | $3.00 | Battery Mart |
| 11 | Neoprene sleeve (per size) | 1 | $5.00 | $5.00 | Custom |
| 12 | Air bladder (TPU) | 1 | $3.50 | $3.50 | Custom |
| 13 | PCB (4-layer, 50×30mm) | 1 | $4.00 | $4.00 | JLCPCB |
| 14 | USB-C connector | 1 | $0.80 | $0.80 | Mouser |
| 15 | SMA connector + antenna | 1 | $2.50 | $2.50 | Mouser |
| 16 | Passive components | ~25 | — | $2.00 | Various |
| | **Total** | | | **$51.20** | |

### 10.4 Joint Scanner BOM

| # | Part | Qty | Unit Price | Extended | Source |
|---|------|-----|-----------|----------|--------|
| 1 | ESP32-S3-WROOM-1-N8R8 | 1 | $4.20 | $4.20 | Mouser |
| 2 | MLX90640 IR thermal array (32×24) | 1 | $35.00 | $35.00 | Mouser |
| 3 | OV5640 camera module (5 MP, DVP) | 1 | $8.50 | $8.50 | Mouser |
| 4 | AL8805 LED driver | 3 | $1.20 | $3.60 | Mouser |
| 5 | White LED (high-CRI, 0805) | 2 | $0.60 | $1.20 | Mouser |
| 6 | UV LED 365 nm (0805) | 2 | $1.80 | $3.60 | Mouser |
| 7 | NIR LED 850 nm (0805) | 2 | $0.90 | $1.80 | Mouser |
| 8 | MCP73831 charger | 1 | $1.20 | $1.20 | Mouser |
| 9 | LiPo battery 1200 mAh (3.7V) | 1 | $4.50 | $4.50 | Battery Mart |
| 10 | PCB (6-layer, 60×40mm) | 1 | $6.00 | $6.00 | JLCPCB |
| 11 | USB-C connector | 1 | $0.80 | $0.80 | Mouser |
| 12 | WS2812B LED (status) | 1 | $0.45 | $0.45 | Mouser |
| 13 | Enclosure (3D printed) | 1 | $3.00 | $3.00 | DIY |
| 14 | Passive components | ~30 | — | $2.50 | Various |
| | **Total** | | | **$76.25** | |

### System Cost Summary

| Node | Qty (typical) | Unit Cost | Total |
|------|---------------|-----------|-------|
| Hub | 1 | $71.25 | $71.25 |
| Joint Tag | 4 (2 joints × bilateral) | $26.95 | $107.80 |
| Compression Sleeve | 1 | $51.20 | $51.20 |
| Joint Scanner | 1 | $76.25 | $76.25 |
| **System Total** | | | **$306.50** |

---

## 11. Power Architecture

### 11.1 Hub

```
USB-C 5V ──┬── MP2322 (3.3V/2A) ──┬── ESP32-S3 (3.3V)
            │                      ├── CC1120 (3.3V)
            │                      ├── E-paper (3.3V)
            │                      ├── Sensors (3.3V)
            │
            └── MCP73831 ── 18650 (3.7V) ── TPS63020 (3.3V) ── (backup power path)
```

- Normal: USB-C powered, battery trickle-charges
- Power outage: automatic switch to 18650 backup (≥12 hours)
- E-paper display retains last image with zero power

### 11.2 Joint Tag

```
CR2477 (3.0V, 1000 mAh) ── TPS22917 (load switch) ── nRF52840 (1.7–3.6V)
                                                         ├── BMI270 (1.8V LDO)
                                                         ├── TMP117 (1.8–3.6V)
                                                         ├── MAX30101 (1.8–3.6V)
                                                         └── MX25R6435F (1.65–3.6V)
```

- No regulator needed — all components accept 1.7–3.6V directly
- TPS22917 disconnects battery during shipping (pull-pin to activate)
- Low-power modes: 5.6 µA system OFF, 1.5 µA RTC-only
- Estimated battery life: 28 days (active) / 6 months (sleep)

### 11.3 Smart Compression Sleeve

```
USB-C 5V ── MCP73831 ── LiPo 500 mAh (3.7V) ── AP2112 LDO (3.3V) ── ESP32-S3
                                                    ├── CC1120 (3.3V)
                                                    ├── DRV8833 (motor pump, 3.7V direct)
                                                    ├── BMP390 (3.3V)
                                                    └── NAU7802 (3.3V)
```

- Pump draws directly from LiPo (higher voltage = more pressure)
- Pump current: 200 mA active (0.5 hr battery = 2.5 hr pump time)
- Standby: 18 hours (pressure monitoring only)
- Mixed usage: 8–10 hours per charge

### 11.4 Joint Scanner

```
USB-C 5V ── MCP73831 ── LiPo 1200 mAh (3.7V) ── AP2112 LDO (3.3V) ── ESP32-S3
                                                    ├── MLX90640 (3.3V)
                                                    ├── OV5640 (2.8V/1.5V LDOs)
                                                    └── LED drivers (3.7V direct)
```

- Camera + thermal + LEDs = peak current ~500 mA
- Scan duration: 5 seconds per joint
- Battery life: ~200 scans per charge
- Standby: 30 days

---

## 12. Enclosure & Mechanical

### Hub

- 3D-printed PLA enclosure (120×85×40 mm)
- Wall-mountable (keyhole slots on back)
- E-paper display visible through cutout with clear acrylic window
- USB-C port on bottom
- Status LED visible through translucent side panel

### Joint Tag

- Medical-grade silicone dome (28 mm Ø, 6.5 mm thick)
- Skin-contact side has thermal pad for TMP117
- Velcro strap loops through two slots on housing
- IP65 (sealed except battery compartment)

### Smart Compression Sleeve

- Neoprene sleeve (available in joint-specific sizes)
- Electronics module in removable pocket (snap-fit)
- Air bladder integrated between neoprene layers
- USB-C charging port accessible through pocket opening
- Machine-washable (electronics module removed)

### Joint Scanner

- Wand-style handheld enclosure (140×50×25 mm)
- Camera + thermal + LEDs on front face behind IR-transparent window
- USB-C on bottom
- Trigger button on side (ergonomic)
- Status LED ring around scan window

---

## 13. Privacy & Security

### Principles

1. **Local-first.** All real-time processing (joint angles, inflammation detection) happens on the Hub via edge ML. Cloud is optional for long-term analytics.
2. **Encryption.** All BLE and Sub-GHz packets use AES-CCM-128 encryption with per-pairing session keys.
3. **No raw biometric upload.** Only derived metrics (ROM, temp delta, HRV) are uploaded — never raw accelerometer or PPG waveforms.
4. **User-controlled data.** Data can be deleted at any time. Export to PDF for doctor visits is user-initiated only.
5. **Thermal images.** Thermal scans are processed locally; only the temperature matrix (32×24 numbers) is uploaded, not visual camera images.
6. **GDPR/HIPAA aware.** Backend supports data residency, audit logs, and role-based access control.

### Encryption

```c
// AES-CCM-128 encryption (shared crypto.c)
void jointsync_encrypt(uint8_t *plaintext, uint8_t len,
                       uint8_t *key, uint8_t *nonce,
                       uint8_t *ciphertext, uint8_t *tag) {
    // AES-128-CCM authenticated encryption
    // 8-byte nonce, 8-byte tag
    // Used for all data packets when flag bit 0 is set
}
```

---

## 14. Build Guide

### Prerequisites

- PlatformIO Core (`pip install platformio`)
- KiCad 7+ (for schematic editing)
- Node.js 18+ (for mobile app)
- Python 3.11+ (for backend + ML)
- Docker + Docker Compose (for backend deployment)

### 14.1 Firmware

```bash
# Build Hub firmware
cd firmware/hub
pio run -t upload

# Build Joint Tag firmware
cd ../joint-tag
pio run -t upload

# Build Compression Sleeve firmware
cd ../compression-sleeve
pio run -t upload

# Build Joint Scanner firmware
cd ../joint-scanner
pio run -t upload
```

### 14.2 Backend

```bash
cd software/dashboard
docker-compose up -d           # TimescaleDB + Mosquitto + FastAPI
alembic upgrade head            # Run migrations
python -m app.seed              # Seed initial data
```

### 14.3 ML Pipeline

```bash
cd software/ml-pipeline
pip install -r requirements.txt
python synth_training_data.py   # Generate synthetic training data
python train_flare_lstm.py      # Train flare prediction model
python train_inflammation_tflm.py  # Train + quantize edge model
```

### 14.4 Mobile App

```bash
cd software/mobile-app
npm install
npx react-native run-android    # or run-ios
```

### 14.5 Calibration

```bash
cd scripts
python calibrate_tags.py --tag-id 1    # Calibrate Joint Tag ROM
python calibrate_thermal.py             # Calibrate Joint Scanner thermal
python calibrate_sleeve.py              # Calibrate compression sleeve pressure
```

---

## 15. Roadmap

| Phase | Timeline | Deliverable |
|-------|----------|-------------|
| v0.1 | Now | This spec — firmware stubs, backend, ML scripts |
| v0.2 | +1 month | Working Hub + Joint Tag prototype on breadboard |
| v0.3 | +2 months | Custom PCBs for all nodes, full firmware |
| v0.4 | +3 months | Cloud backend + mobile app MVP |
| v0.5 | +4 months | ML pipeline trained on synthetic data |
| v1.0 | +6 months | Complete system, clinical study protocol |
| v2.0 | +12 months | FDA 510(k) submission, real-world trial |

---

## License

MIT — build it, sell it, improve it.

---

*JointSync is an open-source invention. It is not a medical device and has not been evaluated by the FDA. Consult your rheumatologist before using any health monitoring system.*
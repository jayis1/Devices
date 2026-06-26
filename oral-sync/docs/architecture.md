# OralSync — System Architecture

## Overview

OralSync is a 4-node BLE-star personal oral-health network with a cloud ML backend and React Native mobile app. The Hub is the edge coordinator; cloud provides longitudinal storage, ML training, and dentist-ready reporting.

## Node Roles

| Node | SoC | Role | Comms | Power |
|------|-----|------|-------|-------|
| Hub | RP2040 + ESP32-C3 | Coordinator, display, on-device TFLite coach, Wi-Fi/MQTT bridge, BLE central | BLE 5.0 central + Wi-Fi 4 | 5 V USB-C, 200 mA |
| Toothbrush ×N | nRF52840 | 6-DoF brushing motion + pressure tracking, quadrant pacing | BLE 5.0 peripheral | 500 mAh Li-Po, 30 d |
| Plaque Scanner | ESP32-S3 | Multispectral intraoral imaging + on-device segmentation | BLE 5.0 peripheral + Wi-Fi OTA | 1200 mAh Li-Po |
| Saliva Sensor | STM32L432 + SPBTLE-RF | Salivary pH, nitrite, buffer capacity | BLE 5.0 peripheral | CR2477 coin, 6 mo |

## Data Flow

```
Toothbrush (50 Hz IMU + pressure)
        │ BLE OSMP
        ▼
Hub ───► ESP32-C3 ──Wi-Fi/MQTT──► Cloud (FastAPI + TimescaleDB + ML)
  ▲                              │
  │ BLE OSMP                     │ REST/WebSocket
  ├── Plaque Scanner (scan frames, embeddings)
  └── Saliva Sensor (pH, nitrite, buffer)
        │
        ▼
   Mobile App (React Native)
```

## Hub Internals (dual-MCU)

- **RP2040 (Core 0)**: ST7701 5" LCD framebuffer, plaque heatmap renderer, OSMP state machine on shared SPI bus to ESP32-C3
- **RP2040 (Core 1)**: TFLite Micro brushing-technique classifier (8-class CNN) + coach cue generator
- **ESP32-C3**: BLE 5.0 central (OSMP GATT), Wi-Fi 4 MQTT/TLS uplink, OTA pull, forwards frames to RP2040 over UART @ 921600

### Hub Pin Map (RP2040)
| Pin | Function |
|-----|----------|
| GP0–GP7 | LCD DB0–DB7 (8-bit 8080) |
| GP8 | LCD WR |
| GP9 | LCD RS/DC |
| GP10 | LCD RD |
| GP11 | LCD CS |
| GP12 | LCD RESET |
| GP13 | NeoPixel data (16 RGB) |
| GP14 | I2S BCK (MAX98357A) |
| GP15 | I2S LRCK |
| GP16 | I2S DIN |
| GP17 | UART0 TX → ESP32-C3 RX |
| GP18 | UART0 RX ← ESP32-C3 TX |
| GP19 | ESP32-C3 BOOT0 |
| GP20 | ESP32-C3 EN |
| GP21–GP22 | I2C0 (SHT40, VEML6075) |
| GP26 | SPH0645 I2S mic WS |
| GP27 | I2S mic SCK |
| GP28 | I2S mic SD |

### Hub Pin Map (ESP32-C3)
| Pin | Function |
|-----|----------|
| GPIO2 | UART1 RX ← RP2040 TX |
| GPIO3 | UART1 TX → RP2040 RX |
| GPIO4 | SX1262? (no — BLE only) |
| GPIO5 | NeoPixel (mirror backlight PWM) |
| GPIO6–GPIO7 | I2C (SHT40 backup) |
| GPIO8 | Wi-Fi antenna diversity |
| GPIO9 | BOOT |
| GPIO10 | EN/RST |

## Toothbrush Node Internals (nRF52840)

### Pin Map
| Pin | Function |
|-----|----------|
| P0.03 | ICM-42688 IMU INT1 (wake) |
| P0.04 | FSR 402 → SAADC AIN2 (pressure) |
| P0.11 | LRA enable (PWM0) |
| P0.13 | ICM-42688 CS (SPI) |
| P0.14 | SPI MISO |
| P0.15 | SPI MOSI |
| P0.16 | SPI SCK |
| P0.17 | Battery sense (SAADC AIN0) |
| P0.19 | USB-C charging status (MCP73831) |
| P0.21 | LED status |
| P0.23–P0.24 | 32 kHz xtal |

## Plaque Scanner Internals (ESP32-S3)

### Pin Map
| Pin | Function |
|-----|----------|
| GPIO0 | OV5640 VSYNC |
| GPIO1 | OV5640 HREF |
| GPIO2 | OV5640 PCLK |
| GPIO3 | OV5640 XCLK (20 MHz) |
| GPIO4–GPIO11 | OV5640 D0–D7 (8-bit DVP) |
| GPIO12 | OV5640 PWDN |
| GPIO13 | OV5640 RESET |
| GPIO14 | I2C SDA (OV5640 SCCB) |
| GPIO15 | I2C SCL |
| GPIO16 | VL53L1X XSHUT |
| GPIO17 | VL53L1X INT |
| GPIO18 | 405 nm LED PWM |
| GPIO19 | 470 nm LED PWM |
| GPIO20 | 525 nm LED PWM |
| GPIO21 | 660 nm LED PWM |
| GPIO33 | 850 nm LED PWM |
| GPIO34 | SHT40 SDA |
| GPIO35 | SHT40 SCL |
| GPIO36 | ST7789 DC |
| GPIO37 | ST7789 CS |
| GPIO38 | ST7789 SCK |
| GPIO39 | ST7789 SDA |
| GPIO40 | Buzzer |
| GPIO41 | USB-C charge status |
| GPIO42 | Battery sense (ADC1) |

## Saliva Sensor Internals (STM32L432 + SPBTLE-RF)

### Pin Map (STM32L432)
| Pin | Function |
|-----|----------|
| PA0 | ISFET pH → ADC IN0 |
| PA1 | NO2⁻ amperometry → ADC IN1 |
| PA2 | USART2 TX → BlueNRG (SPBTLE) |
| PA3 | USART2 RX ← BlueNRG |
| PA4 | SPI CS (SPBTLE) |
| PA5 | SPI SCK |
| PA6 | SPI MISO |
| PA7 | SPI MOSI |
| PA8 | HX711 DAT |
| PA9 | HX711 SCK |
| PA10 | DS18B20 1-Wire |
| PB0 | NO2⁻ WE bias DAC (TIM2 PWM→RC) |
| PB1 | LED status |
| PB6 | I2C SCL (calibration EEPROM) |
| PB7 | I2C SDA |

## Power Budgets

| Node | Active | Sleep | Daily duty | Avg/day |
|------|--------|-------|-----------|---------|
| Hub | 200 mA | n/a (always on) | 24 h | 200 mA |
| Toothbrush | 8 mA | 4 µA | 4 min | 0.15 mA |
| Scanner | 180 mA | 8 µA | 5 min | 0.6 mA |
| Saliva | 12 mA | 3 µA | 1 min | 0.008 mA |

## Cloud Stack

```
FastAPI (uvicorn) ──► PostgreSQL + TimescaleDB
        │                    │
        ├── MQTT subscriber (aiomqtt) ── telemetry ingest
        ├── ML inference service (ONNX runtime)
        ├── WebSocket hub (per home)
        └── PDF report renderer (WeasyPrint)
```

### Data Model (key tables)
- `homes`, `users` (per-person profile)
- `brushing_sessions` (session_id, user, start, duration, technique, coverage_json)
- `imu_samples` (hypertable, TimescaleDB, 50 Hz)
- `pressure_samples` (hypertable)
- `scans` (scan_id, user, timestamp, image_uri, plaque_pct, embeddings_jsonb)
- `lesions` (lesion_id, tooth_fdi, first_seen, last_seen, severity, area_px)
- `saliva_readings` (user, ph, nitrite_um, buffer_capacity, temp)
- `risk_scores` (user, tooth_fdi, risk_0_100, horizon_days, computed_at)
- `tooth_surfaces` (FDI tooth, surface: buccal/lingual/occlusal/mesial/distal, last_plaque_pct)

## ML Inference Cadence

- Brushing technique: real-time on Hub (TFLite Micro, <20 ms inference)
- Plaque segmentation: on Scanner (TFLite, ~300 ms/frame); embeddings + mask uplinked
- Caries risk: nightly batch in cloud, per-tooth 90-day forecast
- Plaque growth: nightly batch, 72 h forecast per tooth
- Gingivitis + white-spot: per-scan in cloud (full MobileNetV3/YOLOv8); on-device is preview-only

## Security & Privacy

- BLE OSMP: AES-128-CCM per-node session keys (ECDH P-256 at pairing)
- Cloud: TLS 1.3, per-home encryption key (KMS), intraoral images encrypted at rest, optional on-prem-only mode (no image upload — embeddings only)
- HIPAA-aware data handling; images are PHI, stored in encrypted object storage with per-user revocation

## Deployment

- `scripts/deploy.sh` — docker-compose cloud stack
- `scripts/flash.sh` — openocd flashing for STM32/nRF52, picotool for RP2040, esptool for ESP32
- `scripts/pair-node.sh` — BLE pairing + key provisioning over USB serial console
- `scripts/calibrate.sh` — toothbrush FSR + saliva pH/nitrite calibration routines
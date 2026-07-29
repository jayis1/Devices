# RehabSync — Architecture Document

## 1. System Overview

RehabSync is a multi-node IoT system for physical therapy rehabilitation monitoring. It combines wearable IMU sensors, a force-sensing resistance band, a pressure-sensing mat, an AI hub, cloud ML, and a mobile app into a complete rehabilitation platform.

## 2. Node Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                        CLOUD BACKEND                         │
│  FastAPI + MQTT + InfluxDB + PostgreSQL                      │
│  6-model ML pipeline:                                        │
│    ExerciseNet · FormNet · RepCount                          │
│    RecoveryLSTM · AdherenceRF · AnomalyIF                    │
│  Therapist dashboard · OTA · Clinical reports                │
└──────────────────────────────┬───────────────────────────────┘
                               │ MQTT / HTTPS
┌──────────────────────────────┴───────────────────────────────┐
│                      REHAB HUB (ESP32-S3)                     │
│  BLE 5.0 BAN Coordinator · SX1262 Sub-GHz                    │
│  Wi-Fi 2.4 GHz · TFLite-Micro edge inference                  │
│  3.5" TFT · I²S Speaker · DRV2605L Haptic                    │
│  OV5640 Camera (pose backup) · 2000 mAh LiPo                 │
└──────┬──────────┬──────────┬──────────┬──────────────────────┘
       │ BLE 5.0  │ BLE 5.0  │ BLE 5.0  │ Sub-GHz 868 MHz
       │ BAN      │ BAN      │ BAN      │
┌──────┴────┐ ┌───┴──────┐ ┌─┴────────┐ ┌─┴────────────┐
│ BODY      │ │ SMART    │ │ BODY     │ │ PRESSURE     │
│ SENSOR ×N │ │ BAND     │ │ SENSOR   │ │ MAT          │
│ nRF52840  │ │ nRF52840 │ │ nRF52840 │ │ ESP32-S3     │
│ LSM6DSO   │ │ HX711    │ │ LSM6DSO  │ │ 256 FSR      │
│ LIS3MDL   │ │ Load Cell│ │ LIS3MDL  │ │ ADS1115      │
│ CR2032    │ │ LiPo     │ │ CR2032   │ │ USB-C        │
└───────────┘ └──────────┘ └──────────┘ └──────────────┘
```

## 3. Communication Layers

### Layer 1: Body Area Network (BLE 5.0)
- **Topology:** Star (Hub = central, sensors = peripherals)
- **PHY:** BLE 5.0 coded PHY (125 kHz) for range, 2M PHY for throughput
- **Connection interval:** 10 ms (low-latency real-time feedback)
- **Throughput:** ~20 kB/s per sensor (100 Hz × 20 bytes = 2 kB/s, well within capacity)
- **Max nodes:** 7 concurrent (6 Body Sensors + 1 Smart Band)
- **Security:** LE Secure Connections (ECDH P-256), AES-128-CCM
- **Pairing:** NFC tap-to-pair (Body Sensors), button pair (Smart Band)

### Layer 2: Sub-GHz Link (868 MHz LoRa)
- **Topology:** Point-to-point (Pressure Mat → Hub) or TDMA mesh
- **Modulation:** LoRa, SF7, BW 250 kHz, +22 dBm
- **Range:** 200+ m line-of-sight, 50+ m indoor
- **Payload:** Pressure mat frame (512 bytes → 200 bytes compressed)
- **TDMA:** 2-second slots, 16-slot frame

### Layer 3: Cloud Link (Wi-Fi / MQTT)
- **Hub → Cloud:** Wi-Fi 2.4 GHz, MQTT over TLS
- **Data rate:** ~50 kB/s during active session
- **Offline buffer:** microSD (30+ days session data)

## 4. Data Processing Pipeline

```
Body Sensors (100 Hz IMU)
    │
    ▼
Hub: Madgwick AHRS → Quaternion per segment
    │
    ▼
Hub: Relative quaternion → Joint angles (±2°)
    │
    ├──→ ExerciseNet (1s window, 30-class, <80ms)
    ├──→ FormNet (2s window, score + deviation, <50ms)
    ├──→ RepCount (state machine, <5ms)
    │
    ▼
Hub: Real-time feedback (audio + haptic + display)
    │
    ▼
Cloud: Full 6-model pipeline
    ├──→ RecoveryLSTM (8-week milestone forecast)
    ├──→ AdherenceRF (7-day dropout risk)
    ├──→ AnomalyIF (compensation pattern detection)
    └──→ ExerciseNet-v2 / FormNet-v2 (cloud refinement)
```

## 5. Sensor Fusion

### Madgwick AHRS Filter
- **Input:** 9-DoF (accel 3-axis + gyro 3-axis + mag 3-axis) at 100 Hz
- **Output:** Quaternion (q0, q1, q2, q3) per body segment
- **Beta parameter:** 0.1 (gradient descent step size)
- **Accuracy:** ±2° static, ±5° dynamic

### Joint Angle Derivation
- **Method:** Relative quaternion between adjacent segments
- **q_rel = q_proximal^-1 × q_distal**
- **Angle = 2 × acos(|q_rel.w|) × (180/π)**
- **Example:** Knee angle = angle between thigh sensor and shin sensor

## 6. Edge ML (TFLite-Micro on ESP32-S3)

### ExerciseNet
- **Architecture:** 1D-CNN (6 conv layers, BatchNorm, ReLU, MaxPool)
- **Input:** 100 × 9 (1 second @ 100 Hz, 9 IMU channels)
- **Output:** 30-class softmax
- **Size:** 180 KB (quantized int8)
- **Latency:** <80 ms on ESP32-S3 @ 240 MHz

### FormNet
- **Architecture:** Temporal CNN (4 dilated conv layers)
- **Input:** 200 × 18 (2 seconds @ 100 Hz, 18 features)
- **Output:** Form score (0-100) + deviation type (6-class)
- **Size:** 95 KB (quantized int8)
- **Latency:** <50 ms

### RepCount
- **Architecture:** Peak detection + state machine
- **Input:** Joint angle + force stream
- **Output:** Rep count increment
- **Size:** 12 KB (C code)
- **Latency:** <5 ms

## 7. Power Management

| Node | Battery | Active Current | Sleep Current | Battery Life |
|------|---------|---------------|---------------|--------------|
| Hub | 2000 mAh LiPo | ~200 mA | ~5 mA | 8h active / 72h standby |
| Body Sensor | CR2032 220 mAh | ~5.5 mA | ~2 μA | 30 days (1h/day) |
| Smart Band | 300 mAh LiPo | ~8 mA | ~5 μA | 7 days (1h/day) |
| Pressure Mat | USB-C (mains) | N/A | N/A | Continuous |

## 8. Security

- **BLE:** LE Secure Connections (ECDH P-256), AES-128-CCM encryption
- **Sub-GHz:** AES-128-CTR encryption, CRC-16-CCITT integrity
- **Cloud:** TLS 1.3 for MQTT/HTTPS, AES-256 at rest
- **HIPAA:** Encrypted PHI, patient data isolation, audit logging
- **OTA:** Signed firmware images, rollback support
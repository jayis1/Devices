# TremorSync Architecture

## Overview

TremorSync is a multi-node hardware+software system for continuous Parkinson's disease (PD) motor symptom monitoring, medication optimization, and fall prevention.

## Design Principles

1. **Multi-modal sensing** — No single sensor can capture all PD motor symptoms. Tremor (IMU), gait (IMU+FSR), speech (microphone), and swallowing (EMG) each require dedicated hardware.
2. **Edge-first inference** — TremorNet and SpeechNet run on-device (ESP32-S3 tflite-micro) to minimize latency for real-time alerts (FOG cueing, fall detection).
3. **Pharmacokinetic personalization** — Each patient's levodopa response is unique. The system learns individual ON/OFF curves over 7 days and optimizes dose timing.
4. **Clinical alignment** — All metrics map to MDS-UPDRS (Movement Disorder Society Unified Parkinson's Disease Rating Scale), the gold-standard clinical assessment.
5. **Safety-first** — Fall detection with 4G LTE emergency dispatch, medication safety interlocks, aspiration risk monitoring.

## Data Flow

```
Sensors → Node MCU → Radio (BLE/Sub-GHz) → Hub → MQTT → Cloud → ML Pipeline → API → Mobile App
                                                         ↓
                                                    PostgreSQL
```

### Real-time path (< 1 second)
- IMU 200 Hz → Tremor Band → BLE → Hub → FOG detection → Haptic cueing
- ADXL362 fall → Hub → 4G LTE → 911 + caregiver SMS

### Near-real-time (1–10 seconds)
- Gait analysis → Sub-GHz → Hub → MQTT → Cloud → Fall risk update
- Speech analysis → BLE → Hub → MQTT → Cloud → Hypophonia score

### Batch (daily/weekly)
- 90-day progression → Cloud ML → MDS-UPDRS estimate → Clinical report
- 14-day gait → Fall risk LSTM → 30-day forecast

## Radio Architecture

### Sub-GHz 868 MHz TDMA Mesh (Hub-coordinated)
- **Nodes:** Gait Pod, Med Station
- **Range:** 200 m indoor, 2 km line-of-sight
- **Penetration:** Excellent (passes through walls/floors — critical for home use)
- **TDMA:** 1-second superframe, 50 ms slots, eliminates collisions
- **Encryption:** AES-128

### BLE 5.0 Star (Hub as central)
- **Nodes:** Tremor Band, Voice Node
- **Range:** 10 m (body-worn, close to Hub)
- **Power:** Lower power than Sub-GHz for wearables
- **GATT:** Custom TremorSync service with 10 characteristics
- **Encryption:** BLE 5.0 LE Secure Connections

## Power Budget

| Node | Battery | Life | Duty Cycle |
|------|---------|------|------------|
| Hub | 18650 LiFePO4 1500mAh + USB-C | 8+ hr backup | Always-on |
| Tremor Band | 402030 LiPo 400mAh | 5 days | IMU 200 Hz continuous, BLE notify 2.56s |
| Gait Pod | CR2477 1000mAh | 14 days | IMU 100 Hz, 10s window/30s, deep sleep |
| Voice Node | 302035 LiPo 300mAh | 7 days | I²S continuous, BLE notify 1s |
| Med Station | 18650 + USB-C | 24+ hr backup | Always-on (dispenser) |

## ML Model Deployment

| Model | Where | Framework | Latency |
|-------|-------|-----------|---------|
| TremorNet | Hub (ESP32-S3) | tflite-micro int8 | 10 ms |
| SpeechNet | Voice Node (ESP32-S3) | tflite-micro int8 | 45 ms |
| FreezeNet | Cloud | ONNX | 50 ms |
| MedResponse | Cloud | XGBoost | 5 ms |
| FallRisk | Cloud | ONNX | 30 ms |
| ProgressionNet | Cloud | ONNX | 100 ms |
| BradykinesiaNet | Cloud | ONNX | 20 ms |

## Security

- **Transport:** TLS 1.3 (MQTT), AES-128 (Sub-GHz mesh), BLE 5.0 encryption
- **Data at rest:** PostgreSQL with encryption
- **Privacy:** No raw audio leaves Voice Node (only extracted features)
- **HIPAA:** Clinical reports use de-identified data format
- **Emergency:** 4G LTE backup for fall/FOG dispatch when Wi-Fi is down
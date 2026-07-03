# DriveSync — Architecture Document

## System Overview

DriveSync is a 4-node driving safety system for drowsiness and distraction prevention:

```
     Steering Wheel (BLE 5.0)     Seat Belt Tag (BLE 5.0)     OBD-II (BLE 5.0)
     ┌──────────┐                 ┌──────────┐                 ┌──────────┐
     │ Wheel    │                 │ Belt Tag │                 │ OBD-II   │
     │ nRF52840 │                 │ nRF52840 │                 │ RP2040   │
     └────┬─────┘                 └────┬─────┘                 └────┬─────┘
          │                            │                            │
          ▼                            ▼                            ▼
     ┌────────────────────────────────────────────────────────────────┐
     │                      DriveSync Dash Hub                        │
     │   ESP32-S3 · BLE 5.0 · Wi-Fi · OV5640 + 940nm IR              │
     │   Edge ML (PERCLOS + Head-Pose) · Risk Fusion Engine           │
     │   Speaker · Haptic · 18650 UPS                                 │
     └────────┬───────────────────────────────────────────────────────┘
              │ Wi-Fi hotspot / 4G LTE
              ▼
     ┌──────────────────┐
     │ DriveSync Cloud  │
     │ FastAPI + MQTT   │
     │ TimescaleDB      │
     │ Trip Scoring     │
     │ Coaching Reports │
     └──────────────────┘
```

## Node Roles

| Node | Role | SoC | Power | Comm |
|------|------|-----|-------|------|
| Dash Hub | Central compute, camera, edge ML, fusion, alerts, cloud | ESP32-S3 | Vehicle USB + 18650 UPS | BLE 5.0 central, Wi-Fi |
| Steering Wheel Node | Steering IMU (micro-jerk), capacitive grip, haptic | nRF52840 | CR2477 (6 months) | BLE 5.0 peripheral |
| Seat Belt Tag | PPG HRV, body sway, chest haptic | nRF52840 | CR2477 (4 months) | BLE 5.0 peripheral |
| OBD-II Dongle | Vehicle telemetry (speed, RPM, throttle, load) | RP2040 | Vehicle battery | BLE 5.0 (via nRF52832) |

## Data Flow

1. **Camera (Hub):** OV5640 + 940nm IR → 10 FPS grayscale → tflite-micro eye-closure CNN → PERCLOS + blink rate → head-pose CNN → pitch/yaw/roll + head-bob count
2. **Steering (Wheel → Hub):** LSM6DSO at 1 kHz (FIFO) → angular velocity reversals (jerk count) + FDC2214 grip → 10 Hz BLE payload
3. **Physiology (Belt → Hub):** MAX30101 PPG at 25 Hz → peak detection → RR intervals → HRV (RMSSD, pNN50) → 1 Hz BLE payload; LSM6DSO at 50 Hz → body sway amplitude → 10 Hz BLE payload
4. **Vehicle (OBD → Hub):** MCP2515 CAN → OBD-II PID queries (speed, RPM, throttle, load) → 10 Hz BLE payload
5. **Fusion (Hub):** Weighted fusion of PERCLOS (35%), steering (25%), HRV (20%), body sway (15%), OBD context (5%) → 0-100 risk score every 5 seconds
6. **Alerts (Hub → nodes):** Risk > 30 → wheel haptic; > 50 → audio + voice; > 70 → alarm + belt haptic; > 85 → critical alarm + emergency contact SMS
7. **Cloud (Hub → Cloud):** Risk timeline + events uploaded post-trip → trip safety score → weekly coaching report

## ML Pipeline

| Model | Input | Output | Training Data | Edge Target |
|-------|-------|--------|---------------|-------------|
| Eye-Closure CNN | 48×48 eye crop | open/closed | NTHU-DDD | ESP32-S3 (2 KB INT8) |
| Head-Pose CNN | 64×64 face crop | pitch/yaw/roll | AFLW2000 | ESP32-S3 (8 KB INT8) |
| Steering XGBoost | 6 features (30s window) | drowsy prob | UAH-DriveSet | Cloud |
| HRV LSTM | 5-min HRV series | drowsy prob | Sleep deprivation | Cloud |
| Risk Fusion (LightGBM) | 10 features | 0-100 score | Driving study | Cloud + simplified edge |

## Communication Topology

- **BLE 5.0** for all inter-node communication (star topology, Hub as central)
- **Wi-Fi** for Hub → Cloud (via phone hotspot or optional 4G LTE module)
- **MQTT** for cloud messaging
- All BLE payloads use the DriveSync binary protocol (11-byte header + payload, XOR checksum)
- Optional AES-128-CTR encryption for sensitive payloads

## Power Budget

| Node | Active Power | Battery Life |
|------|-------------|--------------|
| Dash Hub | ~600 mW | Vehicle-powered + 4h UPS |
| Wheel Node | ~2.4 mW avg | 6 months (CR2477) |
| Belt Tag | ~3.6 mW avg | 4 months (CR2477) |
| OBD-II | ~150 mW | Vehicle-powered |
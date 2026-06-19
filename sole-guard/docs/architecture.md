# SoleGuard Architecture

## Overview

SoleGuard is a 4-node wearable + ambient system for diabetic foot ulcer prevention. It continuously monitors plantar pressure, skin-temperature asymmetry, gait, edema, and wound appearance to predict ulceration 1–3 weeks ahead and trigger offloading alerts before tissue breaks down.

## Nodes

| Node | SoC | Role | Comms | Power |
|------|-----|------|-------|-------|
| Hub Node | RP2040 + ESP32-C6 + nRF52840 | Edge ML (ulcer-risk), TFT heat map, voice alerts, cloud bridge | BLE mesh + WiFi6 | USB-C + LiPo backup |
| Smart Insole ×2 | nRF52840 | 24-point pressure + 8-point temp + IMU gait | BLE mesh | LiPo 350mAh (14–18h, Qi) |
| Ankle Tag | nRF52840 | IMU gait + bioimpedance edema + fall detection + mesh relay | BLE mesh | LiPo 180mAh (5–7d, Qi) |
| Foot Scanner | ESP32-S3 | Multispectral foot imaging + wound detection ML + weight | WiFi6 | USB-C mains |

## Data Flow

```
Insoles (pressure+temp+gait) ──┐
                               ├── BLE mesh ── Hub ── WiFi6 ── MQTT ── Cloud
Ankle Tag (gait+edema+fall) ───┘                 │                  │
                                                 ├── BLE ── Mobile App
                                                 │                  │
Foot Scanner (wound images) ─── WiFi6 ───────────────────────────────┘
```

## Edge ML vs Cloud ML

| Model | Where | Framework | Size |
|-------|-------|-----------|------|
| Ulcer-risk (CNN-LSTM) | Hub (RP2040, TFLite Micro) | int8 quantized | <80KB |
| Wound detection (MobileNetV3) | Scanner (ESP32-S3, TFLite) | int8 quantized | ~2.5MB |
| Gait decline (GRU) | Cloud | PyTorch | — |
| Personal baseline | Cloud (hub cache) | NumPy stats | JSON |

The ulcer-risk model runs on-hub every 5 minutes for low-latency alerts without WiFi. The wound model runs on-scanner for instant flag at the moment of imaging. Cloud handles training, gait-trend analysis, clinician portal, and image storage.

## Fail-Safe Design

- Temperature asymmetry alert (>2.2°C) is a **hard clinical rule** independent of the ML model — an ML failure cannot suppress it.
- If BLE mesh is down, insoles buffer 48h locally; hub's last risk score stays on TFT.
- Scanner works fully offline and queues images for upload.
- All ML alerts are advisory; the system never automates treatment.

## Security & Privacy

- BLE mesh uses SIG Mesh encryption (AES-128-CCM).
- Cloud: TLS for MQTT/HTTPS, encryption at rest (PostgreSQL + S3).
- HIPAA-compliant: patient consents to caregiver/clinician data sharing.
- Foot images are medical data — stored in private S3, access-controlled.
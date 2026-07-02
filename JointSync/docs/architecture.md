# JointSync — Architecture Document

## System Overview

JointSync is a 4-node wearable health system for arthritis management:

```
     Joint Tags (BLE 5.0)          Joint Scanner (BLE 5.0)
     ┌──┐ ┌──┐ ┌──┐ ┌──┐          ┌──────────┐
     │T1│ │T2│ │T3│ │T4│          │ Scanner  │
     └─┬┘ └─┬┘ └─┬┘ └─┬┘          └────┬─────┘
       │    │    │    │                │
       ▼    ▼    ▼    ▼                ▼
    ┌─────────────────────────────────────┐
    │           JointSync Hub              │
    │   ESP32-S3 · BLE · Sub-GHz · Wi-Fi  │
    │   Edge ML · E-Paper Display         │
    └──────┬──────────────────┬───────────┘
           │ Sub-GHz 868 MHz   │ Wi-Fi
           ▼                   ▼
    ┌──────────────┐    ┌──────────────┐
    │ Compression  │    │ JointSync    │
    │ Sleeve       │    │ Cloud (MQTT) │
    └──────────────┘    └──────────────┘
```

## Node Roles

| Node | Role | SoC | Power | Comm |
|------|------|-----|-------|------|
| Hub | Coordinator, edge ML, display, cloud relay | ESP32-S3 | USB + 18650 backup | BLE + Sub-GHz + Wi-Fi |
| Joint Tag ×N | Joint angle, skin temp, PPG | nRF52840 | CR2477 (45 days) | BLE 5.0 |
| Compression Sleeve | Adaptive pneumatic compression (20-40 mmHg) | ESP32-S3-MINI | LiPo 500 mAh | Sub-GHz 868 MHz |
| Joint Scanner | Thermal + multispectral imaging | ESP32-S3 | LiPo 1200 mAh | BLE 5.0 |

## Data Flow

1. **Tags → Hub (BLE 5.0):** IMU (100 Hz), temperature (30 s), PPG (25 Hz)
2. **Hub (Edge ML):** Joint angle via Madgwick AHRS, inflammation detection via tflite-micro
3. **Hub → Sleeve (Sub-GHz):** Therapy commands (mode, pressure, duration)
4. **Sleeve → Hub (Sub-GHz):** Pressure readings, pump state
5. **Scanner → Hub (BLE):** Thermal chunks (32×24), multispectral image metadata
6. **Hub → Cloud (Wi-Fi/MQTT):** Aggregated metrics, alerts
7. **Cloud → App:** REST API for dashboards, WebSocket for real-time alerts

## ML Pipeline

| Model | Location | Input | Output |
|-------|----------|-------|--------|
| Madgwick AHRS | Tag (C) | IMU 6-DoF | Quaternion, ROM |
| Inflammation Detector | Hub (tflite-micro) | Temp delta, ROM, HRV | Probability 0-1 |
| Activity CNN | Tag (tflite-micro) | 3-sec IMU window | 6-class activity |
| Flare LSTM | Cloud (PyTorch/ONNX) | 7-day features | 7-day flare probability |
| Gait XGBoost | Cloud | Bilateral IMU features | Loading asymmetry |
| Thermal Classifier | Cloud (TFLite) | 32×24 thermal map | Swelling grade 0-3 |

## Communication Protocol

- Binary packet format: 11-byte header + 0-245 byte payload
- Sync bytes: 0x4A, 0x53 ("JS")
- XOR checksum
- AES-CCM-128 optional encryption
- BLE GATT service UUID 0x4A53
- Sub-GHz: 868 MHz, 2-FSK, 50 kbps, TDMA (8 × 50 ms slots)

## Clinical Validity

- Bilateral temperature delta >2.2°C = clinically significant inflammation (ACR/EULAR)
- ROM decline correlates with disease progression (OARSI)
- HRV decrease correlates with pain and stress (clinical literature)
- Morning stiffness duration = ACR flare criterion
- Compression 20-40 mmHg = standard medical compression class (RAL-GZ 387)
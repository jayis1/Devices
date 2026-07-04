# GlucoSync — Architecture

## System Overview

GlucoSync is a 4-node hardware + software system for AI-powered glucose management.

```
┌─────────────────────────────────────────────────────────┐
│                    GlucoSync Cloud                       │
│         FastAPI + MQTT + TimescaleDB + ML                │
│   AGP reports · Insulin sensitivity · Clinical exports   │
└────────────────────────▲────────────────────────────────┘
                         │ Wi-Fi / 4G LTE (MQTT/JSON)
┌────────────────────────┴────────────────────────────────┐
│                  Metabolic Hub (ESP32-S3)               │
│  E-ink display · CGM BLE bridge · 30-min forecast LSTM  │
│  Hypo warning ensemble · Progressive alerts · IOB/COB   │
└──▲──────▲──────▲────────────────────────────────────────┘
   │BLE   │BLE   │BLE
   │      │      │
┌──┴──┐ ┌─┴──┐ ┌─┴─────┐
│Meal │ │Act.│ │Insulin│
│Scan │ │Band│ │Pen Tag│
│ESP32│ │nRF │ │nRF    │
│S3   │ │52840││52840  │
└─────┘ └────┘ └───────┘
```

## Node Roles

| Node | SoC | Role | Power | Battery |
|------|-----|------|-------|---------|
| Metabolic Hub | ESP32-S3 | Central coordinator, CGM bridge, edge ML, display, alerts | USB-C + 18650 UPS | 26h backup |
| Meal Scanner | ESP32-S3 | Multispectral food imaging, on-device CNN | 500mAh LiPo | ~200 scans |
| Activity Band | nRF52840 | PPG heart rate + IMU activity classification | CR2477 | ~90 days |
| Insulin Pen Tag | nRF52840 | IMU injection detection + logging | CR2477 | ~180 days |

## Data Flow

1. CGM → Hub (BLE GATT): glucose at 1/min
2. Meal Scanner → Hub (BLE): carb estimates on-demand
3. Activity Band → Hub (BLE): HR + activity at 1 Hz
4. Pen Tag → Hub (BLE): injection events on-demand
5. Hub runs glucose forecast LSTM every 5 min
6. Hub runs hypo warning ensemble every 5 min
7. Hub triggers progressive alerts (display → haptic → audio → phone)
8. Hub → Cloud (MQTT): aggregated data + forecast
9. Cloud runs long-term ML (insulin sensitivity, AGP, reports)
10. Mobile App ← Cloud (REST/WebSocket): real-time + analytics

## Edge vs Cloud ML

| Model | Location | Why |
|-------|----------|-----|
| Glucose forecast LSTM | Hub (ESP32-S3) | Real-time, must work offline |
| Hypo warning ensemble | Hub (ESP32-S3) | Safety-critical, must work offline |
| Food carb CNN | Meal Scanner (ESP32-S3) | On-device, images never leave |
| Insulin sensitivity XGBoost | Cloud | Needs 14-day history, personalization |
| Activity-glucose response | Cloud | Bayesian update from long-term data |
| Risk fusion | Hub (ESP32-S3) | Real-time alert decisions |

## Communication

| Link | Protocol | Frequency | Payload Size |
|------|----------|-----------|-------------|
| CGM → Hub | BLE 5.0 GATT | 1/min | 12 bytes |
| Scanner → Hub | BLE 5.0 | On-demand | 12 bytes |
| Band → Hub | BLE 5.0 | 1 Hz | 8 bytes |
| Pen Tag → Hub | BLE 5.0 | Event-driven | 10 bytes |
| Hub → Cloud | Wi-Fi/MQTT | Every 5 min | ~200 bytes |

## Privacy

- Glucose data: encrypted BLE (AES-128-CTR), TLS to cloud
- Meal images: processed on ESP32-S3, never stored or transmitted (only carb estimates sent)
- Cloud storage: encrypted at rest, HIPAA-aware design
- No data sold to insurers or advertisers
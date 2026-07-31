# BloomSync — Architecture Document

## 1. System Overview

BloomSync is a multi-node IoT system for postpartum maternal health monitoring. It combines a wrist-worn recovery band (vital signs), an adhesive nursing sensor (breastfeeding + mastitis detection), an adhesive wound patch (infection monitoring), an AI hub, cloud ML, and a mobile app into a complete postpartum care platform.

## 2. Node Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                        CLOUD BACKEND                         │
│  FastAPI + MQTT + InfluxDB + PostgreSQL                      │
│  6-model ML pipeline:                                        │
│    HemorrhageRisk · PPDetect · WoundInfect                   │
│    MastitisDetect · PreeclampsiaRF · RecoveryLSTM           │
│  OB/GYN dashboard · OTA · Clinical reports                   │
└──────────────────────────────┬───────────────────────────────┘
                               │ MQTT / HTTPS
┌──────────────────────────────┴───────────────────────────────┐
│                      BLOOM HUB (ESP32-S3)                     │
│  BLE 5.0 WAN Coordinator · Wi-Fi 2.4 GHz                     │
│  TFLite-Micro edge inference (hemorrhage/wound/mastitis)     │
│  3.5" TFT · I²S Speaker · I²S Mic (voice prosody)           │
│  DRV2605L Haptic · 2000 mAh LiPo                             │
└──────┬──────────┬──────────┬──────────────────────────────────┘
       │ BLE 5.0  │ BLE 5.0  │ BLE 5.0
       │ WAN      │ WAN      │ WAN
┌──────┴────┐ ┌───┴──────┐ ┌─┴──────────────┐
│ RECOVERY  │ │ NURSING  │ │ WOUND          │
│ BAND      │ │ SENSOR   │ │ PATCH          │
│ nRF52840  │ │ nRF52840 │ │ nRF52840       │
│ MAX30101  │ │ TMP117×2 │ │ TMP117         │
│ LSM6DSO   │ │ LIS2DW12 │ │ FDC2214        │
│ TMP117    │ │ CR2032   │ │ LMP91200 pH    │
│ LiPo 200  │ │ 14-day   │ │ CR2032 21-day  │
└───────────┘ └──────────┘ └────────────────┘
```

## 3. Communication Layers

### Layer 1: Body Area Network (BLE 5.0)
- **Topology:** Star (Hub = central, wearables = peripherals)
- **PHY:** BLE 5.0, 2M PHY for throughput, coded PHY for range
- **Connection interval:** 20 ms (balanced for battery + latency)
- **Throughput:** Recovery Band ~80 B/s, Nursing Sensor ~25 B/s, Wound Patch ~5 B/s
- **Max nodes:** 7 concurrent (3 Band + 2 Nursing + 2 Wound for bilateral)
- **Security:** LE Secure Connections (ECDH P-256), AES-128-CCM
- **Range:** 10-15 m typical home range

### Layer 2: Cloud Link (Wi-Fi / MQTT)
- **Hub → Cloud:** Wi-Fi 2.4 GHz, MQTT over TLS
- **Data rate:** ~5 kB/s average
- **Offline buffer:** microSD (30+ days of recovery data)
- **QoS:** MQTT QoS 1 for alerts, QoS 0 for routine telemetry

### Layer 3: Cellular Backup (4G LTE, optional)
- **Module:** SIM7600G (optional add-on)
- **Use case:** Emergency alert dispatch when Wi-Fi unavailable

## 4. Edge vs Cloud Inference

| Model | Location | Rationale |
|-------|----------|-----------|
| HemorrhageRisk (screening) | Edge (Hub ESP32-S3) | Life-critical, must work offline, <30s latency |
| WoundInfect (screening) | Edge (Hub ESP32-S3) | Continuous monitoring, low-bandwidth input |
| MastitisDetect | Edge (Hub ESP32-S3) | Simple bilateral temp comparison, low compute |
| PreeclampsiaRF | Cloud | Requires longer time-series + multi-feature fusion |
| PPDetect | Cloud | Voice prosody CNN, higher compute, 3×/day batch |
| RecoveryLSTM | Cloud | Long-horizon multi-modal forecast, daily batch |

## 5. Data Storage

- **Edge (Hub microSD):** Raw vitals, nursing, wound data buffered for 30+ days
- **Cloud (InfluxDB):** Time-series vitals, nursing, wound data (90-day retention)
- **Cloud (PostgreSQL):** Patient profiles, risk assessments, alerts, reports
- **Cloud (S3):** Voice prosody features (128 bytes/sample, not raw audio)

## 6. Security

- **Wireless:** AES-128-CTR on BLE, TLS 1.3 on Wi-Fi/MQTT
- **At rest:** PostgreSQL TDE, InfluxDB encryption
- **Access control:** JWT-based API auth, RBAC (patient/partner/obstetrician/admin)
- **Audit:** All data access logged
- **HIPAA/GDPR:** BAA with cloud provider, data residency configurable

## 7. Power Budget

| Node | Avg Current | Battery | Life |
|------|------------|---------|------|
| Bloom Hub | ~50 mA (active) | 2000 mAh LiPo | 8h (USB-C unlimited) |
| Recovery Band | ~1.2 mA avg | 200 mAh LiPo | 7 days |
| Nursing Sensor | ~0.65 mA avg | 220 mAh CR2032 | 14 days |
| Wound Patch | ~0.44 mA avg | 220 mAh CR2032 | 21 days |
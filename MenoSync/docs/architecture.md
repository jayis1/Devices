# MenoSync — Architecture Document

## 1. System Overview

MenoSync is a multi-node IoT system for menopause management. It combines a wrist-worn band (skin temperature + EDA stress + PPG vitals), an under-mattress bed mat (BCG sleep staging + night sweat detection), room-mounted climate nodes (ambient sensing + HVAC/shade control for pre-emptive cooling), an AI hub, cloud ML, and a mobile app into a complete menopause management platform.

## 2. Node Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                        CLOUD BACKEND                         │
│  FastAPI + MQTT + InfluxDB + PostgreSQL                      │
│  6-model ML pipeline:                                        │
│    HotFlashNet · NightSweatDetect · SleepQuality             │
│    MoodStress · BoneRisk · CoolingOptimizer (DQN)           │
│  Gynecologist dashboard · OTA · Clinical reports             │
└──────────────────────────────┬───────────────────────────────┘
                               │ MQTT / HTTPS
┌──────────────────────────────┴───────────────────────────────┐
│                      MENO HUB (ESP32-S3)                     │
│  BLE 5.0 WAN Coordinator · Wi-Fi 2.4 GHz                     │
│  Sub-GHz 868 MHz TDMA Mesh Coordinator                      │
│  TFLite-Micro edge inference (hot flash/night sweat)         │
│  3.5" TFT · I²S Speaker · I²S Mic (voice prosody)           │
│  DRV2605L Haptic · 2000 mAh LiPo                             │
└──────┬──────────┬──────────┬──────────────────────────────────┘
       │ BLE 5.0  │ BLE 5.0  │ Sub-GHz 868 MHz
       │ WAN      │ WAN      │ TDMA Mesh
┌──────┴────┐ ┌───┴──────┐ ┌─┴────────────────────────────────┐
│ WRIST     │ │ BED MAT  │ │ CLIMATE NODE×N                   │
│ BAND      │ │ nRF52840 │ │ ESP32-C3 + RFM69HCW 868 MHz     │
│ nRF52840  │ │ PVDF BCG │ │ BME280 ambient                   │
│ MAX30101  │ │ FDC2214  │ │ MLX90640 32×24 radiant IR        │
│ TMP117    │ │ TMP117   │ │ 2× relay (HVAC + shade)          │
│ ADS1292   │ │ CR2032   │ │ USB-C or solar powered           │
│ LSM6DSO   │ │ 180-day  │ │ 1 per room                       │
│ LiPo 200  │ │ Under    │ │ Wall/ceiling mount               │
│ 7-day     │ │ mattress │ │                                  │
└───────────┘ └──────────┘ └───────────────────────────────────┘
```

## 3. Communication Layers

### Layer 1: Body Area Network (BLE 5.0)
- **Topology:** Star (Hub = central, Wrist Band + Bed Mat = peripherals)
- **PHY:** BLE 5.0, 2M PHY for throughput, coded PHY for range (Bed Mat through mattress)
- **Connection interval:** 20 ms
- **Throughput:** Wrist Band ~100 B/s, Bed Mat ~20 B/s
- **Max nodes:** 7 concurrent
- **Security:** LE Secure Connections (ECDH P-256), AES-128-CCM
- **Range:** 10-15 m typical home range

### Layer 2: Home Mesh (Sub-GHz 868 MHz)
- **Topology:** TDMA mesh (Hub = coordinator, Climate Nodes = end nodes)
- **PHY:** RFM69HCW, FSK modulation, 868 MHz ISM band
- **Data rate:** 100 kbps
- **Range:** 100-500 m (penetrates walls, whole-home coverage)
- **Max nodes:** 32 Climate Nodes per Hub
- **TDMA slots:** 16 time slots, 500 ms per slot, 8 s superframe
- **Security:** AES-128 hardware encryption (RFM69)
- **Latency:** Cooling commands dispatched within 1 superframe (8 s max)

### Layer 3: Cloud Link (Wi-Fi / MQTT)
- **Hub → Cloud:** Wi-Fi 2.4 GHz, MQTT over TLS
- **Data rate:** ~3 kB/s average
- **Offline buffer:** microSD (60+ days of menopause tracking data)
- **QoS:** MQTT QoS 1 for alerts, QoS 0 for routine telemetry

## 4. Edge vs Cloud Inference

| Model | Location | Rationale |
|-------|----------|-----------|
| HotFlashNet (screening) | Edge (Hub ESP32-S3) | Real-time, must work offline, cooling requires low latency |
| NightSweatDetect (screening) | Edge (Hub ESP32-S3) | Continuous overnight monitoring, low-bandwidth input |
| SleepQuality | Cloud | 7-night multi-modal forecast, daily batch |
| MoodStress | Cloud | Voice prosody CNN, higher compute, 2×/day batch |
| BoneRisk | Cloud | 30-day aggregate, weekly batch |
| CoolingOptimizer (DQN) | Cloud (trains) + Edge (infers) | DQN trained in cloud, policy deployed to Hub for real-time action |

## 5. Data Storage

- **Edge (Hub microSD):** Raw vitals, EDA, BCG, sweat, ambient data buffered for 60+ days
- **Cloud (InfluxDB):** Time-series vitals, EDA, sleep, sweat, ambient data (180-day retention)
- **Cloud (PostgreSQL):** Patient profiles, risk assessments, alerts, reports, treatment response
- **Cloud (S3):** Voice prosody features (128 bytes/sample, not raw audio)

## 6. Security

- **Wireless:** AES-128-CTR on BLE, AES-128 hardware on Sub-GHz (RFM69), TLS 1.3 on Wi-Fi/MQTT
- **At rest:** PostgreSQL TDE, InfluxDB encryption
- **Access control:** JWT-based API auth, RBAC (patient/gynecologist/admin)
- **Audit:** All data access logged
- **HIPAA/GDPR:** BAA with cloud provider, data residency configurable

## 7. Power Budget

| Node | Avg Current | Battery | Life |
|------|------------|---------|------|
| Meno Hub | ~50 mA (active) | 2000 mAh LiPo | 8h (USB-C unlimited) |
| Wrist Band | ~1.2 mA avg | 200 mAh LiPo | 7 days |
| Bed Mat | ~0.51 µA avg | 220 mAh CR2032 | 180 days |
| Climate Node | ~15 mA avg | USB-C / solar | Unlimited |
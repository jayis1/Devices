# SkinSync Architecture

## System Overview

SkinSync is a 4-node ambient + wearable system for personal skin health and sun safety:

1. **Mirror Hub** — RP2040 + ESP32-C6 + nRF52832/SX1262. Bathroom-mirror-mounted coordinator. Bridges Sub-GHz mesh to WiFi/cloud, runs edge UV-dose calculation, displays morning skincare dashboard, triggers dispensing.

2. **UV Patch** — nRF52832 + SX1262 + VEML6075 + TMP117 + LTR390. Wrist/shoulder wearable. Measures UVA/UVB dose, skin temp continuously. 14-day coin-cell. Haptic alerts at 50/70/90% MED.

3. **Skin Scanner** — ESP32-S3 + OV5640 + 4-mode LED ring. Handheld multispectral skin imager. On-device condition CNN + ABCDE melanoma pre-screen. Lesion tracking with IMU-guided angle reproduction.

4. **Smart Dispenser** — ESP32-C6 + SX1262 + 4× peristaltic pumps + HX711 load cells. Countertop unit. Dispenses exact skincare product amounts. RFID cartridge identification. Inventory tracking + auto-reorder.

## Data Flow

```
UV Patch ──Sub-GHz──► Mirror Hub ──WiFi/MQTT──► Cloud (FastAPI + PostgreSQL)
                         │                         │
                    BLE to Mobile           ML Pipeline:
                         │                   - Condition CNN
                    Push alerts              - ABCDE Detector
                                                - UV Risk Model
Scanner ──WiFi──► Cloud (images)              - Routine Optimizer
                    │                          - Skin Age Model
                    └──► Hub (notification)   │
                                        WebSocket alerts
Dispenser ←──Sub-GHz──◄ Hub ←──MQTT──◄ Cloud   │
   │                                    Mobile App
   └──► Load cell inventory ──► Hub ──► Cloud   │
                                          Push notifications
```

## Communication Layers

### Layer 1: Sub-GHz Mesh (868/915 MHz)
- **Technology:** SX1262 LoRa-style modulation, SF7, BW=125kHz
- **Nodes:** UV Patches ↔ Hub ↔ Dispenser
- **Range:** 50m indoor, 200m line-of-sight
- **Power:** Coin-cell compatible (UV Patch), 14µA sleep
- **Protocol:** Binary, CRC16-CCITT, 8-18 byte payloads
- **Mesh:** Store-and-forward (patches relay neighbor packets)

### Layer 2: WiFi6 (ESP32-C6 / ESP32-S3)
- **Hub → Cloud:** MQTT over TLS (UV telemetry, scan results, risk scores)
- **Scanner → Cloud:** HTTP POST (multispectral images, too large for mesh)
- **Scanner → Hub:** WiFi notification (scan result summary)

### Layer 3: BLE 5.3 (ESP32-C6)
- **Hub → Mobile App:** Instant alerts (UV warning, lesion change)
- **Patch Pairing:** BLE advertisement for initial pairing via app

### Layer 4: Cloud (FastAPI + PostgreSQL/TimescaleDB)
- **MQTT ingestion:** Real-time telemetry from hubs
- **REST API:** Mobile app data queries (UV history, scans, lesions, inventory)
- **WebSocket:** Real-time alert push to mobile app
- **ML inference:** Cloud CNN for high-res scan analysis
- **Dermatologist reports:** Clinical PDF generation

## Edge vs Cloud Compute

| Function | Location | Rationale |
|----------|----------|-----------|
| UV dose accumulation | Patch (edge) | Real-time, low-power, no comms needed |
| MED fraction + haptic | Patch (edge) | Real-time burn prevention, no latency |
| UV status + hours-to-burn | Hub (edge) | 5-min update, heuristic + TFLite Micro |
| Condition pre-screen | Scanner (edge) | Instant feedback, TFLite Micro <200KB |
| ABCDE pre-screen | Scanner (edge) | Instant lesion risk, TFLite Micro |
| Full condition CNN | Cloud | 25-class, 20MB model, high-res images |
| Skin cancer risk (cumulative) | Cloud | 90-day temporal CNN, needs full history |
| Routine optimization | Cloud | LSTM on 90-day data + ingredient modeling |
| Skin age | Cloud | Longitudinal trend analysis |
| Lesion evolution | Cloud | Comparison with prior scans, image diff |

## Security & Privacy

- **TLS encryption** for all WiFi/cloud communication
- **AES-256** for skin images at rest
- **No always-on cameras** — scanner is handheld, user-initiated
- **Biometric data protection** — skin images are highly sensitive
- **User-controlled cloud backup** — off by default, opt-in
- **HIPAA-aware architecture** for clinical data handling
- **Delete anytime** — full data deletion on request
- **No third-party sharing**
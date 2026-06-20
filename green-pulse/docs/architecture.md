# GreenPulse System Architecture

## Overview

GreenPulse is a 4-node houseplant health monitoring & care system:

1. **Plant Tag** (nRF52832 + SX1262) — per-pot: soil moisture, light, temp, humidity
2. **Leaf Scanner** (ESP32-S3) — handheld: multispectral disease/pest imaging + species ID
3. **Water Valve** (ESP32-C6 + SX1262) — per-zone: automated watering with flow monitoring
4. **Hub Node** (RP2040 + ESP32-C6 + nRF52832) — coordinator: edge ML + display + cloud bridge

## Data Flow

```
Plant Tags ──Sub-GHz mesh──► Hub Node ──WiFi6──► Cloud (MQTT → FastAPI → TimescaleDB)
   │  (soil moisture, light,         │                   │
   │   temp, humidity, battery,      │  (aggregated      │
   │   flags)                         │   telemetry +      │
│                                │   risk scores)    │
Leaf Scanner ──WiFi6──► Hub + Cloud
   │  (species ID, disease class,
   │   pest count, multispectral images)
   │
Water Valve ──Sub-GHz──► Hub Node
   │  (watering ack: liters, duration,
   │   status, leak flag)
   │
Hub ──BLE──► Mobile App (instant alerts)
Hub ──WiFi──► Cloud (MQTT → FastAPI → TimescaleDB)
Cloud ──► Mobile App (full sync, reports)
```

## Communication Protocol

- **Plant mesh:** Sub-GHz mesh (868 MHz EU / 915 MHz US) via SX1262, 50m indoor range, ultra-low-power, mesh-relay (tags forward neighbor packets)
- **Cloud bridge:** WiFi6 (ESP32-C6), MQTT over TLS
- **Scanner link:** WiFi6 (ESP32-S3) — image data too large for Sub-GHz
- **Mobile:** BLE 5.3 + WiFi
- **Tag battery:** CR2477 coin cell → 18+ months at 15-min sampling

## Disease Detection Pipeline

```
Leaf Scanner                Cloud ML
┌─────────────┐            ┌──────────────┐
│ OV5640 →    │──WiFi──►   │ Disease CNN  │
│  white/UV/  │            │ (EffNet-Lite │
│  NIR 3-shot │            │  40 classes) │
│             │            │ + YOLOv8     │
│ Edge:       │            │  pest detect │
│ species ID  │            │ → annotated  │
│ (MobileNet) │            │   leaf image │
│ + healthy/  │            └──────────────┘
│   suspect   │
└─────────────┘
```

## Watering Loop

```
Soil moisture < species threshold → Hub triggers:
  1. Hub sends GP_MSG_WATERING_CMD to valve (zone, emitter, duration, target_ml)
  2. Valve opens latching solenoid for duration
  3. Flow sensor confirms liters delivered
  4. Valve closes, checks for leak (flow after close)
  5. Valve sends GP_MSG_WATERING_ACK (ml, duration, status, flags)
  6. Next telemetry shows moisture rise → confirms success

Safety:
  - Auto-close after 10 min max (flood prevention)
  - Never open if pressure < 1 PSI (empty reservoir)
  - Leak detection: flow > 5 pulses 3s after close → alert
  - Boot-time close (no stuck-open after power loss)
  - Latching solenoid stays in last position if power lost
```

## Privacy Architecture

- No microphones, no always-on cameras (Leaf Scanner is handheld, user-initiated)
- All data encrypted in transit (TLS) and at rest
- Plant data is yours; no third-party sharing
- Scanner images stored locally on SD + optional cloud backup (user-controlled)
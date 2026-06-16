# CradleKeep — Architecture Documentation

## System Overview

CradleKeep is a 4-node infant monitoring and care system that helps parents track, understand, and respond to their baby's needs. It monitors breathing, classifies cries, tracks feedings, and optimizes the nursery environment.

### Design Philosophy

1. **Safety First**: Breathing monitoring runs locally on the hub with sub-500ms response. No cloud dependency for safety-critical functions.
2. **Privacy Preserving**: Audio is processed on-device (ESP32-S3 TinyML). Video is streamed on-demand only, never stored. Raw breathing waveform stays local.
3. **Non-Invasive**: No wearables on the baby. No cameras pointed at the crib during sleep (IR night vision only). All sensing is through the under-mattress pad and wall-mounted monitor.
4. **Fail-Safe**: Battery backup on all nodes. Mesh network continues without WiFi. CR2450 coin cell in crib pad lasts 18+ months.

## Block Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                        CLOUD (AWS/GCP)                              │
│  ┌────────────┐  ┌──────────────┐  ┌──────────────────────────┐  │
│  │ FastAPI    │  │ PostgreSQL  │  │ ML Pipeline               │  │
│  │ REST API   │  │ TimescaleDB │  │ Cry Classifier (MobileNet) │  │
│  │ WebSocket  │  │             │  │ Sleep Stager (CNN+LSTM)    │  │
│  │ MQTT Bridge│  │             │  │ Pattern Predictor (TFT)    │  │
│  └──────┬─────┘  └──────┬──────┘  └──────────────────────────┘  │
│         │               │                                         │
│         └───────┬───────┘                                         │
│                 │ MQTT                                             │
└─────────────────┼──────────────────────────────────────────────────┘
                  │
          ┌───────┴────────┐
          │  WiFi/BLE       │
          │  ESP32-C6       │
          │                 │
          │  ┌────────────┐ │      Sub-GHz LoRa Mesh (868MHz)
          │  │  RP2040    │ │──────────────────┬──────────────────┐
          │  │  Hub MCU   │ │                  │                  │
          │  │  Coordinator│ │          ┌───────┴───────┐  ┌─────┴──────┐
          │  │  + Display  │ │          │  Crib Pad     │  │Nursery     │
          │  │  + Speaker  │ │          │  STM32L476    │  │Monitor     │
          │  │  + Safety   │ │          │  + 4×FSR (BCG)│  │ESP32-S3   │
          │  │    Rules    │ │          │  + LIS3DH     │  │+ Camera    │
          │  └────────────┘ │          │  + SHT40      │  │+ Dual Mic  │
          │                 │          │  + Wetness    │  │+ Env Sens. │
          │                 │          │  + SX1261     │  │+ SX1261   │
          └─────────────────┘          │  + CR2450     │  │+ LiPo     │
                  │                    └───────┬───────┘  └─────┬──────┘
                  │                            │                 │
                  │ BLE                 ┌──────┴───────┐        │
                  │                     │Feeding Station│        │
                  └─────► Mobile App    │nRF52840      │        │
                         (React Native) │+ HX711×2    │◄───────┘
                                        │+ DS18B20    │
                                        │+ PTC Heater │
                                        │+ OLED       │
                                        │+ SG90 Servo │
                                        │+ SX1261     │
                                        │+ LiPo       │
                                        └──────────────┘
```

## Data Flow

### Sensor Data Flow (every 2 seconds)

1. **Crib Pad** (Slot 0): Breathing rate + regularity + position + temperature + wetness → Hub
2. **Nursery Monitor** (Slot 1): Cry type + room conditions + baby presence → Hub
3. **Feeding Station** (Slot 2): Bottle weight + temperature + feeding state → Hub
4. **Hub** (Slot 3): Sync + commands broadcast
5. **ACK** (Slot 4): Retransmit if needed

### Hub Processing Pipeline

1. Receive sensor data from all nodes (every 500ms TDMA frame)
2. Run local breathing safety rules (sub-100ms response)
3. Run local sleep staging on BCG data (TFLite Micro on RP2040)
4. If cry detected: auto-soothe with appropriate sound
5. Aggregate data, buffer for WiFi upload
6. Upload to cloud via ESP32-C6 WiFi (every 5 seconds)
7. Broadcast status to mobile app via BLE

### Cloud Processing

1. MQTT broker receives sensor data from hub
2. FastAPI persists to PostgreSQL/TimescaleDB
3. ML pipeline processes:
   - Cry classification refinement (cloud model is more accurate than on-device)
   - Sleep stage analysis over longer windows
   - Pattern prediction for next 2-4 hours
4. Push notifications for alerts
5. Pattern insights: "Your baby typically wakes in 45 minutes"

## Communication Protocols

### Sub-GHz Mesh (Primary — Critical)

- **Frequency**: 868 MHz (EU) / 915 MHz (US)
- **Modulation**: LoRa SF7 (normal) / SF9 (alerts)
- **TDMA**: 500ms frame, 5 × 100ms slots
- **Priority**: Crib Pad always gets Slot 0 (breathing safety data)
- **Range**: 30m indoor (nursery + adjacent rooms)

### WiFi (Secondary — Cloud)

- **Protocol**: MQTT over TLS (QoS 1 for alerts, QoS 0 for telemetry)
- **Frequency**: Every 2 seconds for breathing, every 5 seconds for environment
- **Video/Audio**: On-demand RTSP stream (ESP32-S3 camera)

### BLE (Local — Mobile App)

- **Protocol**: GATT server on hub
- **Characteristics**: Sleep state, cry status, feeding log, room conditions, commands
- **Range**: 10m (nursery area)

## Power Management

### Hub Node
- **Source**: 5V USB-C (primary) + 3000mAh Lipo (backup)
- **Average**: 250mA (WiFi on, speaker active) → ~6 hours on battery
- **Failsafe**: Auto-switches to battery on USB loss; mesh continues

### Crib Pad
- **Source**: CR2450 coin cell (primary) + optional USB-C
- **Average**: 8µA (sleep between samples) → 18+ months
- **Duty Cycle**: Sample 200Hz for 100ms, process 5ms, transmit 15ms, sleep 380ms

### Nursery Monitor
- **Source**: 5V USB-C (primary) + 1200mAh Lipo (backup)
- **Average**: 180mA (WiFi, camera standby) → ~7 hours on battery
- **Low Power**: Mic-only mode → 30mA (40+ hours)

### Feeding Station
- **Source**: 5V USB-C (primary) + 2000mAh Lipo (backup)
- **Average**: 50mA (standby) → 3A peak (PTC heater)
- **Heater**: Only during warming cycle (~4 minutes per session)

## Breathing Safety Architecture

> **CradleKeep is NOT a medical device. It does not claim to prevent SIDS or diagnose any condition.**

### Monitoring Pipeline (Local, Always-On)

1. FSR sensors sample at 200Hz → bandpass filter (0.2-2.0 Hz)
2. Peak detection → breath rate and regularity
3. Apnea detection: no breath peak for >5s → warning
4. Movement detection: FSR delta > threshold → movement epoch
5. Position detection: LIS3DH gravity vector → supine/prone/lateral
6. All processing on Crib Pad MCU → transmit summary every 2s

### Alert Escalation (Local on Hub, No Cloud Needed)

| Time | Action |
|------|--------|
| 0-5s no breath | Internal monitoring |
| 5-10s | Gentle app vibration ("check on baby?") |
| 10-15s | Hub speaker alert + app push notification |
| >15s | Full alarm: siren + SMS to emergency contacts |

### Position Safety

- Prone (on stomach) detected → info notification
- Prone for >10 minutes → warning notification

## Cry Classification

### On-Device (ESP32-S3, TinyML)

- Model: MobileNetV1 0.25 (quantized INT8)
- Input: 1-second mel-spectrogram (64×64)
- Output: 6-class probability (none + 5 cry types)
- Latency: ~100ms per inference
- Accuracy: >85% (5-class), >95% (cry vs. no-cry)

### Cloud Refinement

- Model: EfficientNet-B0 (full precision)
- Input: 3-second mel-spectrogram (128×128)
- Output: 6-class probability with higher accuracy
- Used for: pattern analysis, parental reports, long-term insights

### Cry Response Matrix

| Cry Type | Suggested Response | Auto-Soothe |
|----------|-------------------|-------------|
| Hungry | "Time to feed?" + feeding tracker | None (feeding needed) |
| Tired | "Baby seems sleepy" + dim lights | White noise |
| Pain | "Check for discomfort" | Heartbeat sound |
| Colic | "Try tummy massage" | Shushing sound |
| Discomfort | "Check diaper/temperature" | White noise |

## Privacy Architecture

- **No cloud video storage**: Video streamed live on-demand only
- **Audio processed locally**: Cry classification on ESP32-S3 — only results sent to cloud
- **Breathing data**: Rate + regularity + position sent to cloud; raw waveform stays local
- **Feeding data**: Volume + timestamps only (no camera on feeding station)
- **Parent controls**: All data sharing opt-in. Local-only mode available.
- **GDPR/COPPA**: No PII stored for children under 13
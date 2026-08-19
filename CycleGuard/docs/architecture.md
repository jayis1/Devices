# CycleGuard Architecture

## Overview

CycleGuard is a multi-node hardware+software system for cycling safety, crash detection, theft prevention, and route safety scoring.

## Design Principles

1. **Multi-modal sensing** — No single sensor covers all safety needs. Crash detection (helmet IMU), proximity warning (hub camera), lighting (smart light IMU + ambient), theft (lock IMU + GPS + load cell) each need dedicated hardware.
2. **Edge-first inference** — CrashNet (helmet), BlindSpotNet (hub), and CollisionPredict (hub) run on-device to minimize latency for real-time crash alerts (<500 ms).
3. **Safety-critical redundancy** — Crash confirmation requires 2+ sensor agreements. 911 dispatch has 10-second cancelable window. Cellular backup works without phone.
4. **Privacy-preserving** — No audio recording. Camera processes on-device. GPS only tracked during rides or theft alarm.
5. **Fail-safe** — All safety functions have graceful degradation paths.

## Data Flow

```
Sensors → Node MCU → Radio (BLE/Sub-GHz) → Hub → MQTT → Cloud → ML Pipeline → API → Mobile App
                                                         ↓
                                                    PostgreSQL
```

### Real-time path (< 1 second)
- Helmet IMU 500 Hz → CrashNet → BLE → Hub → crash confirmation → 911 dispatch
- Hub camera → BlindSpotNet → BLE → Helmet bone-conduction + haptic warning
- Hub GPS + sensors → CollisionPredict → helmet haptic triple-burst

### Near-real-time (1–10 seconds)
- Bike sensor speed/cadence/TPMS → BLE → Hub → display + cloud
- Smart light brake detection → BLE → Hub → cloud safety score

### Batch (daily/weekly)
- 48-hour crash risk forecast → cloud ML → app notification
- Route safety scoring → GCN + historical data → route planner
- Ride reports → cloud → app + PDF export

## Radio Architecture

### Sub-GHz 868 MHz TDMA Mesh (Hub-coordinated)
- **Nodes:** Smart Lock
- **Range:** 500 m urban, 2 km line-of-sight
- **Purpose:** Arm/disarm, theft alerts, geo-fence (long range needed for parked bike)

### BLE 5.0 Star (Hub as central)
- **Nodes:** Smart Helmet, Smart Light, Bike Sensor
- **Range:** 10 m (all bike-mounted, close to Hub)
- **Purpose:** Real-time sensor data, crash alerts, light commands

### 4G LTE Cellular (Independent)
- **Nodes:** Hub (crash 911 dispatch), Smart Lock (theft tracking)
- **Purpose:** Emergency communication when Wi-Fi unavailable

## Power Budget

| Node | Battery | Life | Duty Cycle |
|------|---------|------|------------|
| Hub | 18650 LiFePO4 3200mAh + USB-C | 12+ hr riding | GPS 10 Hz, camera 10 fps, display always-on |
| Smart Helmet | 402030 LiPo 500mAh | 5 days | IMU 500 Hz continuous, BLE notify 1 s |
| Smart Light | 18650 Li-ion 2600mAh | 20+ hr | Front 50%, rear 50%, brake detection 100 Hz |
| Bike Sensor | CR2477 1000mAh | 12 months | Interrupt-driven, BLE every 1–2 s |
| Smart Lock | 18650 Li-ion 2600mAh | 30+ day standby | IMU 100 Hz when armed, GPS off, 4G off |

## ML Model Deployment

| Model | Where | Framework | Latency |
|-------|-------|-----------|---------|
| CrashNet | Helmet (nRF52840) | tflite-micro int8 | 15 ms |
| BlindSpotNet | Hub (ESP32-S3) | tflite-micro int8 | 200 ms |
| CollisionPredict | Hub (ESP32-S3) | tflite-micro int8 | 100 ms |
| TheftPattern | Cloud | ONNX | 50 ms |
| RouteSafety | Cloud | TorchScript | 200 ms |
| CrashRiskForecast | Cloud | XGBoost | 10 ms |

## Security

- **Transport:** TLS 1.3 (MQTT), AES-128 (Sub-GHz mesh), BLE 5.0 encryption
- **Data at rest:** PostgreSQL with encryption
- **Privacy:** No raw audio leaves helmet. Camera frames processed on-device.
- **Emergency:** 4G LTE backup for crash dispatch + theft tracking when Wi-Fi down
- **Lock security:** Failsafe deadbolt position verification (AS5600), anti-drill, anti-pry
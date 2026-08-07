# GuideSync — Architecture

## System Overview

GuideSync is a 5-node-type wearable IoT system for spatial awareness,
navigation, and visual assistance for the blind & visually impaired.
It combines head-level vision (smart glasses), ground-level sensing
(smart cane), ambient infrastructure (nav beacons), wrist-level safety
(haptic band), and a coordinating gateway (vision hub) into a
coordinated spatial awareness network with a 6-model ML pipeline.

## Node Topology

```
                    ┌─────────────┐
                    │  Mobile App │  (caregiver + user with VoiceOver)
                    └──────┬──────┘
                           │ HTTPS
                    ┌──────▼──────┐
                    │   Cloud     │ (FastAPI + MQTT + InfluxDB + PostgreSQL)
                    └──────┬──────┘
                           │ MQTT / HTTPS
                    ┌──────▼──────┐
                    │  Vision Hub │ (ESP32-S3, Wi-Fi + 4G LTE backup)
                    └──────┬──────┘
                           │ BLE 5.0 Star Network
          ┌────────────────┼────────────────┬──────────────┐
          │                │                │              │
   ┌──────▼─────┐  ┌──────▼─────┐  ┌───────▼─────┐  ┌─────▼──────┐
   │ Smart      │  │ Smart Cane │  │ Haptic Band │  │ Nav Beacon │
   │ Glasses    │  │ nRF52840   │  │ nRF52840    │  │ ×N         │
   │ ESP32-S3   │  │ HC-SR04    │  │ DRV2605L    │  │ nRF52840   │
   │ OV5640 cam │  │ VL53L0X    │  │ ICM-42688   │  │ BLE adv    │
   │ VL53L5CX   │  │ ICM-42688  │  │ SOS button  │  │ CR2032     │
   │ SceneNet   │  │ DRV2605L   │  │ FallNet CNN │  │ 12-18 mo   │
   │ CrosswalkN │  │            │  │             │  │            │
   │ Bone cond. │  │            │  │             │  │            │
   └────────────┘  └────────────┘  └────────────┘  └────────────┘
```

## Data Flow

1. **Smart Glasses** (head-mounted) capture scene images + ToF depth
   → SceneNet (YOLOv8-nano) detects obstacles → ObstacleNet verifies
   via ToF → CrosswalkNet detects signals → bone conduction audio
   describes scene

2. **Smart Cane** (hand-held) sweeps ground level → ultrasonic detects
   obstacles → downward ToF detects drop-offs/stairs → haptic motor
   vibrates with direction patterns

3. **Nav Beacons** (×N) broadcast BLE advertisements → glasses + band
   RSSI fingerprints → NavNet LSTM computes indoor position →
   haptic band provides turn-by-turn navigation

4. **Haptic Band** (wrist-worn) provides nav haptics + fall detection
   (FallNet 1D-CNN) → on fall, sends BLE alert to Hub → Hub dispatches
   emergency SMS + 911 via 4G LTE

5. **Vision Hub** (pocket-carried) coordinates BLE star, runs OCR
   (EAST+CRNN), bridges to cloud via Wi-Fi/MQTT with 4G LTE backup

6. **Cloud** runs 6-model ML pipeline + emergency dispatch + beacon
   registry + familiar face database (encrypted)

7. **Mobile App** configures beacons, manages contacts, reviews
   history, shares location with caregivers

## Communication

- **BLE 5.0:** 2.4 GHz star network (Hub = central, wearables = peripherals)
- **Nav Beacons:** BLE advertising (iBeacon/Eddystone compatible)
- **Hub→Cloud:** Wi-Fi/MQTT with 4G LTE cellular backup
- **Encryption:** BLE LE Secure Connection (AES-128-CCM)
- **Range:** 10 m indoor (BLE 5.0), 30 m LOS
- **Max nodes:** 3 peripherals + 32 beacons = 35

## Power Architecture

| Node | Power | Battery | Autonomy |
|------|-------|---------|----------|
| Vision Hub | USB-C / LiPo | 2000 mAh | 12h portable, continuous on USB |
| Smart Glasses | LiPo | 800 mAh | 6 hours continuous |
| Smart Cane | LiPo | 500 mAh | 20+ hours |
| Haptic Band | LiPo | 300 mAh | 48+ hours |
| Nav Beacon | CR2032 | 220 mAh | 12-18 months |

## ML Pipeline

| Model | Type | Edge Target | Inference Time |
|-------|------|-------------|----------------|
| SceneNet | YOLOv8-nano | ESP32-S3 | <300 ms |
| ObstacleNet | 1D-CNN | ESP32-S3 | <20 ms |
| TextReader | EAST+CRNN | Hub ESP32-S3 | ~1.2 s |
| NavNet | LSTM | Cloud + Hub | <100 ms |
| CrosswalkNet | MobileNetV3 | ESP32-S3 | <80 ms |
| FallNet | 1D-CNN | nRF52840 | <400 ms |

## Safety

- **Fall detection:** FallNet 96% sensitivity, <0.3 FP/day → 30s cancel
  window → SMS + 911 dispatch via 4G LTE
- **SOS:** 3s long-press → SMS + 911 → 60s cancel (3× rapid press)
- **Obstacle dual-confirmation:** SceneNet + ObstacleNet must agree
- **Crosswalk safety:** >90% confidence required to announce "walk"
- **Open-ear design:** Bone conduction preserves environmental hearing
- **Audio priority:** Safety alerts preempt non-critical audio

## Privacy

- All scene understanding runs on-device (no video leaves glasses)
- OCR only activates on explicit voice command
- Face recognition opt-in (encrypted embeddings, on-device matching)
- No cloud video streaming — only text scene descriptions sent
- BLE beacons broadcast only UUID + RSSI (no personal data)
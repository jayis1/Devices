# FireSync — Architecture

## System Overview

FireSync is a 5-node-type fire safety IoT system that replaces dumb smoke
detectors with a coordinated, multi-sensor, AI-driven fire detection,
suppression, and escape guidance network. It combines multi-sensor room
sentinels (smoke + CO + temperature + thermal array + FlameNet CNN),
a stove guard (thermal array + knob sensors + motorized gas valve),
an electrical panel monitor (CT clamps + FFT + ArcDetect CNN),
an escape controller (addressable LED path lighting + voice guidance +
door release), and a coordinating gateway (hub) into a complete fire
safety pipeline with a 6-model ML pipeline running across edge and cloud.

## Node Topology

```
                    ┌─────────────┐
                    │  Mobile App │  (homeowner + caregiver)
                    └──────┬──────┘
                           │ HTTPS
                    ┌──────▼──────┐
                    │   Cloud     │ (FastAPI + MQTT + InfluxDB + PostgreSQL)
                    └──────┬──────┘
                           │ MQTT / HTTPS
                    ┌──────▼──────┐
                    │ FireSync    │ (ESP32-S3, Wi-Fi + 4G LTE backup)
                    │ Hub         │ (Sub-GHz TDMA mesh coordinator)
                    └──────┬──────┘
                           │ Sub-GHz 868 MHz TDMA Mesh
          ┌────────────────┼────────────────┬──────────────┐
          │                │                │              │
   ┌──────▼─────┐  ┌──────▼─────┐  ┌───────▼─────┐  ┌─────▼──────┐
   │ Room       │  │ Stove      │  │ Panel      │  │ Escape     │
   │ Sentinel×N │  │ Guard      │  │ Monitor    │  │ Controller│
   │ ESP32-S3   │  │ ESP32-S3   │  │ STM32G431  │  │ ESP32-S3  │
   │ SX1262     │  │ SX1262     │  │ SX1262     │  │ SX1262    │
   │ PMSA003    │  │ MLX90640  │  │ SCT-013×2  │  │ WS2812B×4 │
   │ ZE07-CO    │  │ AS5600×4  │  │ DS18B20×4  │  │ MAX98357A │
   │ DS18B20    │  │ L298N     │  │ Shunt trip │  │ W25Q128   │
   │ MLX90640   │  │ Gas valve │  │ ArcDetect  │  │ Relays×4  │
   │ FlameNet   │  │ PanTemp   │  │            │  │ LiFePO4   │
   │ PIR        │  │           │  │            │  │           │
   │ LiPo 1000  │  │ LiPo 1200 │  │ LiPo 500   │  │ 5000 mAh  │
   └────────────┘  └────────────┘  └────────────┘  └───────────┘
```

## Data Flow

1. **Room Sentinels** fuse 4 sensor modalities → FlameNet CNN classifies
   → on fire class, sends FIRE_ALERT to Hub (emergency priority)

2. **Stove Guard** watches thermal array + knob sensors → auto-shutoff
   timer + thermal-based gas valve close → sends FIRE_ALERT

3. **Panel Monitor** samples current at 8 kHz → FFT → ArcDetect CNN
   → on arc fault, triggers shunt-trip + sends FIRE_ALERT

4. **Hub** receives FIRE_ALERT → multi-node consensus → if confirmed:
   - ALARM_TRIGGER to all sentinels (buzzer + strobe)
   - ESCAPE_UPDATE to Escape Controller (LED path + voice + doors)
   - STOVE_SHUTOFF, HVAC_SHUTOFF, PANEL_SHUTOFF (suppression)
   - 911 dispatch via 4G LTE
   - Publish to cloud + mobile app

5. **Escape Controller** drives LED strips (green path, red zone) +
   voice guidance + door release

6. **Cloud** runs 6-model ML pipeline, fire history, risk forecast,
   OTA, insurance reports

## Communication

- **Sub-GHz 868 MHz:** SX1262 LoRa, TDMA mesh (Hub = coordinator)
- **Hub→Cloud:** Wi-Fi/MQTT with 4G LTE cellular backup
- **Encryption:** AES-128-CTR (application layer, per-node key)
- **CRC:** CRC-16-CCITT (application layer end-to-end integrity)
- **Range:** 100 m indoor (penetrates 3+ walls), 2 km LOS
- **Emergency preemption:** FIRE_ALERT bypasses TDMA (3× immediate TX, <2s)

## Power Architecture

| Node | Power | Battery | Autonomy |
|------|-------|---------|----------|
| Hub | USB-C / PoE | LiPo 2000 mAh | 18 hours |
| Room Sentinel | USB-C wall | LiPo 1000 mAh | 14 hours |
| Stove Guard | USB-C wall | LiPo 1200 mAh | 10 hours |
| Panel Monitor | AC mains | LiPo 500 mAh | 6 hours |
| Escape Controller | USB-C wall | LiFePO4 5000 mAh | 48+ hours |

Battery backup is non-negotiable: 60% of fatal fires occur during power outage.
Escape Controller uses LiFePO4 (fire-safe chemistry — no thermal runaway).

## ML Pipeline

| Model | Type | Edge Target | Inference |
|-------|------|-------------|----------|
| FlameNet | Multi-modal CNN | ESP32-S3 | <200 ms |
| ThermalAnomaly | LSTM Autoencoder | ESP32-S3 | <100 ms |
| ArcDetect | 1D-CNN (FFT) | STM32G431 | <10 ms |
| EscapeRouter | Dijkstra + weights | Hub ESP32-S3 | <50 ms |
| OccupantTracker | HMM | Cloud | <100 ms |
| RiskForecast | XGBoost | Cloud | <10 ms |

## Safety

- **Fire detection:** FlameNet 97.3% accuracy, 98.6% recall on fire classes
- **Multi-node consensus:** Single node >85% or two nodes agree
- **Suppression:** Stove valve (fails closed), shunt-trip (fails open),
  HVAC shutoff
- **Escape:** Dynamic Dijkstra routing avoiding fire + adjacent rooms
- **911 dispatch:** 4G LTE with 60s cancel window
- **Fail-safe:** Gas valve fails closed, breaker fails open, doors fail open,
  LED path LiFePO4

## Privacy

- All sensor processing on-device + cloud (no third-party sharing)
- Thermal images never uploaded unless fire event
- Data shared only on confirmed fire
- No raw audio/voice recorded (voice guidance is pre-recorded)
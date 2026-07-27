# GrillSync — Architecture

## System Overview

GrillSync is a 4-node-type IoT system for smart grilling and BBQ safety.
It combines wireless multi-probe thermocouple monitoring, thermal-array
grill surface monitoring, gas leak detection, flare-up prediction, smoke
quality classification, and AI doneness prediction into a unified system
that makes outdoor cooking safe and foolproof.

## Node Topology

```
                    ┌─────────────┐
                    │  Mobile App │
                    └──────┬──────┘
                           │ HTTPS / WebSocket
                    ┌──────▼──────┐
                    │   Cloud     │ (FastAPI + MQTT + InfluxDB + PostgreSQL)
                    │ 6-model ML  │
                    └──────┬──────┘
                           │ MQTT / HTTPS
                    ┌──────▼──────┐
                    │  Grill Hub  │ (ESP32-S3, Wi-Fi)
                    └──────┬──────┘
                           │ Sub-GHz 868 MHz + BLE 5.0
          ┌────────────────┼────────────────┬──────────────┐
          │                │                │              │
   ┌──────▼─────┐  ┌──────▼─────┐  ┌───────▼─────┐  ┌─────▼──────┐
   │ Grill      │  │ Meat       │  │ Smoke       │  │ (More      │
   │ Sentinel   │  │ Probe ×N   │  │ Node        │  │  Probes)   │
   │ ESP32-S3   │  │ nRF52840   │  │ ESP32-S3    │  │            │
   │ +SX1262    │  │ +BLE 5.0   │  │ +SX1262     │  │            │
   │ MLX90640   │  │ MAX31855×4 │  │ PMS5003     │  │            │
   │ MQ-2 gas   │  │ LiPo 500   │  │ BME680      │  │            │
   │ IR flame   │  │ IP67       │  │ MQ-135      │  │            │
   │ BME280     │  │            │  │ UV flame    │  │            │
   │ Piezo      │  │            │  │ BME280      │  │            │
   └────────────┘  └────────────┘  └────────────┘  └────────────┘
```

## Data Flow

1. **Grill Sentinel** (mounted on grill side shelf) continuously monitors
   the grill via MLX90640 32×24 thermal array (surface temperature map),
   MQ-2 gas sensor (propane leak detection at 10% LEL), IR flame detector
   (runaway fire detection), BME280 (ambient conditions), and piezo
   acoustic sensor (fat-drip and flare-up sound patterns) → on-device
   FlareUpNet LSTM predicts flare-ups 8–15s in advance → reports to Hub
   immediately via Sub-GHz 868 MHz (event-driven + 2s telemetry)

2. **Meat Probe** (×N, inserted into meat, wireless) uses 4× Type-K
   thermocouples via MAX31855 to measure internal meat temperature at 4
   depths + ambient grill temp → BLE 5.0 to Hub → reports every 2 seconds
   during active cook → DonenessNet predicts doneness from thermal gradient

3. **Smoke Node** (for BBQ smoking, placed in smoker chamber) monitors
   PMS5003 PM2.5 particulate, BME680 VOC, MQ-135 gas, and UV flame sensor
   → Sub-GHz 868 MHz to Hub → SmokeNet classifies smoke quality (clean
   blue / dirty white / creosote) every 10 seconds

4. **Grill Hub** aggregates all sensor data, runs local edge doneness
   prediction + flare-up alert + gas-leak alert, drives RGB LED ring
   (doneness color: red→orange→yellow→green), triggers gas shutoff relay
   on leak/fire, drives OLED display (current temps + countdown), forwards
   to cloud via MQTT, manages OTA firmware distribution

5. **Cloud** runs full 6-model ML pipeline:
   - DonenessNet: meat doneness prediction (retraining)
   - FlareUpNet: flare-up prediction (LSTM)
   - GasLeakNet: gas leak pattern classification (XGBoost)
   - SmokeNet: smoke quality classification (1D-CNN)
   - GrillAnomaly: grill behavior anomaly detection (Isolation Forest)
   - SafetyForecast: cook session safety risk forecast (LSTM)

6. **Mobile App** receives push notifications (doneness countdown, flare-up
   warning, gas leak alert, food safety alert), displays real-time
   multi-probe temperature, thermal heat map, doneness predictions, cook
   timer, rest-time calculator, and cook history

## Communication

- **Sub-GHz:** 868 MHz (EU) / 915 MHz (US), LoRa SX1262, TDMA mesh
- **BLE 5.0:** Meat Probe (proximity to Hub, 10 m range)
- **Hub→Cloud:** Wi-Fi/MQTT
- **Encryption:** AES-128-CCM on all radio messages
- **Range:** 300 m LOS (Sub-GHz), 15 m (BLE)
- **Max nodes:** 2 grill sentinels + 1 smoke node + 8 meat probes = 11

## Power Architecture

| Node | Power | Battery | Autonomy |
|------|-------|---------|----------|
| Grill Hub | USB-C 5V / 12V | — | Continuous |
| Grill Sentinel | USB-C 5V | — | Continuous |
| Smoke Node | USB-C 5V | — | Continuous |
| Meat Probe | USB-C charge | 500 mAh LiPo | 8 hours (active cook) |

## Safety Architecture

- **Gas shutoff:** Hardware relay + motorized ball valve (fail-closed)
- **Flame detection:** Two independent sensors (IR photodiode + thermal array max temp)
- **Gas leak:** MQ-2 + BME680 VOC (cross-validation)
- **Temperature:** 4 thermocouples per probe (redundancy + gradient analysis)
- **Alert delivery:** Buzzer + LED ring + TFT display + push notification (4 channels)
- **Watchdog:** TPL5010 on battery-powered nodes for automatic recovery
- **OTA with rollback:** Firmware updates with automatic rollback on failure

## Privacy

- No camera or audio recording (thermal array only provides temperature values)
- All on-device AI inference (DonenessNet, FlareUpNet, SmokeNet)
- Only sensor readings and classification results transmitted to cloud
- No personal data collected beyond cook session logs
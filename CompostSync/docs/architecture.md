# CompostSync — Architecture

## System Architecture

```
                    ┌──────────────────────────────────────────────────┐
                    │                  CLOUD LAYER                      │
                    │  ┌──────────┐  ┌──────────┐  ┌──────────────────┐ │
                    │  │ FastAPI   │  │ Timescale│  │  ML Pipeline     │ │
                    │  │ Backend   │  │  DB      │  │  (6 models)      │ │
                    │  └────┬─────┘  └────┬─────┘  └────────┬─────────┘ │
                    │       │             │                  │           │
                    │       └──────┬──────┘                  │           │
                    │         MQTT Broker (Mosquitto)        │           │
                    │              │                         │           │
                    └──────────────┼─────────────────────────┼──────────┘
                                   │ WiFi                     │
                    ┌──────────────┼──────────────────────────┼──────────┐
                    │              ▼                          ▼          │
                    │     ┌─────────────────┐     ┌───────────────────┐  │
                    │     │      HUB        │     │  Mobile App       │  │
                    │     │  ESP32-WROOM-32E│     │  React Native      │  │
                    │     │                 │     │                   │  │
                    │     │ LoRa 868 Coordinator   │  - Dashboard     │  │
                    │     │ BLE 5.0 GATT    │     │  - Scanner        │  │
                    │     │ WiFi/MQTT client│     │  - Actions       │  │
                    │     │ Edge ML (TFLM)   │     │  - Timeline      │  │
                    │     │ OLED Display    │     │  - Settings      │  │
                    │     │ SD Logger       │     │                   │  │
                    │     └──┬───┬───┬──────┘     └───────┬───────────┘  │
                    │        │   │   │                     │ BLE 5.0    │
                    │   LoRa │   │   │ BLE                 │            │
                    └────────┼───┼───┼─────────────────────┼────────────┘
                             │   │   │                     │
              ┌──────────────┘   │   └──────────────┐      │
              ▼                  ▼                    ▼      │
    ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
    │  BIN NODE   │  │ WEATHER STA  │  │  SOIL PROBE  │    │
    │  ESP32      │  │  nRF52840    │  │  RP2040      │    │
    │             │  │              │  │              │    │
    │ Temp ×3     │  │ BME280       │  │ Temp ×4      │    │
    │ Moist ×3    │  │ Anemometer   │  │ Moist ×3     │    │
    │ SCD41 CO₂   │  │ Rain gauge   │  │ pH probe     │    │
    │ MQ-4 CH₄   │  │ Solar+LiPo   │  │ SCD41 CO₂    │    │
    │ HX711 mass  │  │ LoRa 868 TX  │  │ OLED display │    │
    │ Servo vent  │  │              │  │ BLE to Bin   │    │
    │ LoRa 868    │  │              │  │              │    │
    │ BLE to Soil │  │              │  │              │    │
    │ Solar+18650 │  │              │  │ 18650        │    │
    └──────────────┘  └──────────────┘  └──────────────┘    │
                                                           │
                        BLE 5.0 (Bin ↔ Soil Probe) ◄────────┘
```

## Data Flow

1. **Bin Node** reads sensors every 15 minutes → packs into `bin_node_data_t` → TX via LoRa in TDMA slot 1
2. **Weather Station** reads sensors every 5 min → packs into `weather_data_t` → TX via LoRa in TDMA slot 3
3. **Soil Probe** reads sensors every 60s → packs into `soil_probe_data_t` → sends via BLE to Bin Node
4. **Hub** receives all data via LoRa → runs edge ML (phase classification) → forwards via WiFi/MQTT to cloud
5. **Cloud** stores in TimescaleDB → runs heavy ML (maturity LSTM, C:N, completion, pest risk) → publishes results
6. **Mobile App** receives push notifications + pulls data via REST API or reads directly via BLE

## Communication Protocols

| Link | Protocol | Frequency | Range | Power | Encryption |
|------|----------|-----------|-------|-------|------------|
| Hub ↔ Bin Node | LoRa TDMA | 868 MHz | 500 m | Low | AES-128-CCM |
| Hub ↔ Weather | LoRa TDMA | 868 MHz | 500 m | Low | AES-128-CCM |
| Hub ↔ Mobile | BLE 5.0 GATT | 2.4 GHz | 10 m | Med | Pairing |
| Bin ↔ Soil | BLE 5.0 UART | 2.4 GHz | 5 m | Low | Pairing |
| Hub ↔ Cloud | WiFi/MQTT | 2.4/5 GHz | WiFi range | High | TLS |

## Power Architecture

All nodes except the Hub are solar-powered:
- **Hub**: USB-C primary, 18650 + solar backup (always-on, needs WiFi)
- **Bin Node**: 2W solar → 18650 3000mAh. 15-min duty cycle, 35-day dark runtime
- **Soil Probe**: 18650 replaceable, 90-day runtime on 60s sleep cycle
- **Weather Station**: 2W solar → LiPo 2000mAh, 30-day dark runtime

## Edge vs Cloud ML

| Model | Where | Why |
|-------|-------|-----|
| Phase Classifier | Hub (ESP32, TFLite Micro) | Real-time, 15-min latency, works offline |
| Maturity LSTM | Cloud | Needs 14-day time series, 64-unit LSTM too heavy for ESP32 |
| C:N Estimator | Cloud | XGBoost with 18 features |
| Completion Forecaster | Cloud | Gradient boosting, needs full history |
| Add-Item Classifier | Mobile (quantized) | On-device for instant camera scanning |
| Pest Risk | Cloud | Logistic regression, needs weather context |
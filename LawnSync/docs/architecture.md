# LawnSync — Architecture Document

## System Overview

LawnSync is a 5-node IoT system for intelligent lawn and turf health management. It combines distributed soil sensing, automated irrigation, weather monitoring, and multispectral imaging with a 6-model ML pipeline to optimize lawn care while reducing water consumption by 30–50%.

## Network Topology

```
                    ┌─────────────┐
                    │   Cloud     │
                    │ (FastAPI +  │
                    │  MQTT + ML) │
                    └──────┬──────┘
                           │ Wi-Fi / HTTPS
                    ┌──────▼──────┐
                    │    HUB      │
                    │  ESP32-S3   │
                    │ SX1276 868  │
                    │ TDMA Mesh   │
                    │ Coordinator │
                    └──────┬─────┘
              ┌────────┬─────┼──────┬────────┐
              ▼        ▼     ▼      ▼        ▼
          ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐
          │ Soil │ │ Soil │ │Weather│ │Sprink│ │Scanner│
          │Node×N│ │Node×N│ │ Station│ │ Ctrl │ │ Node │
          │nRF52 │ │nRF52 │ │ESP32-S3│ │ESP32 │ │ESP32-S3│
          │SX1262│ │SX1262│ │SX1262  │ │SX1262│ │SX1262 │
          └──────┘ └──────┘ └────────┘ └──────┘ └──────┘
```

## Communication Stack

| Layer | Protocol | Details |
|-------|----------|---------|
| Physical | LoRa (SX1262/SX1276) | 868 MHz, SF7-SF11 adaptive, +22 dBm |
| Link | TDMA Mesh | Hub-assigned slots, 50 ms per slot, 32 slots |
| Network | Binary protocol | 6-byte header + payload + CRC16 |
| Transport | AES-128-CCM | Encrypted, authenticated |
| Application | Telemetry, Commands, OTA | Type-tagged messages |

## Data Flow

### Soil Moisture Data
1. Soil node wakes every 15 min (RTC timer)
2. Reads FDC2214 (capacitive moisture), DS18B20 (temp), VEML7700 (light)
3. Powers ISE probes, reads NPK via SAADC
4. Builds telemetry message (19 bytes payload)
5. Waits for TDMA slot, transmits via LoRa
6. Hub receives → forwards to cloud via MQTT
7. Cloud stores in InfluxDB, runs SoilForecast LSTM
8. Dashboard updates, mobile app receives via WebSocket

### Irrigation Control
1. Cloud ML pipeline generates schedule (DQN + weather forecast)
2. Schedule published to hub via MQTT
3. Hub forwards to sprinkler via Sub-GHz mesh
4. Sprinkler caches schedule, executes at scheduled time
5. Valve opens with soft-start PWM, flow meter monitors
6. Safety checks every second (leak, pressure, freeze, rain)
7. Telemetry reports back through mesh → cloud
8. Water usage tracked, savings calculated vs. timer baseline

### Disease Detection
1. Scanner wakes on schedule (daily) or hub command
2. Captures RGB image (white LED), then NIR image (850 nm LED)
3. Computes NDVI: (NIR - Red) / (NIR + Red)
4. Runs DiseaseNet (15-class, TFLite-Micro, ~200 ms)
5. Runs WeedSeg (9-class, TFLite-Micro, ~800 ms)
6. Sends results via mesh (14-byte payload)
7. Hub forwards to cloud; raw images uploaded via Wi-Fi
8. Cloud runs full-precision model, generates alert if disease detected
9. Mobile app receives push notification

## Edge vs. Cloud Processing

| Task | Edge (ESP32-S3) | Cloud (GPU) |
|------|-----------------|-------------|
| Disease classification | DiseaseNet int8 (200 ms) | Full-precision re-analysis |
| Weed segmentation | WeedSeg int8 (800 ms) | Full-precision + pixel-accurate |
| Soil moisture forecast | — | SoilForecast LSTM (14-day) |
| Irrigation scheduling | Schedule caching | DQN inference + weather API |
| Drought stress | DroughtNet int8 | Full pipeline + NDVI map |
| Fertilization timing | — | XGBoost + SHAP |
| OTA distribution | Block relay | Firmware storage + signing |

## Power Management

### Solar Nodes (Soil, Weather, Scanner)
- LiFePO4 battery + MCP73871 solar charger
- Deep sleep between measurements (nRF52: ~3 µA, ESP32-S3: ~10 µA)
- Radio duty-cycled: TX only in assigned TDMA slot
- Sensor power-gating: ISE probes powered only during measurement
- Solar budget: 20 Wh/day input vs. 0.02–0.4 Wh/day consumption

### Mains Nodes (Hub, Sprinkler)
- USB-C 5V or 24VAC transformer
- Always-on (hub coordinates mesh, sprinkler monitors safety)
- TPL5010 external watchdog on sprinkler (independent reset)

## Reliability

- **Mesh relay:** Out-of-range nodes relay through intermediate nodes
- **SD card buffering:** Hub buffers 7 days of data during Wi-Fi outage
- **Local schedule cache:** Sprinkler continues irrigation schedule without cloud
- **OTA with rollback:** Dual-partition on ESP32, A/B partition on nRF52
- **CRC + AES-128-CCM:** All radio messages authenticated and integrity-checked
- **Hardware watchdog:** TPL5010 on sprinkler, ESP32 internal watchdog on others
- **Safety interlocks:** 8 hardware/firmware interlocks on sprinkler (leak, pressure, freeze, rain, max runtime, valve fault, manual override, flow anomaly)
# StormSync — Architecture Document

## System Overview

StormSync is a 5-node IoT system for intelligent home flood prediction and sump pump health monitoring. It combines ultrasonic water level sensing, pump current/vibration analysis, multi-depth soil moisture monitoring, weather tracking, and automated flood defense (backflow valve + backup pump) with a 6-model ML pipeline to predict flooding 6 hours ahead and prevent sump pump failures.

## Network Topology

```
                    ┌─────────────┐
                    │   Cloud     │
                    │ (FastAPI +  │
                    │  MQTT + ML) │
                    └──────┬──────┘
                           │ Wi-Fi / HTTPS (4G LTE backup)
                    ┌──────▼──────┐
                    │    HUB      │
                    │  ESP32-S3   │
                    │ SX1262 868  │
                    │ SIM7000 4G  │
                    │ TDMA Mesh   │
                    │ Coordinator │
                    └──────┬─────┘
              ┌────────┬─────┼──────┬────────┐
              ▼        ▼     ▼      ▼        ▼
          ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐
          │ Sump │ │ Soil │ │ Soil │ │Weather│ │Flood │
          │Sent. │ │Probe │ │Probe │ │Sent. │ │ Act. │
          │ESP32 │ │nRF52 │ │nRF52 │ │ESP32S3│ │ESP32 │
          │SX1262│ │SX1262│ │SX1262│ │SX1262│ │SX1262│
          └──────┘ └──────┘ └──────┘ └──────┘ └──────┘
```

## Communication Stack

| Layer | Protocol | Details |
|-------|----------|---------|
| Physical | LoRa (SX1262) | 868 MHz, SF7-SF11 adaptive, +22 dBm |
| Link | TDMA Mesh | Hub-assigned slots, 50 ms per slot, 24 slots |
| Network | Binary protocol | 7-byte header + payload + CRC16 |
| Transport | AES-128-CCM | Encrypted, authenticated |
| Application | Telemetry, Commands, Flood Status, OTA | Type-tagged messages |

## Data Flow

### Sump Pit Monitoring
1. Sump sentinel wakes every 30s (15s in storm mode)
2. Reads ultrasonic water level, CT clamp pump current, ADXL355 vibration, flow, water temp
3. Builds telemetry message (19 bytes payload)
4. Transmits via LoRa mesh to Hub
5. Hub forwards to cloud via MQTT (or 4G LTE if Wi-Fi down)
6. Cloud stores in InfluxDB, runs PumpHealth CNN + FloodForecast LSTM
7. Dashboard updates, mobile app receives via WebSocket

### Flood Defense Activation
1. StormRisk score exceeds threshold OR sump sentinel detects critical water level
2. Hub sends FLOOD_STATUS broadcast to all nodes
3. Flood actuator receives STORM_MODE command
4. Actuator closes backflow valve (30s max travel)
5. Backup pump put on standby
6. All nodes increase sampling rate
7. Cellular backup activated on hub
8. Mobile app sends "Storm Preparation" push notification

### Pump Health Monitoring
1. Sump sentinel collects 1024 vibration samples at 1 kHz per measurement
2. Computes RMS + peak → transmits as telemetry
3. Hub runs PumpHealth CNN edge inference (TFLite-Micro, ~200ms)
4. If anomaly detected → cloud re-analysis with full model
5. Cloud tracks degradation trend over weeks
6. "Replace pump in X weeks" notification when class ≥ 4

### Soil Saturation Early Warning
1. Soil probes measure moisture at 3 depths every 15 min
2. Rising deep (90cm) moisture = groundwater table rising
3. Cloud SoilSat LSTM predicts 24-hour moisture trend
4. If deep moisture rising + rain forecast → flood risk increases
5. Often provides 6-12 hours advance warning before sump pit water rises

## Edge vs. Cloud Processing

| Task | Edge (ESP32-S3 Hub) | Cloud (GPU) |
|------|---------------------|-------------|
| Pump health anomaly | PumpHealth int8 (200 ms) | Full-precision re-analysis |
| Flood level forecast | — | FloodForecast LSTM (6-hour) |
| Soil saturation forecast | — | SoilSat LSTM (24-hour) |
| Rainfall-runoff | — | XGBoost model |
| Storm risk scoring | — | Bayesian ensemble |
| Sensor anomaly | — | Isolation Forest |
| Flood status broadcast | Risk level relay | Full StormSync Score |
| OTA distribution | Block relay | Firmware storage + signing |

## Power Management

### Battery-Backed Nodes (Sump Sentinel, Flood Actuator)
- 12V SLA battery with float charging — CRITICAL for power outages
- Sump Sentinel: 48h monitoring (reduced 60s rate on battery)
- Flood Actuator: 24h including valve + pump operation
- Automatic mains-to-battery switchover

### Solar Nodes (Soil Probes, Weather)
- LiFePO4 battery + MCP73871 solar charger
- Deep sleep between measurements (nRF52: ~3 µA, ESP32-S3: ~10 µA)
- Radio duty-cycled: TX only in assigned TDMA slot
- Solar budget: 20 Wh/day input vs. 0.02–0.4 Wh/day consumption

### Mains Node (Hub)
- USB-C 5V or PoE
- 4G LTE cellular backup for alerts when Wi-Fi down
- Always-on (coordinates mesh)

## Reliability

- **4G LTE cellular backup:** Hub maintains cloud connectivity during internet outage
- **SD card buffering:** Hub buffers 14 days of data during connectivity outage
- **Hardware float switch:** Flood actuator operates independently of MCU
- **Spring-return valve:** Fails to closed (safety) on power loss
- **TPL5010 watchdog:** External supervisor on Flood Actuator
- **OTA with rollback:** Dual-partition on ESP32, A/B on nRF52
- **CRC + AES-128-CCM:** All radio messages authenticated and integrity-checked
- **Storm Mode Protocol:** Preemptive defense activation when risk > 55
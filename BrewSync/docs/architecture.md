# BrewSync Architecture

## System Overview

BrewSync is a distributed fermentation monitoring and control system consisting of 4 node types communicating over Sub-GHz radio and BLE, coordinated by a Hub gateway with cloud uplink.

## Block Diagram

```
┌────────────────────────────────────────────────────────────────────────┐
│                         CLOUD LAYER                                    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                 │
│  │ FastAPI REST │  │  TimescaleDB │  │  ML Pipeline │                 │
│  │ + WebSocket  │  │  (PostgreSQL)│  │  (6 models)  │                 │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘                 │
│         │                 │                  │                          │
│         └─────────────────┼──────────────────┘                          │
│                           │ MQTT / HTTPS                                │
└───────────────────────────┼────────────────────────────────────────────┘
                            │
┌───────────────────────────┼────────────────────────────────────────────┐
│                       EDGE LAYER                                       │
│         ┌─────────────────▼──────────────────┐                         │
│         │          BrewSync Hub               │                         │
│         │  ┌──────────┐  ┌──────────────┐    │                         │
│         │  │  RP2040   │  │   ESP32-C3   │    │                         │
│         │  │(Sub-GHz   │  │  (Wi-Fi/MQTT │    │                         │
│         │  │ coord,    │  │   bridge,    │    │                         │
│         │  │ relay     │  │   OTA)       │    │                         │
│         │  │ control)  │  │              │    │                         │
│         │  └─────┬─────┘  └──────┬───────┘    │                         │
│         │        │   UART Bridge  │            │                         │
│         │        └────────┬───────┘            │                         │
│         │        3.5" IPS LCD    Buzzer         │                         │
│         │        BMP390          SHT40          │                         │
│         └────────┬───────────────┬──────────────┘                         │
│                  │               │                                        │
│     Sub-GHz 868 MHz    BLE 5.0                                         │
│                  │               │                                        │
│    ┌─────────────┼───────┐      │                                        │
│    │             │       │      │                                        │
│ ┌──▼───┐  ┌─────▼──┐    │  ┌───▼────────┐                              │
│ │Ferment│  │Cellar  │    │  │Brew Scanner│                              │
│ │Node×N │  │Monitor │    │  │(Handheld)  │                              │
│ │       │  │        │    │  │            │                              │
│ │SG/Temp│  │T/H/Bar│    │  │Spectral/   │                              │
│ │CO2/pH │  │Vibr/   │    │  │ToF/CO2     │                              │
│ │Press. │  │Light   │    │  │            │                              │
│ └───────┘  └────────┘    │  └────────────┘                              │
└─────────────────────────────────────────────────────────────────────────┘
```

## Data Flow

### Fermentation Monitoring Pipeline

```
Fermenter Node Sensors                    Hub                              Cloud
─────────────────────     ──────────────────────────     ─────────────────────────
ADXL362 (tilt/SG)  ──┐
DS18B20 (temp)     ──┤
SCD41 (CO2)        ──┼─► STM32L476 ──► SX1262 ──► RP2040 ──► ESP32-C3 ──► MQTT ──► FastAPI
MS5837 (pressure)  ──┤    firmware        RF        protocol    bridge      broker   dashboard
EZO-pH (pH)        ──┘                                                                 │
                                                                                      ▼
                                                                              TimescaleDB (store)
                                                                              ML Pipeline (predict)
                                                                              WebSocket (push)
```

### Brew Scanner Pipeline

```
Scanner Sensors                             Hub                          Cloud
─────────────────────     ─────────────────────────     ─────────────────────────
AS7341 (spectral)  ──┐
VL53L1X (ToF)      ──┤
SCD41 (CO2)        ──┼─► ESP32-S3 ──► BLE 5.0 ──► Hub ──► MQTT ──► FastAPI
ICM-42670 (IMU)    ──┘    firmware          GATT              broker   dashboard
                                                                               │
1.3" LCD (local display)                                                       ▼
                                                                       Infection Detector
                                                                       IBU/Color Estimator
                                                                       Volume Measurement
```

## State Machine

### Fermentation State Machine (runs on Hub)

```
                    ┌──────────────┐
                    │   IDLE       │
                    │ (no batch)   │
                    └──────┬───────┘
                           │ batch_start command
                           ▼
                    ┌──────────────┐
                    │  LAG_PHASE   │
                    │ (waiting for │◄─── CO2 < threshold
                    │  yeast start)│     for 6+ hours
                    └──────┬───────┘
                           │ CO2 rate > 0.5 ppm/min
                           ▼
                    ┌──────────────┐
                    │ ACTIVE_      │
                    │ FERMENTATION │─────── Stuck detection
                    │ (CO2 active) │─────── (CO2 rate drops
                    └──────┬───────┘        <10% of peak for
                           │                12+ hours)
                           │ SG within 0.005 of target
                           ▼
                    ┌──────────────┐
                    │  FINISHING   │
                    │ (approaching │─────── Temperature
                    │   FG)        │        ramp for diacetyl
                    └──────┬───────┘        rest (optional)
                           │ SG stable for 3 days
                           ▼
                    ┌──────────────┐
                    │  CONDITIONING│
                    │ (aging,      │─────── Cold crash
                    │  clearing)   │        command
                    └──────┬───────┘
                           │ Package / Keg
                           ▼
                    ┌──────────────┐
                    │  COMPLETE    │
                    └──────────────┘
```

## Power Budget

| Node | Avg Current | Voltage | Power | Battery Life (18650 2600mAh) |
|------|------------|---------|-------|------------------------------|
| Fermenter (normal) | 2.1 mA | 3.7V | 7.8 mW | ~52 days |
| Fermenter (active) | 15 mA | 3.7V | 55.5 mW | ~7 days |
| Cellar Monitor | 0.5 mA | 3.7V | 1.85 mW | ~217 days |
| Brew Scanner (active) | 120 mA | 3.7V | 444 mW | ~8 hrs continuous |
| Hub (always-on) | 150 mA | 5V USB | 750 mW | N/A (USB powered) |

## Security Model

- **Radio encryption**: AES-128-CCM on all Sub-GHz and BLE payloads
- **Key provisioning**: ECDH key exchange during pairing (X25519 on Hub, pre-shared on nodes)
- **Cloud**: TLS 1.3, JWT authentication, per-user data isolation
- **OTA**: Signed firmware updates (Ed25519), rollback on failure
- **Physical**: Encrypted debug port, JTAG lock after provisioning

## Failure Modes & Mitigations

| Failure | Detection | Mitigation |
|---------|-----------|------------|
| Fermenter node battery low | Voltage monitor < 3.3V | Alert user, increase reporting interval to extend life |
| Hub Wi-Fi down | ESP32-C3 link monitor | Local logging continues, batch on Hub LCD |
| Sensor drift | Cross-validation (SG vs CO2) | Flag suspicious readings, request calibration |
| Stuck fermentation | CO2 evolution rate model | Alert + recommend temperature bump or yeast nutrient |
| Infection detected | Spectral anomaly | Alert + recommend specific treatment based on organism |
| Radio interference | CRC failures, RSSI monitor | Adaptive spreading factor (SF7→SF12), frequency hopping |

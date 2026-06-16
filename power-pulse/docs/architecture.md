# PowerPulse Architecture Documentation

## System Overview

PowerPulse is a multi-node home energy monitoring, optimization, and electrical safety system. It provides per-circuit and per-appliance energy visibility, real-time arc fault detection, solar self-consumption optimization, and predictive billing — all in an open, affordable package.

## Architecture Principles

1. **Offline-First**: Critical alerts (arc fault, overload) work without internet. Hub buffers data to microSD when cloud is unreachable.
2. **Mesh Resilience**: BLE mesh for appliance tags means tags relay data through each other — no single point of failure.
3. **Sub-GHz for Metal Penetration**: Circuit monitors inside metal breaker panels require Sub-GHz (868 MHz) to communicate reliably.
4. **Edge Intelligence**: Hub runs TensorFlow Lite Micro for local anomaly detection. No cloud dependency for safety-critical features.
5. **Progressive Complexity**: Start with just a hub + circuit monitor. Add appliance tags and solar node as needed.

## Network Topology

```
[Cloud] ←→ [Hub] ←→ [Circuit Monitor]   (Sub-GHz 868 MHz)
                 ↕
            [Appliance Tags ×N]           (BLE 5.0 mesh)
                 ↕
            [Solar Node]                  (Sub-GHz 868 MHz)
```

### Sub-GHz Network (868 MHz SRD Band)
- **Range**: ~30m through walls/panels (circuit monitor), ~50m (solar node)
- **Data Rate**: 10 kbps
- **Protocol**: Custom PowerPulse frame format (see protocol-spec.md)
- **Modulation**: FSK, CC1101
- **Frequency**: 868.0 MHz (EU SRD band, FCC 15.231 compliant)
- **TX Power**: +10 dBm

### BLE Mesh Network (2.4 GHz)
- **Range**: ~10m per hop
- **Data Rate**: 125 kbps (long range mode)
- **Protocol**: BLE mesh with custom vendor models
- **Nodes**: Up to 32 appliance tags
- **Relaying**: Tags relay data for tags that are out of hub range

## Data Flow

### Circuit Data (every 500ms)
1. ADS131E08 samples all 16 CT channels at 8 kHz
2. STM32G474 computes RMS values, real power, and power factor per circuit
3. Arc detection algorithm runs on sampled data
4. Packaged into PowerPulse frame, transmitted via Sub-GHz
5. Hub receives, parses, and:
   - Runs local anomaly detection (LSTM autoencoder)
   - Buffers to microSD
   - Publishes to MQTT → cloud
   - Updates local display

### Appliance Data (every 5s)
1. BL0937 measures instantaneous V, I, P, PF
2. nRF52840 accumulates energy (Wh)
3. BLE mesh publishes to hub
4. Hub forwards to cloud via MQTT

### Solar Data (every 10s)
1. INA260 measures PV voltage and current
2. RP2040 runs MPPT algorithm and adjusts buck duty cycle
3. LTC6811 monitors battery cells (via CAN)
4. Packaged and sent via Sub-GHz
5. Hub publishes to cloud for dashboard and optimization

### Alert Path (immediate)
1. Circuit monitor detects arc fault
2. Sends 3x repeated ARC_FAULT_ALERT frame
3. Hub receives, triggers:
   - Local buzzer alarm
   - LED flash
   - Display alert message
   - Cloud push notification (if online)
   - Local logging (always)

## Power Architecture

### Hub Node
- **Main power**: USB-C 5V → MP28167 buck → 3.3V (2A)
- **Backup**: 18650 (3.7V 2600mAh) → TP4056 charger + DW01A protection → 3.3V via auto-switch
- **Runtime on battery**: ~4 hours (WiFi active)

### Circuit Monitor
- **Power**: AC mains → HLK-PM03 (120/240VAC → 5V/600mA) → AP2112 → 3.3V
- **Isolation**: Full galvanic isolation between CT inputs and MCU via ISO7741 digital isolators
- **Power budget**: ~350mA @ 3.3V (well within supply capability)

### Appliance Tag
- **Power**: AC mains → HLK-PM01 (120/240VAC → 5V/600mA) → AMS1117 → 3.3V
- **Standby consumption**: ~30mA @ 3.3V (~100mW)
- **Relay on**: Additional ~15mA for SSR LED

### Solar Node
- **Power**: Battery → MP1584 buck (48V → 5V) → AMS1117 → 3.3V
- **Self-powered from battery bank**
- **Fan**: 12V via dedicated buck, PWM controlled

## Safety Design

### Galvanic Isolation
- **Circuit Monitor**: ISO7741 quad digital isolators create an isolation barrier between the CT/voltage sense side (connected to mains) and the MCU side. The ADS131E08 ADC straddles this barrier, with its SPI interface crossing via digital isolators.
- **Appliance Tag**: BL0937 is an isolated energy measurement IC with internal isolation.
- **Solar Node**: INA260 has no isolation requirement (same ground domain as battery).

### Overcurrent Protection
- Each circuit is inherently protected by its breaker (15-20A)
- Software overcurrent detection triggers alerts at configurable thresholds
- Auto-shed capability via appliance tag relays

### Arc Fault Detection
- Real-time spectral analysis on STM32G474 (170 MHz Cortex-M4F with DSP instructions)
- Detection within 200ms of arc onset
- 3x repeated transmission for reliability
- Visual + audible alarm at hub
- Cloud push notification to mobile app
- UL 1699-aligned sensitivity requirements

## Cloud Infrastructure

### Backend
- **FastAPI** REST API on port 8000
- **TimescaleDB** (PostgreSQL extension) for time-series data
- **Mosquitto** MQTT broker for hub communication
- **Docker Compose** for deployment

### ML Pipeline
- **Arc Fault Detector**: 1D CNN → TFLite Micro → ESP32-S3
- **NILM Disaggregator**: Sequence-to-point CNN → ONNX → Cloud
- **Consumption Forecaster**: Temporal Fusion Transformer → ONNX → Cloud
- **Anomaly Detector**: LSTM Autoencoder → TFLite Micro → ESP32-S3

### Data Retention
- Raw data: 7 days (1-second granularity)
- Downsampled: 90 days (1-minute granularity)
- Aggregated: 2 years (1-hour granularity)
- Alerts: Forever

## Scalability

- **1 Circuit Monitor** supports up to 16 circuits (most homes have 12-24)
- **Up to 32 Appliance Tags** per hub (BLE mesh limit)
- **1 Solar Node** per installation (expandable to multiple)
- **Multiple Hubs** can coexist for large homes (different Sub-GHz channels)
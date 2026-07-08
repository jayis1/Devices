# QuakeGuard — Architecture

## System Overview

QuakeGuard is a multi-node earthquake early-warning and structural safety system consisting of 4 node types:

1. **Hub** (ESP32-S3) — Central coordinator, P-wave CNN, consensus, dispatch
2. **Floor Nodes** (ESP32-S3, 2–8 units) — Distributed MEMS accelerometer sensors
3. **Shutoff Controller** (ESP32-C3) — Motorized gas/water valve control
4. **Structural Tags** (RP2040, 2–6 units) — Battery-powered strain/vibration monitors

## Communication Topology

```
        ┌──────────┐
        │  Cloud   │ (FastAPI + MQTT + TimescaleDB)
        └────▲─────┘
             │ Wi-Fi / 4G LTE
        ┌────┴─────┐
        │   Hub    │ (ESP32-S3)
        └──┬───┬───┘
           │   │     Sub-GHz 868 MHz TDMA mesh
     ┌─────┘   └─────┐
     ▼         ▼     ▼
  Floor×N   Shutoff  Struct×M
```

- **Hub ↔ Cloud**: Wi-Fi (primary) + SIM7000 4G LTE (backup)
- **Hub ↔ Nodes**: Sub-GHz 868 MHz CC1101 (TDMA mesh, 38.4 kBaud)
- **Hub ↔ Mobile**: BLE 5.0 (pairing) + Cloud WebSocket (real-time)

## P-Wave Detection Pipeline

```
Ground motion → ADXL355 (1000 Hz) → Threshold (6σ adaptive)
   → 2 s waveform capture → Sub-GHz TX to Hub
   → Hub CNN (200 ms inference): P-wave? S-wave? Noise?
   → Consensus: 2+ Floor Nodes within 500 ms?
   → P-wave: pre-alert (siren + LED yellow + push)
   → S-wave: SHUTOFF_NOW → gas/water valves + relays
   → Post-event: poll Structural Tags (30 s)
   → Family check-in dispatch (Wi-Fi or cellular)
   → Cloud: event record + USGS cross-validation
   → Cloud: aftershock risk LSTM (72 h forecast)
```

## Seismic Physics

| Wave | Type | Speed | Effect | Detection |
|------|------|-------|--------|-----------|
| P-wave | Compressional | ~6 km/s | Non-destructive | Pre-alert trigger |
| S-wave | Shear | ~3.5 km/s | Destructive | Action trigger (shutoff) |

Lead time = (distance / S-wave speed) - (distance / P-wave speed)
At 50 km: (50/3.5) - (50/6) = 14.3 - 8.3 = **6 s lead time**
At 100 km: (100/3.5) - (100/6) = 28.6 - 16.7 = **12 s lead time**

## Consensus Algorithm

Multi-node consensus prevents false positives from local noise (door slams, footsteps):

1. Floor Node A detects acceleration >6σ → sends SEISMIC_CANDIDATE to Hub
2. Hub starts 500 ms consensus window
3. Hub CNN classifies waveform: P-wave / S-wave / noise
4. If 2+ Floor Nodes trigger within 500 ms → confirmed seismic event
5. Single-node trigger → filtered as local noise (but waveform logged for ML retraining)

## Structural Health Monitoring

Structural Tags continuously monitor:

- **Strain**: 350Ω foil gauge → HX711 24-bit ADC → μStrain (0.1 με resolution)
- **Vibration**: LIS3DH 100 Hz → 256-point FFT → resonance frequency
- **Temperature**: DS18B20 for thermal compensation

An LSTM autoencoder (cloud) detects anomalous patterns:
- Crack propagation (sudden strain step + increasing variance)
- Foundation settlement (accelerating non-linear drift)
- Resonance shift (stiffness loss → frequency change)
- Sensor drift (gradual offset)

Anomaly score > 0.75 → structural alert + civil-engineer report generated.

## Power Architecture

| Node | Primary | Backup | Runtime |
|------|---------|--------|---------|
| Hub | USB-C 5V | 2× 18650 (6800 mAh) | 12+ h |
| Floor Node | USB-C 5V | 1× 18650 (3400 mAh) | 6+ h |
| Shutoff | 12V adapter | 2× 18650 (boost to 12V) | 24+ h |
| Structural Tag | — | 3× CR2032 (1000 mAh) | 12 months |

## Data Flow

```
Floor Node → Hub: SEISMIC_CANDIDATE (2 s waveform, compressed)
Hub → All: SEISMIC_CONFIRMED (broadcast, severity + magnitude)
Hub → Shutoff: SHUTOFF_NOW (action flags)
Shutoff → Hub: SHUTOFF_ACK (valve states + gas readings)
Hub → Cloud: EVENT_CONF (timestamp, magnitude, actions)
Hub → Cloud: STRUCT_REPORT (strain, resonance, anomaly)
Cloud → App: WebSocket push (event, family check-in)
App → Cloud: FAMILY_RESPONSE (safe / need_help)
Cloud → App: Push notification (Firebase FCM)
Cloud: Aftershock risk LSTM inference (72 h forecast)
Cloud: Structural autoencoder inference (daily batch)
```
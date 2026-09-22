# SensorySync — sensory-accessible space regulation

Designed and documented by [jayis1](https://github.com/jayis1).

SensorySync is a local-first, low-voltage system for people who experience sensory overload. It combines room conditions, wearer-initiated context, and explicit preferences to offer calm-space recommendations and bounded comfort controls. It is distinct from general stress and acoustic systems: it targets sensory-accessibility routines with local overrides, no recording, and no behavioural diagnosis.

> **Safety and validation:** This is an unvalidated concept/reference design, not a medical, diagnostic, emergency, fire, security, or behavioural treatment device. It cannot control mains, locks, medication, life-safety systems, or any person. Validate electrical, RF, accessibility, and privacy decisions with qualified professionals before use.

## What it does

- maps light, estimated room sound level, CO2, temperature, and humidity without storing speech;
- lets a Comfort Band wearer explicitly request a break or locally stop outputs;
- presents explainable cards instead of inferring a diagnosis or forcing an action;
- controls only SELV LED, USB fan, pink-noise, and haptic outputs with manual override;
- shows unknown/stale status when hub, broker, radio, or ML is unavailable.

## Nodes

| Node | SoC | Link | Role |
|---|---|---|---|
| Sensory Hub | Raspberry Pi CM4 + ESP32-S3 | Ethernet/Wi-Fi + Sub-GHz | coordinates local event store and API |
| Room Beacon ×N | ESP32-C6 + SX1262 | Sub-GHz TDMA | environmental sensing |
| Comfort Band | nRF52840 | BLE 5.3 + Sub-GHz | wearer input and EDA/activity trend |
| Ambient Controller | STM32G0B1 + SX1262 | Sub-GHz TDMA | SELV light/fan output |
| Quiet Pod | RP2040 + SX1262 | Sub-GHz TDMA | local pink noise and haptic output |

## Architecture, protocol, and hardware

The normative build contract is [docs/system-manifest.json](docs/system-manifest.json). Read [architecture](docs/architecture.md), [protocol](docs/protocol.md), [API](docs/api.md), and [deployment](docs/deployment.md) before assembly. Pin assignments, voltage domains, buses, power limits, and failure paths are in architecture and per-node schematic notes. Each BOM is in [hardware/bom](hardware/bom).

Communication uses versioned, CRC-32C protected TDMA frames and exactly these MQTT topics: `sensorysync/v1/<node_id>/telemetry`, `sensorysync/v1/<node_id>/command`, and `sensorysync/v1/<node_id>/health`. Commands expire in 30 seconds and target only Ambient Controller or Quiet Pod.

## Software and ML

The FastAPI prototype validates telemetry, exposes health/cards, and bounds command requests; it is an in-memory reference and needs persistence and production MQTT/TLS configuration for deployment. `software/ml-pipeline/train.py` is a deterministic synthetic-data smoke baseline with six numeric features (lux, dBA estimate, CO2 ppm, °C, RH %, EDA µS). It makes no performance claim. The React Native prototype reads local hub health and explicitly represents offline state.

## Build and validate

```text
python3 scripts/validate.py
cd software/dashboard && SENSORYSYNC_API_TOKEN=replace-me uvicorn main:app
cd software/ml-pipeline && python3 train.py
```

Compile production firmware with ESP-IDF, nRF Connect SDK, STM32Cube, and Pico SDK after replacing portable HAL references. No credentials, device keys, certificates, audio, or real health/sensory data are included.

## Repository contents

- conceptual per-node schematic connection notes and prototype BOMs;
- portable C protocol and host-tested state-machine references for every node;
- FastAPI dashboard, reproducible ML smoke baseline, and React Native prototype;
- protocol, deployment, privacy, failure-mode, pin, and power documentation.

All artifacts in this directory are authored by jayis1.

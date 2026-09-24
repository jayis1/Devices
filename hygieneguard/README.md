# HygieneGuard — household hand-hygiene and infection-spread prevention

Designed and documented by [jayis1](https://github.com/jayis1).

HygieneGuard helps households and caregivers make handwashing supplies, sink conditions, and room-to-room routines visible without identifying people or recording audio/video. It is distinct from SickDaySync: it measures hygiene-station readiness and supports explicit, voluntary wash events rather than monitoring illness.

> **Safety and validation:** This is an unvalidated reference design, not a medical, diagnostic, access-control, emergency, or behaviour-enforcement device. It never unlocks/locks doors, diagnoses infection, or makes clinical claims. Validate electrical, RF, plumbing, accessibility, and privacy decisions with qualified professionals before building or using it.

## Who it helps

A caregiver managing a busy home checks stations several times daily; the incumbent is memory, disposable signage, and manual soap refilling. Multi-node design matters because a single sensor cannot determine whether separate sinks are ready, whether a dispenser is empty, or whether an optional wash confirmation happened near a doorway. Failure cost is limited to missing guidance: every node shows stale/unknown status and people retain ordinary manual handwashing.

## Architecture

| Node | SoC | Link | Job |
|---|---|---|---|
| Hygiene Hub | Raspberry Pi CM4 + ESP32-C6 | Ethernet/Wi-Fi, Sub-GHz | local API, MQTT bridge, policy |
| Sink Sentinel ×N | ESP32-C6 + SX1262 | Sub-GHz TDMA | water flow, ambient temperature, soap-area readiness button |
| Smart Dispenser ×N | STM32G0B1 + SX1262 | Sub-GHz TDMA | load-cell soap level and manual pump use |
| Door Beacon ×N | nRF52840 + SX1262 | BLE + Sub-GHz TDMA | voluntary button feedback and supply reminder display |

The normative design contract is [docs/architecture.md](docs/architecture.md). Frames are versioned and CRC-protected; see [docs/protocol.md](docs/protocol.md). The prototype uses only `hygieneguard/v1/<node_id>/{telemetry,command,health}` MQTT topics.

## Hardware Nodes and BOM

The Hub is the coordinator and local gateway; Sink Sentinels observe station conditions; Smart Dispensers report supply; Door Beacons give optional, non-identifying feedback. Full per-node BOMs with real manufacturer part numbers are in [hardware/bom](hardware/bom). Pin maps, buses, voltage domains, and power limits are in [docs/architecture.md](docs/architecture.md) and the [schematic notes](schematic/).

## Firmware

`firmware/common/` defines the bounded versioned frame and CRC-32C validator. The Hub rejects replayed/invalid frames. Sink, dispenser, and beacon references cover telemetry, refill threshold, and expiring display-command logic; production HALs must be integrated and hardware-tested per target SDK.

## Cloud Backend

The FastAPI prototype exposes local health, stations, cards, and authenticated telemetry ingest. MQTT is an integration boundary, not an embedded public broker; deploy TLS, ACLs, persistence, rate limits, and audit policy before exposure.

## ML Pipeline

The training script generates deterministic synthetic supply/flow/temperature data and fits a small standard-library logistic baseline for refill triage. It has no people, medical data, hygiene-compliance label, or performance claim.

## Mobile App

The React Native/Expo scaffold reads local hub health and explicitly displays offline/unknown state. It never identifies a user or issues physical-control commands.

## Deployment

Follow [docs/deployment.md](docs/deployment.md) for calibration, key provisioning, and radio-loss tests. Keep all electronics outside wet zones and ensure routine operation never depends on the system.

## Hardware, firmware, and software

Each node has pin and power assignments in `schematic/`; connection notes are human-readable reference schematics, not fabrication-ready KiCad projects. BOMs name manufacturer part numbers. Portable C references share `firmware/common/hygieneguard_protocol.h`; platform SDK integration is deliberately left to ESP-IDF, STM32Cube, nRF Connect SDK, and Pico/Linux maintainers.

`software/dashboard/main.py` is a FastAPI in-memory reference backend. `software/ml-pipeline/train.py` is a deterministic synthetic-data smoke baseline for refill-risk triage, not a clinical or performance model. `software/mobile-app/` is a React Native offline-aware scaffold.

## Build and validate

```text
python3 scripts/validate.py
cd software/dashboard && HYGIENEGUARD_API_TOKEN=replace-me uvicorn main:app
cd software/ml-pipeline && python3 train.py
```

Use unique credentials provisioned outside this repository, TLS to the broker/API, and per-node topic ACLs. No credentials, identity records, audio/video, or medical data are included.

## Quality rubric

| Criterion | Score | Basis |
|---|---:|---|
| Novelty | 4/5 | hygiene-station readiness and voluntary routing are not represented by existing systems |
| Daily value | 4/5 | reduces forgotten refills and supports repeatable household care routines |
| Buildability | 3/5 | reference electronics and firmware require hardware review and bench validation |

All artifacts in this directory are authored by jayis1.
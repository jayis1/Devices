# CommonSignal — accessible community-room readiness

Status: concept design. Firmware is host-testable where noted; no hardware fabricated, no KiCad/ERC or RF validation performed. Not a safety, emergency, occupancy enforcement, or medical system.

## Problem brief
A volunteer coordinator opening a neighborhood meeting room often cannot quickly tell whether the room is usable: the accessible route is obstructed, a folding table is missing, or the room is too loud/hot. Today this is discovered by walking the room or waiting for a complaint. CommonSignal aims to cut opening-check time to under five minutes while preserving anonymity. Multi-node sensing is justified because the route, shared equipment, and room climate are spatially separated and must be checked together without cameras or personal tracking.

## Novelty
The repository already covers community heat-site readiness (Heat Relief Network), pantry inventory (PantryLink), library seating (LibraryLoop), tool availability (ToolShare), queue estimates (QueueKind), repair-bench readiness (RepairLoop), and park assets (ParkPulse). This design is specifically an indoor, volunteer-run community-room opening checklist: it senses route clearance, equipment presence, and ambient comfort; it neither counts people nor tracks users. It differs from LibraryLoop (seat utilization) and RepairLoop (tool/bench readiness). Accessibility is advisory: a human must inspect the route.

## User journeys
1. Setup: steward places the three low-voltage nodes, scans local pairing code, and defines acceptable route/equipment thresholds.
2. Normal opening: hub shows route-clear confidence, equipment status, and comfort; steward confirms checklist and records a manual correction.
3. Failure: node loss marks its signal unknown (never clear); local hub continues, queues bounded events, and displays a visible fault. No cloud or ML output can clear an obstacle.

## Architecture
```text
Route Sentinel (VL53L1X + ESP32-C6) ─┐
Equipment Tag (reed/load input + nRF52840) ─┼─ BLE 5 star ─ Hub (RP2040 + ESP32-C3) ─ MQTT/TLS ─ FastAPI
Room Climate (SHT31 + ESP32-C6) ──────┘                                      ├─ ML anomaly hints
                                                                              └─ React Native steward app
```
Hub aggregates versioned observations, not identities. Nodes never capture audio/video. BLE peer pairing and MQTT credentials are provisioning requirements; this reference implementation does not implement cryptographic provisioning or BLE radio drivers. No actuator controls are provided.

## Nodes and interfaces
All pin assignments are proposed GPIOs requiring datasheet and board-level verification. Schematic directory contains explicit conceptual net tables, not fabricated KiCad files.

| Node | SoC / parts | Proposed pins and buses | Power and safe behavior |
|---|---|---|---|
| Hub | Raspberry Pi Pico W (RP2040 + onboard CYW43439); USB power | UART0 GP0/1 to ESP32-C3 module (radio coprocessor); I2C GP4/5 optional OLED; SWD pads | 5V USB input; 3.3V logic; radio loss shows stale/unknown; RP2040 has no internal flash, module includes flash |
| Route Sentinel | ESP32-C6-WROOM-1; VL53L1X ToF, 3.3V | I2C SDA GPIO6/SCL GPIO7, XSHUT GPIO4; BLE radio onboard | 5V USB input to regulated module board; sensor fault = unknown; no obstacle-absence guarantee |
| Equipment Tag | nRF52840 module; reed switch, optional load-cell interface HX711 | GPIO P0.11 reed with pull-up; P0.26/P0.27 HX711 DOUT/SCK; SWD | 3.0V AA pack (2× alkaline), no charging; open/short reports fault; no latch/lock |
| Climate Node | ESP32-C6-WROOM-1; SHT31-D | I2C GPIO6/7; BLE radio onboard | 5V USB; sensor failure reported, never substituted with a normal value |

The route sensor needs a fixed, documented mounting geometry and a calibration target. It detects a configured beam/zone interruption, not legal accessibility or every obstacle. GPIO and carrier-module details must be checked against the selected vendor board before fabrication.

## Power and BOM estimate
Hub and powered sensors use certified, enclosed 5V USB supplies; user-facing circuitry remains SELV. Estimated quantities/costs are indicative retail, not quotes: hub $30; route node $24; equipment tag $18; climate node $20; enclosure/cables/mounts $45; total approximately $137 (qty 1, excluding phone and labor). A clipboard checklist costs less; the value case is faster, repeatable opening checks. Target install <1 hour.

Battery life is not claimed: tag current, radio duty cycle, and cell capacity require measurement. Do not use rechargeable cells without a matched charger. Do not connect mains loads. Observe local radio regulations; no RF range claim is made.

## Protocol and software
Shared frame: `A5 5A | version:u8 | type:u8 | node:u16le | seq:u16le | uptime_s:u32le | payload_len:u8 | payload | crc16-ccitt:u16le`. CRC is corruption detection, not authentication. Payload is UTF-8 JSON for prototype interoperability, capped at 96 bytes. Message types: 1 heartbeat, 2 route observation (clear/blocked/unknown), 3 equipment observation (present/absent/unknown), 4 climate (temperature centi-degrees C, RH centi-percent), 5 fault. Sequence duplicates are discarded; gaps are reported. BLE link security is a required deployment feature, not implemented here.

MQTT topics: `commonsignal/<site_id>/<node_type>/<node_id>/telemetry` (QoS 1, non-retained). The gateway queues at most 256 events and drops oldest telemetry with a visible counter when full. Backend validates bounds and persists nothing in this stub. API exposes `GET /health` and `POST /telemetry`; authentication/TLS must be configured by deployment. App is a UI stub. ML script demonstrates synthetic anomaly scoring only; it cannot mark a route clear.

## Safety, privacy, and limitations
No camera, microphone, biometric, or person-counting data. Retain only device observations; site operator controls deletion. Not a certified accessibility inspection, fire/life-safety sensor, security system, or substitute for a human walkthrough. Offline hub is authoritative for faults; cloud and ML are optional. Threat model includes spoofed/replayed radio and unauthorized broker clients; production needs per-device identity, authenticated encryption/replay protection, TLS certificate validation, topic ACLs, API auth, signed OTA, and key rotation. Those are not implemented in this prototype.

## Build and calibration
See `docs/architecture.md`, `docs/protocol.md`, and `docs/api.md`. `firmware/common/protocol.c` is portable C and can be compiled with a host compiler. Node `main.c` files are SDK integration skeletons, not full drivers. Calibrate the route sensor with a fixed mount and known clear/blocked target; mark unreliable geometry unknown. Run `python3 -m unittest discover -s software/dashboard/tests` and `python3 -m py_compile software/dashboard/app.py software/ml-pipeline/train.py`.

## Readiness
Concept complete; no physical build, electrical validation, production security, cloud deployment, mobile build, or real-world ML validation. BOMs are estimates; verify exact module/carrier MPN and pinout before procurement. No performance or accuracy claims.
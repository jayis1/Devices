# MarketChill — vendor cold-chain exception coordination

Status: engineering prototype specification. No food-safety certification or field validation is claimed. Author: jayis1.

## User and value

For a small market-stall operator moving chilled goods between a walk-in refrigerator and a temporary sales table. Without MarketChill, they repeatedly check separate thermometers and may discover a cooling failure late; with it, a local hub correlates calibrated temperature probes with door events and gives an audible, actionable exception while preserving a time-stamped excursion record. It does not decide whether food is safe to sell.

## Scope and distinction

Unlike FreshKeep (household inventory/spoilage) and MealBridge (shared-kitchen inventory), this is a portable, vendor-operated cold-chain excursion logger for temporary market setups. Its mechanism is event-correlated temperature monitoring with local alarm and auditable operator acknowledgment; no cameras, customer tracking, autonomous disposition, or compressor control. Three node types: Wi-Fi hub, BLE probe node, and BLE door-event node.

## Architecture

```text
[Probe: ESP32-C3 + TMP117] --BLE advertisements--> [Hub: ESP32-S3]
[Door: ESP32-C3 + reed switch] --BLE advertisements--/     | Wi-Fi/MQTT (optional)
                                                        [FastAPI service]
                                                             | REST
                                                       [Mobile UI stub]
```

Hub is authoritative for alarms and buffers records locally during internet loss. BLE observations are unsigned prototype telemetry; production deployment requires authenticated pairing and a reviewed security design. Cloud is optional and never in the alarm path. The door node reports open/closed transitions, not people. A lost or stale probe produces a sensor-fault alarm, never a safe-temperature assumption.

## Node design

| Node | SoC / sensing | Link and behavior | Power assumption |
|---|---|---|---|
| Hub | ESP32-S3-WROOM-1; status LED, buzzer; local flash queue | BLE scan, Wi-Fi MQTT; local thresholds and acknowledgment | Regulated 5 V USB-C; <1 W design target, measure in prototype |
| Probe | ESP32-C3-MINI-1; TMP117 I²C temperature sensor | BLE manufacturer-data frame every 10 s; sealed probe enclosure | 2×AA lithium primary through regulated 3.3 V; target >6 months, unverified |
| Door | ESP32-C3-MINI-1; normally-closed reed input | BLE state/event frame every 10 s and on transition | 2×AA lithium primary; target >6 months, unverified |

Use development modules/boards first; the pin map and BOM identify interface intent, not a released PCB. TMP117 is a digital temperature sensor; a production food-contact probe needs a separately validated thermowell and calibration. Never immerse electronics. Door switch wiring uses GPIO4 to ground with internal pull-up; probe SDA GPIO6, SCL GPIO7. Hub GPIO8 drives buzzer through a transistor; GPIO9 LED through resistor. Avoid GPIO strapping pins for external loads.

## Protocol and operation

See [protocol](docs/PROTOCOL.md), [architecture and safety](docs/ARCHITECTURE.md), [API](docs/API.md), and [power assumptions](docs/POWER.md). The frame carries schema version, node kind, 16-bit sequence, temperature in centi-degrees C (probe only), door state, and battery millivolts. BLE is best-effort and unauthenticated in this prototype; discard malformed/old frames, deduplicate repeated sequence values, timestamp at reception, and flag stale data after 60 seconds. Local threshold is configurable (default 5.0°C solely as a configurable example); follow applicable local food rules and site HACCP plan. No claim of legal compliance.

## Software and ML

Portable C protocol codec has unit tests; per-node `main.c` files document board wiring and integration points, not complete ESP-IDF applications. FastAPI validates telemetry and provides local endpoints; MQTT bridge is a development adapter with no broker configured by default. Mobile app is an API-connected UI scaffold. `software/ml-pipeline/train.py` trains a reproducible synthetic-data baseline for excursion classification; it is not trained on real food data and must not be used to decide product safety. Deterministic threshold alarms remain authoritative.

## Failure, privacy, and maintenance

Hub power loss disables alarms and logging; use a separate calibrated thermometer and manual checks. Internet loss leaves BLE, local alarms, and local queue available. Probe loss/staleness raises sensor fault; door loss removes door context but does not suppress temperature alarms. Buzzer has a physical silence button in a future enclosure; operator acknowledgment does not clear the underlying condition. No identifying data, audio, images, or customer data is collected. Keep records locally by default; define retention with the operator. Calibrate probes against a traceable reference before service and at a documented interval; clean only the external compatible probe assembly. Firmware update/rollback and production BLE authentication are future release gates.

## Build/verification limits

The repository contains schematic design notes and BOMs, not validated KiCad electrical schematics or PCB layouts. Verify electrical design, antenna placement, battery runtime, calibration, EMC, enclosure/IP rating, and all applicable food-safety requirements before deployment. No physical hardware validation is claimed.

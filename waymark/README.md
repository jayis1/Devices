# Waymark — public tactile wayfinding readiness

Status: reference prototype; not field-tested, accessibility-certified, or a navigation/safety product. Invented and maintained by [jayis1](https://github.com/jayis1).

## User and value

A municipal accessibility coordinator gets recurring reports that tactile street-name plates, public-building direction markers, or audible-crossing call-button labels are missing or damaged, but inspections are infrequent and reports lack precise location/context. Waymark combines fixed landmark checks with a staff inspection beacon so a steward can prioritize a human visit. Primary outcome to test: reduce time from a reported missing landmark to a confirmed inspection; no reduction claim has yet been measured. Unlike CurbCue (curb-ramp obstruction) and LiftLink (elevator availability), Waymark observes stationary tactile/wayfinding marker condition and never tracks travelers or infers route safety.

## Architecture

```text
[Landmark marker node] --BLE advertisements--> [Site gateway]
[Staff inspection beacon] -----------BLE------>      | local SQLite queue
                                                     +--MQTT/TLS--> FastAPI
                                                     +--React Native staff stub
```

Nodes are physically distributed along a pedestrian route; removing one removes visibility at that landmark. Gateway stores bounded events locally when network is unavailable. Humans inspect and confirm conditions; no user/visitor data is collected. No actuation.

## Nodes and reference pin map

| Node | Reference components | Pin/interface | Power / purpose |
|---|---|---|---|
| Gateway | Raspberry Pi Pico W (RP2040/CYW43439) | USB; Wi-Fi upstream; BLE scanning via external USB BLE adapter (not included) | Isolated certified 5 V USB adapter; local event queue |
| Marker node | ESP32-C3-MINI-1; VL53L0X distance sensor; sealed service button | I²C SDA GPIO4/SCL GPIO5; button GPIO6; 3V3 | 2×AA candidate, sleep/runtime TBD; checks marker presence/obstruction at a fixed mount |
| Inspection beacon | ESP32-C3-MINI-1; button, LED | Button GPIO4; LED GPIO5 | USB or 2×AA; staff-confirmed inspection event |

Pin assignments are design candidates and must be reconciled against exact module datasheets and the selected development board before layout. Sensor readings indicate local proximity only, not tactile quality or accessibility compliance. BLE event payloads are small JSON frames; production pairing/authentication is not implemented.

## Software and data

`firmware/` contains a portable C frame codec and node examples. `software/dashboard/` is a FastAPI service with SQLite event storage and HTTP ingestion; MQTT deployment adapter is documented as a planned integration, not claimed implemented. `software/ml-pipeline/` demonstrates synthetic-data anomaly scoring only; deterministic threshold rules and staff confirmation are the baseline. `software/mobile-app/` is a UI stub. No production inference, user accounts, or security controls are present.

## Safety, privacy, failure behavior

No cameras, microphones, location tracking, route guidance, alarms, or actuation. Events identify only an installed node and coarse condition. Gateway loss means no new upstream events; nodes continue local sensing/advertising and staff can inspect manually. Cloud outage queues events at the gateway with bounded retention. False positives, sensor drift, mounting changes, weather and radio interference require field evaluation. Never use readings to certify an accessible route or replace required inspections. Provide accessible human reporting alternatives.

## Artifact status

| Artifact | Status |
|---|---|
| Architecture, pin/net plans and BOMs | DRAFT; electrical review and sourcing verification required |
| C protocol codec | WORKING on host when compiled/tested; no target firmware build claimed |
| Node firmware | DRAFT reference loops; hardware drivers/radio integration pending |
| FastAPI dashboard | DRAFT; MQTT adapter not implemented |
| ML | Synthetic demonstration only; no real data/performance claim |
| React Native | STUB |
| KiCad | Not supplied; textual schematic/net plans only |
| Physical hardware | Not built or tested |

See `docs/architecture.md`, `docs/protocol.md`, `docs/safety.md`. Costs and battery runtime are not asserted. jayis1 is the author; implementation is unvalidated.
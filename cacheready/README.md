# CacheReady

Design status: reference prototype, not a certified emergency system. No deployment, inventory accuracy, battery runtime, radio range, or life-safety performance has been validated. CacheReady helps volunteer stewards keep shared neighborhood emergency-supply cabinets organized between human inspections; it does not dispatch aid or determine that supplies are safe to use.

## User and problem

A volunteer arriving at a neighborhood supply cache after a storm may find missing water, expired batteries, or a cabinet that was opened since its last inspection. Paper checklists get stale, while camera-based systems would intrude on people seeking help. CacheReady records coarse cabinet condition and staff-confirmed stock checks without identifying visitors.

## Before and after

Before: periodic manual checks with no dependable record of cabinet openings, temperature excursions, or which checklist is overdue. After: a local gateway shows last-seen sensor state, staff enter confirmed counts and expiry dates, and overdue checks generate reminders. Sensors never claim that supplies are present or usable.

## Why multiple nodes

A gateway may be indoors or offline; separate cabinet door/temperature and room-power nodes keep observations near the assets and detect missing telemetry. The system is useful during internet outages and does not actuate locks, alarms, or supplies.

## Nodes

| Node | Role / components | Power and link |
|---|---|---|
| Cache Hub | Raspberry Pi Zero 2 W gateway; USB-C 5 V supply; local SQLite, Wi-Fi uplink; RFM95W 915 MHz radio via SPI | 5 V, mains adapter; Sub-GHz star to sensors |
| Cabinet Sentinel | ESP32-C3-MINI-1, reed switch, TMP117 temperature sensor, RFM95W | Protected 18650 cell + regulator (prototype); Sub-GHz telemetry |
| Utility Sentinel | ESP32-C3-MINI-1, INA219 low-voltage supply monitor, SHT40, RFM95W | USB 5 V; Sub-GHz telemetry |

Parts and pin maps are preliminary reference selections. Confirm exact board/module variants, antenna, regional radio rules, power protection, and footprints before fabrication. 915 MHz is a regional example only; use the legal ISM band and matching radio variant for deployment location. No mains wiring is included.

## Architecture and event path

```text
Reed + TMP117 ─┐
                ├─ Sub-GHz star → Hub (validate, timestamp, SQLite) → MQTT/TLS → API → steward app
INA219 + SHT40 ┘                         │
                                      offline UI
```

Sentinels send versioned, bounded telemetry at a configured interval and on door changes. The hub deduplicates by node ID and sequence number, rejects stale/invalid frames, and stores last-known state. The hub has no remote actuator command path. Radio loss is shown as stale data, not interpreted as a normal reading. Staff-entered counts and expiry checks remain the source of inventory truth.

## Data, privacy, and security

No camera, microphone, visitor counter, or occupant identity. Raw sensor observations stay on the gateway; only configured site-level condition and staff-entered inventory records may be synchronized. Retention is configurable; suggested default is 30 days of sensor history and indefinite staff audit records subject to local policy. Use per-node keys in deployment, authenticated MQTT over TLS, restricted topic ACLs, unique enrollment, revocation, and signed firmware updates where the selected bootloader supports them. The example code does not implement production cryptography, provisioning, or OTA security; CRC is error detection only. Never place secrets in this repository.

## Failure behavior and safety

- Internet unavailable: gateway stores local observations; local dashboard remains available.
- Hub unavailable: nodes continue sampling and retry with bounded backoff; UI reports no recent data.
- Sensor disconnected or implausible: mark that field invalid; do not infer stock status.
- Duplicate/out-of-order frame: reject by sequence; show last valid observation.
- Battery low: report service-needed; staff still perform the physical checklist.
- ML wrong or unavailable: no operational decision depends on ML.

CacheReady is an administrative reminder aid, not a substitute for emergency planning, inspection, supply verification, or emergency services.

## BOM and cost target

Indicative prototype subtotal target: Hub $35–60; each Cabinet Sentinel $25–45; Utility Sentinel $25–45, excluding enclosure, labor, shipping, and regional radio compliance. These are planning estimates, not quotes. Per-node BOM CSVs list reference parts; select protected cells and suitable enclosures through qualified review.

## Firmware, backend, ML, and app

Portable C telemetry framing and host-checkable node state examples are under `firmware/`. The FastAPI example accepts simulated observations and stores them in SQLite. The ML script demonstrates a synthetic overdue-inspection baseline only; it is not trained on real cache data and must not be used to make safety decisions. The React Native directory is an explicitly labeled UI stub, not a production app.

## Build / validation status

See `docs/architecture.md`, `docs/protocol.md`, and `docs/api.md`. The `schematic/` directories contain pin/net design notes, not native KiCad schematics; electrical-rule checking and PCB design remain outstanding. No target SDK build, physical test, radio certification, security audit, or production mobile build is claimed. Host C syntax and Python tests are the available validation scope.

# LabSignal — community science-lab readiness and instrument context

LabSignal is a local-first, non-safety system for volunteer-run school and community science labs. It makes shared instrument readiness and calibration status visible before a class starts, reducing setup delays without tracking students or making claims about measurement validity. It is distinct from WorkshopSync (general workshop tool/PPE readiness): LabSignal focuses on scientific instrument calibration records and shared bench consumables, with no people sensing or tool-control.

## User value

Persona: Jo coordinates a weekly after-school chemistry club and spends setup time discovering a pH meter with stale calibration or missing buffer solution. Current workaround is a paper sign-out sheet and manual inspection; the target is fewer delayed starts, measured by staff-recorded setup minutes per session (no outcome has been measured). Multiple separated nodes are needed because the instruments live on movable benches, while the gateway/display is by the lab entrance and the dock measures stock at a separate prep counter.

Non-tech alternative is a clipboard; it remains the authoritative calibration record. This system adds timestamped reminders and visible context, not a replacement for required calibration or competent supervision. Target prototype BOM: USD $80–160, excluding enclosure, labor, reference calibration equipment, and instruments; a club or school would fund it as a reusable operations aid. Cloud down: gateway retains recent status locally. Phone ignored: e-paper/LED and a physical acknowledge button remain usable.

## Topology

```text
[Instrument tag: voltage/current + operator check-in] --Sub-GHz--> [Gateway + local display + cache]
[Prep dock: buffer/consumable presence + manual stock] --Sub-GHz-->       | optional MQTT/TLS
                                                                         + optional API/app
```

Intended topology (not yet implemented): gateway with local display, plus a sensor node and bench dock. BOMs name RP2040 Pico W modules, INA219/ADS1115 breakouts, and an optional regional SX1262 module as candidate components. Current firmware files only provide portable frame/event interfaces and do not implement board drivers, radio transport, display, or sensor acquisition. Therefore no live node-to-gateway communication is currently claimed. No student identity, audio, video, or occupancy sensing.

## Node electrical intent

| Unit | SoC / interfaces | Prototype pin intent | Power and boundary |
|---|---|---|---|
| Gateway (intended) | RP2040; candidate regional SX1262 and display | Candidate SPI SCK18/MOSI19/MISO16/CS17; I²C SDA4/SCL5 | USB 5 V; offline cache/display not implemented |
| Sensor node (intended) | RP2040; candidate INA219 and ADS1115 | Candidate I²C SDA4/SCL5 | USB 5 V; drivers and input front-end not implemented |
| Bench dock (intended) | RP2040; candidate dry-contact switches/button | Candidate GPIO10/11 | USB 5 V; event collection not implemented |

These are pin-level design intents, not confirmed board-revision schematics. ADS1115 input range and front-end must be engineered for each probe; never connect an electrode directly without an appropriate high-impedance conditioning circuit. The source includes portable protocol/reference functions, not vendor SDK integrations. No measurement can be called calibrated unless performed with traceable standards and recorded by a qualified operator. Do not use this system for safety decisions, clinical work, or release of experimental results.

## Protocol and API

Authoritative binary frame in `firmware/common/protocol.h`: 12 bytes, version(1), kind(1), sequence LE32, signed value_milli LE32, CRC-16/CCITT-FALSE LE16 over first 10 bytes. Kind 1 sensor context, 2 dock event; reserved values rejected. CRC detects corruption, not malicious forgery. Authentication, encryption, provisioning, replay protection, and secure OTA are not implemented; radio deployment is lab prototype only. Sequence counters are monotonic per node; gateway should reject duplicates and keep last-seen state in a bounded local cache. Unsynchronized time is represented as unknown, not fabricated.

Intended MQTT topic: `labsignal/v1/{site_id}/{node_id}/telemetry`; no retained commands or actuation topics. The current backend does not connect to MQTT; only the API prototype endpoint `GET /health` and `POST /events` exists. Production requires TLS, authentication, site authorization, rate limits, broker ACLs, and retention controls. The dashboard scaffold is not exposed to untrusted networks.

## Data, ML, and failure behavior

Only instrument metadata, coarse voltage/current context, stock state, operator-entered calibration date, and node health. Local history target 7 days, configurable; cloud disabled by default. A failed sensor, radio loss, expired calibration reminder, or invalid frame becomes UNKNOWN and requires manual inspection. Gateway failure does not change instrument state. The ML pipeline is deliberately a synthetic-data smoke-test only: threshold rules and manual records are sufficient for this use case; no model or performance claim is made.

## BOM and maturity

Node BOM CSV files are in `hardware/bom/`; schematic folders contain net/pin-intent documents and are not native KiCad schematics. BOM selections are indicative; verify exact module revisions, datasheets, sourcing, isolation, protection, enclosures, and RF regulations before procurement. Prototype BOM excludes calibration standards and instruments.

| Component | Status |
|---|---|
| Shared frame encode/decode | Implemented and host-testable |
| Node source | Portable interface examples; no target SDK/driver implementation |
| Native schematics / PCB | Not implemented; pin intent only |
| Backend/MQTT | Prototype scaffold; production security not implemented |
| ML | Synthetic smoke test only; not a validated model |
| React Native | Minimal stub |
| Hardware validation | Not performed |

## Build and verification

Run from this directory: `gcc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/labsignal-test && /tmp/labsignal-test`. Python smoke checks: `python3 -m py_compile software/dashboard/app.py software/ml-pipeline/train.py`; CSV validation may use Python's `csv` module. No hardware has been assembled, calibrated, field-tested, or certified. Review datasheets and perform electrical, RF, privacy, and safety review before any real deployment.

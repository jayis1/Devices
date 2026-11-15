# BorrowBench — community tool-library coordination

Status: engineering concept; reference interfaces and software examples, not electrically validated or build-ready. Invented and maintained by jayis1.

BorrowBench helps volunteer-run tool libraries answer: which item is physically present, which checkout or return needs staff attention, and what should be inspected before relending? Unlike ToolShare (shelf availability), MaintainSync (predictive home maintenance), and WorkshopSync (workshop readiness), BorrowBench coordinates an explicit staff-mediated lifecycle for individual shared tools. It does not identify borrowers, decide whether a tool is safe, or unlock/dispense equipment.

## Topology and behavior

A local Site Hub coordinates BLE 5 LE sensors and a staff-operated interface. Tool Dock nodes sense tagged-item presence and detect placement/removal transitions. A Return Scale records aggregate deposited mass as an optional discrepancy cue; it cannot identify an item. An Inspection Beacon is a physical, staff-operated button/LED station that records human-confirmed inspection status. The hub stores an append-only local SQLite event log and serves the LAN API while internet is down. Optional MQTT/TLS forwarding is disabled by default. Only explicit staff actions create checkout/return records; RF observations are advisory. No camera, microphone, person tracking, or actuator is present.

## Prototype hardware outline

All nodes use ESP32-C3-MINI-1 (3.3 V, BLE 5); the hub is Raspberry Pi Zero 2 W (5 V USB). Tool Dock uses a PN532 breakout over I²C (SDA GPIO4, SCL GPIO5, address 0x24/0x48 per breakout strap—confirm module convention), reed switch GPIO6 to ground with pull-up, and WS2812 status LED GPIO7 through 330 Ω. A tool receives a passive, uniquely encoded NFC tag; dock reads the tag only when placed close to the reader. Return Scale uses four 5 kg half-bridge load cells as a full bridge into NAU7802 (SDA4/SCL5, DRDY GPIO6), with a stable 5 V bridge supply and logic-level I²C at 3.3 V. Inspection Beacon has a normally-open button on GPIO4, status LED GPIO7 through 330 Ω, and no remote actuation. Pin selections are a board-level proposal; check boot strapping and exact module carrier before PCB layout.

Power: nodes use listed 5 V USB supplies; no battery charging. Hub uses its own 5 V USB supply. Add input fuse, reverse-polarity protection and local decoupling on custom boards; do not connect mains or motor loads. BLE advertisements carry no borrower identity. NFC IDs are random asset tokens, not names. Radio coverage, EMC, RF regional compliance and BOM availability are unverified.

## Interface / protocol

Firmware emits a bounded CBOR-compatible JSON envelope in examples: `{v:1,id,seq,kind,ts_ms,payload}`. `id` is a provisioned random node ID; sequence is monotonic in RAM and must be persisted in production. Hub rejects unknown versions, oversized payloads (>512 bytes), malformed values, duplicate `(id,seq)` within the retained event window and events older than 24 h. BLE pairing is physical-button initiated; production requires LE Secure Connections and per-node credentials. MQTT, if enabled, uses TLS and per-site broker ACLs; devices do not connect directly to MQTT. No real credentials are included.

## Contents and evidence

- `firmware/`: portable C event validation and minimal node event producers; sensor/radio drivers are integration stubs.
- `hardware/bom/`: prototype BOM CSVs; quantities and compatibility need procurement review.
- `schematic/`: pin and wiring reference, not native KiCad or fabrication data.
- `software/dashboard/`: runnable FastAPI local event API and optional MQTT boundary stub.
- `software/ml-pipeline/`: synthetic event-triage demonstration only; no accuracy claims.
- `software/mobile-app/`: React Native UI stub for staff review; not a functional app build.
- `docs/`: architecture, API, protocol and operation constraints.

Run backend tests with `cd software/dashboard && python3 -m unittest -v`. Firmware has portable host tests via `cc -std=c11 -Wall -Wextra -Werror firmware/common/protocol_test.c -o /tmp/borrowbench-test && /tmp/borrowbench-test`. No hardware, radio, KiCad, mobile build, security, or electrical safety validation is claimed. Use human inspection and the library's established safety procedures before any tool is lent.

# StepWise — household stairway readiness

A local-first, advisory system for documenting changing household stairway conditions for residents and visiting helpers. It does not detect falls, certify code compliance, or control locks or building equipment. The useful outcome is simple: a resident can see whether a route has been checked recently and what an observer reported (for example, a loose handrail or an item on a landing).

## Nodes and workflow

- Home Hub concept: Raspberry Pi Zero 2 W with local Mosquitto, FastAPI and SQLite planned; current API reference is in-memory only and does not yet subscribe to MQTT or persist data.
- Landing Beacon: ESP32-C6 dev board, button, status LED; volunteers/residents can submit a manual clear/needs-attention check-in.
- Route Beacon: ESP32-C6 dev board, button and status LED at a second landing/entry; same protocol, distinct provisioned ID.

Nodes communicate over Wi-Fi MQTT on the local network (TLS and per-device credentials required in deployment). The reference firmware exposes the event contract and GPIO mapping; it deliberately does not contain production credentials or pretend to implement secure provisioning. A prototype broker must use an isolated network. Hub receipt time is authoritative; stale observations are visibly marked. Internet is not required for local reporting. On loss of hub/Wi-Fi, the node indicates failure and requires a repeat check-in after reconnect; no false success is shown.

## Build boundary

This repository contains a prototype reference design, not a certified accessibility or building-safety product. There are no fabricated PCB layouts, validated environmental enclosures, measured RF range, or field-study results. Use low-voltage USB-powered development boards only. Do not connect to building wiring or actuators. Human inspection remains mandatory; reports are subjective and never establish that a route is safe or accessible.

## Architecture / electrical design

Each beacon uses an ESP32-C6-DevKitC-1 powered by regulated 5 V USB. GPIO 9 is a momentary active-low check-in button to ground with internal pull-up; GPIO 8 drives a low-current LED through 1 kΩ to ground; USB-UART programming/debug is via the board's native connector. The dev board provides regulation, reset and decoupling; no external sensor bus or battery charger is fitted. Do not exceed the board's GPIO ratings. The schematic directory contains the pin-level reference design and explicit review status. BOM CSVs identify the board and discrete parts; verify the exact board revision before procurement.

The intended hub data path is button event → versioned JSON event → MQTT QoS 1 → FastAPI validation and storage → read-only mobile status view. The current API reference validates HTTP events in process memory; MQTT ingestion, SQLite persistence and authentication are not implemented. It rejects unknown node IDs and invalid enum values. No autonomous control path exists.

## Software

- `firmware/beacon/main.c`: ESP-IDF GPIO bring-up only; event transport, Wi-Fi, MQTT and provisioning are not implemented.
- `firmware/common/event.h`: shared event vocabulary.
- `software/dashboard`: FastAPI in-memory HTTP reference. `GET /reports` returns recent check-ins; `POST /events` validates event bodies for local testing. No durable storage, MQTT subscriber, retention cleanup or authentication exists.
- `software/mobile-app`: React Native display stub. Pairing and push notifications are not implemented.
- `software/ml-pipeline`: experimental baseline to flag unusually long gaps between check-ins; synthetic fixtures only, not a safety classifier or validated model.

## Bring-up

1. Flash GPIO demo with ESP-IDF (target `esp32c6`); it currently only mirrors the button to LED.
2. Run the HTTP reference API locally; it is suitable only for local tests, not deployment.
3. Submit validated event examples to the API and verify duplicate/unknown IDs are rejected. No over-the-air behavior is implemented.
4. Inspect routes directly. Record whether a report is stale and resolve reported issues through normal human processes.

## Validation and limitations

Run backend checks with `python -m pytest software/dashboard`; syntax-check Python with `python -m compileall`. Firmware requires Espressif ESP-IDF and has not been compiled in this environment. KiCad/ERC requires conversion of the documented reference schematic into a reviewed project; no fabrication-ready KiCad project is claimed. Synthetic ML examples do not support predictive or safety claims. Test power loss, hub restart, duplicate/out-of-order events, broker outage, revoked device credentials and stale reports before any pilot. No personal identity, audio, image, or location data is collected.

See `docs/architecture.md`, `docs/protocol.md`, and `docs/api.md` for details. Invented and maintained by jayis1.

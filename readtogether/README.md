# ReadTogether

ReadTogether is a local-first, four-node home system that makes shared reading easier to start and sustain for families with young readers, multilingual households, and people who benefit from predictable routines. It coordinates physical book identification, optional manual reading-session check-ins, room-level accessibility cues, and a household hub. It does not listen to speech, record children, measure reading ability, or diagnose learning differences. Progress is an opt-in household log, never a child score.

## System and operating modes

- Family Hub: Raspberry Pi Zero 2 W (Linux edge gateway), local SQLite store, MQTT 5 broker, Wi-Fi, and USB-C 5 V supply. It coordinates the nodes and serves a local REST API; internet loss does not stop local use.
- Book Tag: nRF52840-based BLE tag attached to a book, with a physical button and RGB status LED. Tags advertise a rotating pseudonymous identifier; no location history is retained.
- Reading Dock: ESP32-C3, NFC PN532 reader, large tactile session button, piezo, and status LED. A tap selects a registered book; pressing the button explicitly starts/ends a session.
- Room Beacon: nRF52840 + e-paper prompt display and physical acknowledge button for a scheduled gentle reminder; reminders are opt-in and silent by default.

The first prototype uses BLE advertisements to the hub (star topology, not mesh); the dock communicates with the hub over Wi-Fi/MQTT. A node can buffer a small event queue and resend after reconnect. No remote control of the child or automatic inference occurs. Adults manage household profiles and retention locally.

## Architecture and data contract

`Book Tag / Room Beacon -- BLE --> Family Hub -- local MQTT --> Reading Dock`

All event payloads use protocol v1 JSON: `{\"v\":1,\"node\":\"dock-01\",\"seq\":7,\"ts\":1730000000,\"kind\":\"session_start\",\"book\":\"book-uuid\"}`. Node IDs are provisioned locally. The hub rejects unknown versions, oversized payloads (>512 bytes), invalid event kinds, and duplicate `(node,seq)` pairs. Event timestamps are UTC Unix seconds; hub receipt time is authoritative when node time is absent. MQTT topics are `readtogether/v1/<household-id>/events` (QoS 1, non-retained) and `.../status` (retained status only). Production deployment must add per-device credentials, broker ACLs, TLS, and signed OTA; this prototype does not implement those protections and must remain on a trusted LAN.

REST API: `GET /health`, `GET /events?limit=100`, `POST /events` with the above schema. Mock mode is clearly indicated in the app. Session state is derived from explicit start/end events, not presence or ambient sensing.

## Hardware notes

All prototype boards are low-voltage USB-powered or coin-cell reference designs. Tag and beacon pin assignments, component lists, current estimates, and schematic status are in `docs/hardware.md`; BOMs are under `hardware/bom`. Schematic files are design notes, not KiCad schematics or fabrication-ready layouts. RF range, battery life, EMC, and enclosure effects are unmeasured. Avoid attaching coin-cell tags to items accessible to infants; use secured enclosures and adult supervision.

## Firmware and software

Shared C protocol validation is in `firmware/common/protocol.h` and unit tests in `firmware/tests`. Node examples are portable C event producers; radio/NFC/display drivers are explicitly platform integration points, not complete board firmware. Compile host tests with `gcc -std=c11 -Wall -Wextra -Werror firmware/tests/protocol_test.c -o /tmp/readtogether-test && /tmp/readtogether-test`.

Backend: `software/dashboard/main.py`; install dependencies from its requirements file and run `uvicorn main:app --host 127.0.0.1`. ML pipeline: `software/ml-pipeline/` demonstrates routine-event aggregation with synthetic fixtures only; it does not train a validated predictive model. Mobile app: `software/mobile-app/App.tsx` is a UI stub; no production credentials or child data are embedded.

## Build and validation status

This is a concept/prototype reference implementation. Host protocol tests and backend tests are executable. No physical boards, radio integration, certified accessibility testing, or user study is supplied. Do not make educational efficacy claims from synthetic data. See `docs/architecture.md`, `docs/protocol.md`, `docs/api.md`, and `docs/hardware.md`.

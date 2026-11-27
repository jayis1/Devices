# ParkKeys — public piano readiness and care

ParkKeys is a prototype multi-node system for volunteer-run outdoor and community pianos. It helps stewards distinguish a recently checked instrument from one needing attention, and gives visitors a simple way to report a problem. It does not assess tuning, certify an instrument as safe, or control the instrument.

## System

Three low-voltage nodes coordinate over local Wi-Fi/MQTT: a Raspberry Pi Zero 2 W site hub (Mosquitto, FastAPI, SQLite), an ESP32-C6 instrument station (large check-in button, RGB status LED, door/reed input for the weather cover), and an ESP32-C6 weather/context node (SHT31 temperature/humidity and rain switch). A React Native client reads hub summaries; an optional edge job derives maintenance intervals from steward-confirmed visits. Public users submit only one of three fixed reports: OK, needs-cleaning, or needs-repair. No audio recording, camera, identity, location tracking, or automated repair decision is involved.

## Data flow and protocol

Nodes publish JSON to `parkkeys/v1/<site>/<node>/events` with QoS 1. Payload fields: `v` (integer, 1), `event_id` (UUID), `node_id`, `seq` (monotonic per provisioned node), `observed_at` (UTC ISO-8601), `kind`, and `value`. Hub receipt time is authoritative when node time is missing or implausible. The backend validates node allowlists, schema, enum values and payload size; repeated `(node_id,event_id)` events are idempotently ignored. Retain no raw visitor data. MQTT uses TLS and per-node credentials in deployment; credentials are never included here. Local observations buffer on nodes only when transport implementation is added; current firmware is a GPIO/sensor bring-up reference, not a network client.

## Electrical reference

Instrument station and weather node each use an ESP32-C6-DevKitC-1 on regulated 5 V USB. Station GPIO9 is active-low button to ground (internal pull-up); GPIO8 drives a low-current LED through 1 kΩ. Weather node uses I²C SDA GPIO6/SCL GPIO7 at 3.3 V for SHT31 (address 0x44); rain switch is GPIO9 to ground with pull-up. Verify exact development-board revision, GPIO availability, and sensor breakout voltage before assembly. USB only; no battery charging, mains wiring, actuator, or exposed public electrical connection. BOMs and reference schematics are design drafts, not manufacturing releases.

## Software and limits

`firmware/` has node-specific C bring-up examples and a shared event contract. `software/dashboard/` provides a FastAPI validation/read API with in-memory reference storage; MQTT ingestion, durable retention, authentication and production deployment are not implemented. `software/ml-pipeline/` contains a deterministic synthetic maintenance-interval baseline, not a trained condition classifier. `software/mobile-app/` documents the intended read-only screens; it is a stub. `schematic/` contains text-based pin/net reference drawings, not KiCad projects. No hardware bench testing, measured radio range, model accuracy, or field evaluation is claimed. Human inspection remains mandatory.

## Checks

From this directory: `python -m pytest software/dashboard/tests`; `python -m compileall -q software/dashboard software/ml-pipeline`. ESP-IDF compilation and KiCad/ERC are not claimed. Before any pilot, test restart, duplicate delivery, malformed payloads, clock drift, loss of Wi-Fi, sensor disconnect, and credential revocation. Use an isolated test broker; never expose the prototype API or unauthenticated broker to the internet.

Invented and maintained by jayis1.

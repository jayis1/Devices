# ParkPlay — community playground readiness

ParkPlay helps parks crews prioritize human inspections of public playgrounds after weather, heavy use, or a reported issue. It is an advisory maintenance system, not a playground safety certification system: staff must inspect equipment and follow local standards before opening or closing an area.

## Why this exists

A parks team often learns about a broken swing, a waterlogged landing surface, or a dangerously hot slide through a delayed phone call. ParkPlay gives staff a low-cost, privacy-preserving view of equipment use anomalies and local surface conditions so they can direct inspections to the right place. Distributed nodes are needed because a swing's motion and a slide/ground's temperature and wetness occur at different physical assets; a single weather station cannot distinguish them. A staff check-in beacon records that a person inspected a reported issue, not that the system certified it safe. The pitch: “Know which playground deserves a human look first.” Target pilot is one site under $300 estimated hardware BOM; costs are planning estimates, not quotes.

This is distinct from ParkPulse (fountain/bin maintenance) and LibraryLoop (seat utilization): it concerns playground equipment and weather-related inspection prioritization, not general park assets or occupancy.

## Nodes and operation

- Site Hub: Raspberry Pi Pico W, Wi-Fi uplink, local queue and status LED; receives BLE advertisements from nodes and exposes local status. A small host gateway is required for BLE scanning and MQTT forwarding; this prototype uses an ESP32-S3 gateway node.
- Equipment Tag: ESP32-C3-MINI-1, LIS2DH12 accelerometer and sealed reed switch; reports coarse swing-use counts and unusual movement/contact changes. It never identifies people.
- Surface Sentinel: ESP32-C3-MINI-1, TMP117 temperature sensor and capacitive wetness electrode; reports local surface readings, not injury risk.
- Inspection Beacon: ESP32-C3, physical acknowledge button and LED; staff can mark an inspection as requested/in progress/complete with a non-certifying note.

Radio topology: nodes advertise compact, versioned telemetry over BLE 5.0 to the gateway; gateway forwards over MQTT/TLS when network is available. The local event queue remains useful offline. BLE link encryption/pairing and unique keys are deployment work; do not deploy sensitive sites until that is implemented. No audio, camera, face, or location tracking is collected.

## Data path

Sensor sample → bounded BLE telemetry frame → gateway validates version/range/freshness → FastAPI service via MQTT → mobile-friendly REST status → React Native pilot screen. Current API storage is in-memory; SQLite persistence and gateway buffering are design requirements, not implemented. The hub flags stale or abnormal readings with deterministic thresholds; ML is optional ranking only and cannot declare equipment safe.

## Hardware and BOM

Each BOM CSV under `hardware/bom/` lists the prototype parts and estimated cost. All listed boards are development modules; schematics are connection-level design notes, not fabrication-ready KiCad captures. Confirm exact module revision, regulator headroom, antenna clearance, enclosure ingress protection, and local radio rules before a field pilot. Use fused, protected USB 5 V supplies indoors; no mains switching or unattended actuators. Nodes are mounted out of reach in weather-rated enclosures with strain relief. Battery nodes require separate measured current profiling; runtime is not claimed here.

Pin/net assignments and assembly caveats are in `schematic/`. Confirm every pin against the exact board revision before wiring.

## Software

- `firmware/common/protocol.h` and `.c`: bounded telemetry parser/encoder and CRC-16/CCITT-FALSE for host-side demonstration.
- `software/dashboard`: minimal FastAPI ingestion/status endpoints with MQTT integration seam.
- `software/ml-pipeline`: synthetic fixture and baseline scoring demo; synthetic output is not evidence of field accuracy.
- `software/mobile-app`: React Native pilot UI stub; not a production app.
- `scripts/`: local setup and calibration checklist.

## Safety, privacy, and limits

ParkPlay does not detect injuries, inspect hidden structural damage, or replace a qualified inspection. Sensor faults, depleted power, missing packets, and stale data are shown as unknown, never as safe. All observations are advisory and humans decide access, repair, and reopening. Do not connect this prototype to gates or access-control systems. Retain only coarse counters and environmental values; define a site retention policy before deployment. MQTT must use TLS and per-device credentials in deployment; this repository contains no production credentials. Firmware signing/secure provisioning and physical environmental testing remain required before outdoor use.

## Build and verify

Host protocol test: `cc -std=c11 -Wall -Wextra -Werror firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/parkplay-protocol-test && /tmp/parkplay-protocol-test`

Backend test: `python3 -m unittest discover -s software/dashboard -p 'test_*.py'`

ML demo: `python3 software/ml-pipeline/train_baseline.py`

Hardware has not been physically tested. No target SDK build, KiCad ERC, weather ingress test, BLE interoperability test, or mobile build is claimed. See `docs/architecture.md` and `docs/protocol.md` before adapting the prototype.

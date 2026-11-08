# ClaimLoop — community lost-property coordination

ClaimLoop helps libraries, recreation centers, and community venues turn unstructured lost-property piles into an orderly, staff-mediated process. A staff member assigns a short random claim code, records a broad category and shelf location, and prints or writes the code on a paper receipt. A public display shows only aggregate counts and service hours. It does not photograph objects, identify visitors, publish item descriptions, or decide ownership. Staff verify claims in person.

Status: prototype reference design. Software host tests are provided; hardware has not been assembled, RF range has not been measured, and schematics below are connection plans rather than fabrication-ready KiCad. No user-facing deployment or security audit is claimed.

## System / data flow

Three nodes cooperate: a USB-powered Raspberry Pi Pico W gateway (RP2040, Wi-Fi) bridges local sensor events to the LAN API; an ESP32-C3 intake tray senses a staff-placed object using a load cell/HX711 and a physical confirmation button; an ESP32-C3 shelf node senses shelf-door open/closed and reports a bounded stock count entered by staff. The intended radio link is BLE advertisements, but transport and authenticated forwarding are not implemented. The gateway hosts a local HTTP API; events can be submitted by the test harness. There is no bounded offline queue yet. If gateway/network/API is down, staff continue with paper log; the physical property remains unaffected. The tray never identifies an object or person.

The future service workflow (not automated by firmware): staff create a random claim code, record category/shelf, and issue a paper receipt. Public views expose only category totals and opening hours. Claim records are retained locally for a venue-configured period, then deleted. No cloud service is required; an optional deployment can reverse-proxy the local API, but must add TLS and user authentication before exposure.

## Node specification

| Node | Compute / role | I/O and power | Safe behavior |
|---|---|---|---|
| Gateway | Raspberry Pi Pico W / RP2040; Wi-Fi bridge | 5 V USB, BLE receiver via compatible external BLE USB adapter is an unverified future integration; current reference transport is HTTP serial test harness only | Network failure leaves staff paper workflow usable; no remote actuation |
| Intake tray | ESP32-C3-MINI-1; HX711 load cell interface; confirmation button | 3.3 V logic; 5 V load-cell excitation/regulator module, common ground; Wi-Fi HTTPS in deployed firmware adapter | Reject unstable/negative/out-of-range readings; button only records staff-confirmed event |
| Shelf node | ESP32-C3-MINI-1; reed switch | 3.3 V GPIO pull-up, USB 5 V with 3.3 V regulator | Door switch is advisory; no lock or access control |

Pin references are prototype assignments and must be checked against the exact module datasheet and board before assembly. Tray: HX711 DOUT GPIO4, SCK GPIO5; button GPIO9 (avoid boot-strap interaction; verify selected board), status LED GPIO8 through 1 kΩ. Shelf: reed GPIO4 to ground with pull-up; LED GPIO8 through 1 kΩ. Use USB supplies only; no mains wiring, battery charging, or actuator loads.

## Repository map

- `firmware/common/protocol.h`: versioned fixed-width event contract and bounds.
- `firmware/intake-tray/main.c`, `firmware/shelf-node/main.c`: portable C event-state examples; board HAL and transport adapters explicitly remain integration work.
- `software/dashboard/`: FastAPI + SQLite local event intake, duplicate rejection, bounds validation.
- `software/ml-pipeline/`: reproducible synthetic-data pipeline smoke test only; not trained evidence or a classifier for claims.
- `software/mobile-app/`: React Native UI contract stub; no production authentication or claim matching.
- `hardware/bom/`: per-node estimated prototype parts.
- `schematic/`: explicit net/pin plans, not KiCad files.
- `docs/`: architecture, API, protocol, threat model, assembly cautions.

## Build and verify

Backend: `cd software/dashboard && python -m pip install -r requirements.txt && pytest -q`.
ML smoke test: `python software/ml-pipeline/train.py --output /tmp/claimloop-model.json` (synthetic data only).
Firmware source is host-portable C logic and can be checked with `cc -std=c11 -Wall -Wextra -Werror -fsyntax-only` per source; actual ESP-IDF/Pico SDK builds require board adapters and toolchains not included here.

## Limitations and safety

This prototype does not establish ownership, guarantee retrieval, or replace venue policies. Never store sensitive item descriptions, names, contact details, or identity documents in public telemetry. Do not put unclaimed property in an unattended automated locker. Staff must follow local lost-property retention and disposal rules. Design is not electrically reviewed, EMC tested, or production-ready.

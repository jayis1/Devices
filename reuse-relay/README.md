# ReuseRelay

Local-first building system for routing reusable move-out items to neighbors and community reuse partners before they become bulky waste.

## Product and boundaries

Job to be done: “When someone leaves my building, help me offer useful items and arrange a safe, timely handoff without posting my identity or leaving belongings in an escape route.” Target users are apartment residents, building staff, and participating reuse organizations. Unlike WasteSort (classification of household discards) and ToolShare (tracked shared inventory), ReuseRelay coordinates temporary, owner-authorized move-out offers and physical handoffs. Multiple room/staging nodes detect item arrival and pickup; a site hub mediates a local offers board. It does not identify people, appraise items, certify safety, or guarantee pickup.

Day-one value needs no ML: residents/staff enter a coarse item category and pickup window; local rules flag prohibited categories and expired offers. Install is renter-safe: battery sensor pucks attach with removable adhesive; hub plugs into USB-C. Staff moderation and manual confirmation remain required. Offers expire automatically; no names, apartment numbers, images, or tracking leave the building. Site data stays on the hub by default; cloud sync is opt-in and exports only pseudonymous offer IDs and coarse category/status. Delete offers on expiry and site data on decommissioning.

## System and flow

Three nodes: (1) Site Hub: Raspberry Pi Zero 2 W, local SQLite/API, Wi-Fi uplink, BLE provisioning; (2) Staging Node: ESP32-C6, load cell platform and door/reed sensor at a designated reuse shelf; (3) Pickup Node: ESP32-C6, load-cell shelf and staff-confirm button at collection point. Nodes send signed-by-link-layer, versioned CBOR-like fixed binary telemetry over encrypted Wi-Fi ESP-NOW is NOT used; the nodes use BLE 5 GATT to the hub (local range, no cloud dependency). Hub serves a local HTTPS API and optional MQTT TLS uplink. Telemetry path: node→hub→local queue→optional backend→mobile web view. Offer creation is manual; item presence only prompts a staff review. Pickup confirmation is a local button/app action. No automatic release or physical access control.

Hub is always the authority for offer state. Duplicate event IDs are idempotent; stale events are rejected; outage queues are bounded to 7 days and surfaced to staff. Nodes cache no personal data. On radio loss, sensors buffer a small ring and show offline status; the local board remains available. Cloud loss never blocks local operation. No actuators are present.

## Hardware reference design (not fabrication-ready)

All circuitry is 3.3 V logic. Use certified USB supplies; no mains wiring. The load-cell excitation/ADC is an HX711 breakout with 10 kΩ pull-ups not required on its differential input; place 100 nF at its supply. Mechanical platform needs overload stops and staff-only handling. BLE antenna keep-out per module datasheet. Battery-powered staging/pickup units use protected 18650 cell and a charger module with protection; prototype charging must be supervised. Battery life is not claimed until measured.

| Node | SoC / parts | Pin map (ESP32-C6 module) |
|---|---|---|
| Hub | Pi Zero 2 W; USB-C certified 5 V/2.5 A supply; local storage | BLE adapter via USB; Wi-Fi; no custom PCB |
| Staging | ESP32-C6-WROOM-1; HX711 + 5 kg load cell; reed switch; status LED/button | GPIO4/5 HX711 DOUT/SCK; GPIO6 reed; GPIO7 button; GPIO8 LED; ADC1 GPIO0 battery monitor (divider) |
| Pickup | ESP32-C6-WROOM-1; HX711 + 20 kg load cell; confirm button; LED | GPIO4/5 HX711 DOUT/SCK; GPIO6 button; GPIO7 LED; ADC1 GPIO0 battery monitor (divider) |

Pin assignments are a proposed mapping; confirm the exact module variant, strapping constraints, and SDK configuration against its datasheet before PCB design. Use BLE pairing with per-node unique credentials; this reference firmware does not yet implement cryptographic identity or secure boot. Do not use for unattended or safety-critical operation.

## Software status

Firmware directories contain host-testable protocol and node state-machine examples, not target-ready firmware. Backend is a local FastAPI demonstrator with in-memory storage; production needs persistent database, user authentication, TLS termination, consent flow, audit controls, backups, and deployment review. ML is optional and intentionally limited to a synthetic-data baseline for coarse category suggestions; rule-based moderation is the only operational path. Mobile app is an API-connected React Native scaffold, not a released app.

## Build and checks

See `docs/architecture.md`, `docs/protocol.md`, and `docs/api.md`. `schematic/*/README.md` is a pin/netlist specification; no KiCad capture or ERC is supplied. BOMs identify prototype-level parts and indicative estimates, not quotes. Host checks: `python3 -m unittest discover -s software/dashboard -p 'test_*.py'`; `python3 -m py_compile software/dashboard/main.py software/ml-pipeline/train.py`; `gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only firmware/common/protocol.c`. Hardware, target builds, RF coexistence, battery life, security, usability, and field performance remain unverified. This is a concept/reference prototype, not a validated product.

## Safety, privacy, readiness

Staff must reject unsafe, recalled, contaminated, upholstered, electrical, or otherwise restricted goods according to local policy; system suggestions are not safety approvals. Never obstruct egress. Require a staffed, accessible staging area and a manual fallback. No medical, safety, or waste-diversion efficacy claims. FCC/CE/RED assessment and battery transport review are outstanding. Data retention defaults: offers expire in 48 hours; aggregate counts retained locally 30 days; no recipient identities.

*Invented 2026-10-28 by jayis1.*

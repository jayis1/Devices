# SeedShare — community seed-library stewardship

SeedShare helps volunteer-run seed libraries keep donated packets identifiable, dry, and findable across a public lending shelf. A small local gateway joins a climate logger and a check-in scale/scanner dock; volunteers can see storage excursions and reconcile packet movement without tracking borrowers. It addresses a recurring operational gap in community food-growing infrastructure, rather than plant care or crop automation.

**Why multiple nodes:** storage conditions are measured where packets live, while intake/return events happen at a separate counter; a gateway must reconcile both over time and work through internet outages. A single phone or sensor cannot establish this audit trail. Primary user: volunteer coordinators at community seed libraries. No claim of preserving seed viability: germination tests and human inspection remain authoritative.

## System and operating boundary

- **Gateway**: ESP32-S3 dev board; local Wi-Fi, BLE provisioning, MQTT bridge and offline queue. Local display/buzzer and physical acknowledge button; no cloud required for event capture.
- **Storage node**: ESP32-C3 + SHT31 temperature/RH sensor in the cabinet. USB 5 V. Reports samples over authenticated BLE GATT to gateway; local LED marks high-RH conditions.
- **Intake dock**: ESP32-C3 + NAU7802 load-cell ADC, 5 kg load cell, QR scanner module (UART); USB 5 V; physical tare/confirm buttons. Human confirms identity and quantity; weight is a consistency cue, never a count claim.
- **Mobile app**: volunteer-facing API client stub; not required for local operation.

BLE LE encrypted link, per-node bonded credentials, event sequence numbers and CRC-16 protect transport integrity; pairing requires a physical button on the gateway. Do not reuse development keys. Gateway validates bounded payloads and publishes `seedshare/v1/<site>/<node>/telemetry` over MQTT TLS when configured. It buffers events locally while offline; expired advisory alerts are not replayed. No personal/borrower data is collected.

## Power, interfaces and build status

All nodes use certified external 5 V USB supplies; onboard 3.3 V regulation is on the dev boards. No mains switching, chemical treatment, or automated seed dispensing. Approximate BOM planning cost: gateway $18, storage node $14, dock $28 (excluding enclosure and tax); pricing is indicative. See `hardware/bom/`. Pin assignments and electrical assumptions are in `docs/architecture.md`; conceptual connection drawings are in `schematic/` and are not KiCad manufacturing schematics. Physical validation, RF compliance, enclosure design and seed-specific storage validation remain outstanding.

Firmware is portable C protocol/validation code with host tests; board drivers are integration stubs, not a certified ESP-IDF product. Backend is a runnable FastAPI local event API plus MQTT bridge adapter. ML is an exploratory, synthetic-data baseline only; the shipped decision path uses explicit humidity thresholds. The React Native screen is a UI scaffold, not a complete app.

## Quick checks

`python3 -m unittest discover -s software/dashboard/tests -v`

`gcc -std=c11 -Wall -Wextra -Werror -Ifirmware/common -fsyntax-only firmware/common/protocol.c firmware/gateway/main.c firmware/storage-node/main.c firmware/intake-dock/main.c`

See `docs/` for protocol, API, assembly, privacy, failure behavior and validation limits. Do not use SeedShare as a seed-quality, food-safety, or agricultural diagnostic system.

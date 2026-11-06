# ShareTable — Community meal-service coordination

Invented and maintained by jayis1. ShareTable helps volunteer-run community meal sites coordinate prepared-food batch availability, serving-line temperature observations, and washable-container returns without identifying diners or making food-safety decisions.

## Scope and safety

This is an advisory logging system, not a HACCP instrument, certified thermometer, or guarantee of safe food. Staff remain responsible for required calibrated measurements, time/temperature controls, allergen communication, and disposal decisions. Sensors are non-contact/context aids unless independently validated. No cameras, microphones, recipient accounts, or individual meal records are collected. Keep network failure from interrupting service: nodes buffer observations; staff use paper procedures.

## System and data path

Three low-voltage nodes form a BLE 5.0 star: (1) ESP32-C3 gateway with Wi-Fi, local SQLite queue, and MQTT/TLS uplink; (2) ESP32-C3 serving station with DS18B20 probe input for staff-entered probe readings and a physical event button; (3) RP2040 container-return scale node with HX711/load cell for aggregate crate counts. Gateway publishes versioned JSON observations to `sharetable/v1/{site_id}/{node_id}/telemetry`; API validates site-scoped node identity and writes append-only events. React Native app shows latest batch/event counts, stale-data status, and manual checklist links. It does not publish control commands.

Example: `{"v":1,"node":"serve-01","seq":17,"ts":"2026-01-01T12:00:00Z","kind":"probe_reading","value":72.4,"unit":"C","quality":"staff_entered"}`. Values are observations only. Bounded payloads, monotonic sequence numbers, duplicate suppression, reconnect backoff, and a 24-hour local queue are specified in `docs/protocol.md`.

## Hardware and wiring (conceptual; review before fabrication)

Gateway: ESP32-C3-MINI-1, USB-C 5 V input, 3.3 V regulator, status LED GPIO8, BLE/Wi-Fi internal radio, UART0 programming pads. Serving node: ESP32-C3-MINI-1; DS18B20 data on GPIO4 with 4.7 kΩ pull-up to 3.3 V (three-wire powered probe only), event button GPIO5 to ground with internal pull-up, LED GPIO8. Probe is staff-handled and requires separate calibration against the site’s approved reference thermometer. Return node: RP2040, HX711 DOUT GPIO16 / SCK GPIO17, load cell to HX711 bridge terminals, USB 5 V input and 3.3 V logic; tare/calibration is site-specific and count estimates are not inventory truth. Do not connect mains or actuate kitchen equipment.

Power: USB 5 V, regulated 3.3 V logic; budget each node for 500 mA peak radio transient and use a 1 A supply. No battery design is claimed. Verify module pinout, regulator thermal margin, ESD, enclosure wash-down separation, load-cell mechanics, and probe sanitation before deployment.

## Files

- `schematic/`: KiCad-compatible project notes/netlists, conceptual only; not fabrication-ready.
- `firmware/`: C node examples and common protocol header.
- `hardware/bom/`: per-node CSV BOMs with orderable parts and substitution notes.
- `software/dashboard/`: FastAPI/MQTT ingestion example.
- `software/ml-pipeline/`: reproducible baseline event-volume forecaster; synthetic smoke data only, no accuracy claim.
- `software/mobile-app/`: minimal React Native screen stub.
- `docs/`: architecture, API, protocol, privacy and operational limitations.
- `scripts/`: local smoke checks and deployment configuration template.

## Build status

Conceptual reference design. Host-side checks only; no target firmware SDK build, KiCad ERC, calibrated instrument validation, RF certification, or physical deployment has been performed. See `docs/architecture.md` and `docs/limitations.md` before adapting.

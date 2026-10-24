# RepairLoop — community repair-space readiness

Status: prototype/reference design. Firmware and schematics are not fabrication- or safety-verified; textual netlists only. Invented by jayis1.

## Problem and value
Volunteer-run repair cafes and tool libraries lose time discovering whether a bench is available, whether shared tools are present, and whether ventilation is operating. RepairLoop gives staff a local, glanceable readiness view without identifying visitors or recording repair activity. Day-one value works with the hub and any single sensor; internet and ML are optional. Success measure for a pilot: reduce volunteer opening-check time and avoidable bench interruptions; measure baseline locally before setting a target.

Distinct from existing systems: unlike ToolShare (shared tool inventory/returns), LibraryLoop (seat utilization), and WorkshopSync (private workshop safety), RepairLoop coordinates public repair-bench readiness: anonymous bench occupancy, shared equipment presence, and air-extraction status. It does not track borrowers, diagnose hazards, or switch machinery.

## Architecture
```
Bench sensor (ESP32-C6, BLE) ─┐
Tool dock tag (nRF52840, BLE) ─┼─ RepairLoop hub (RP2040 + ESP32-C6 Wi-Fi) → local API/MQTT → optional app
Air sentinel (ESP32-C6, BLE) ──┘
```
Hub aggregates BLE reports; safe local display/status continues without cloud. Nodes report measurements only. No machine control. BLE pairing and authenticated application payloads are required for deployment; prototype code currently demonstrates CRC framing only, not cryptographic security.

| Node | SoC/radio | Supply | Assigned interfaces |
|---|---|---|---|
| Hub | RP2040 + ESP32-C6 module, Wi-Fi/BLE | USB-C 5 V SELV | UART0 GP0/1, I2C0 GP4/5, status LED GP16 |
| Bench sensor | ESP32-C6, BLE | USB 5 V | I2C SDA GPIO6/SCL GPIO7, occupancy GPIO4, LED GPIO8 |
| Tool dock | nRF52840 module, BLE | USB 5 V | I2C SDA P0.26/SCL P0.27, load-cell ADC via external HX711 DOUT P0.11/SCK P0.12 |
| Air sentinel | ESP32-C6, BLE | USB 5 V | I2C SDA GPIO6/SCL GPIO7, fan tach GPIO4 (read-only) |

Pin maps are conceptual and must be checked against selected board variants before assembly. All nodes use low-voltage USB supplies; no mains wiring or actuators are included.

## Protocol and data
Version 1 frames contain version, node type, node ID, sequence, payload length, payload and CRC-16/CCITT-FALSE (little-endian fields). Maximum payload 48 bytes. Duplicate or stale sequence values are discarded; data older than 120 s is marked stale. Detailed contract: [docs/protocol.md](docs/protocol.md). CRC detects accidental corruption, not malicious modification. Production requires per-device authenticated encryption, secure enrollment and key rotation.

## Build and operation
Host protocol test: `cc -std=c11 -Wall -Wextra -Werror firmware/common/protocol.c firmware/tests/test_protocol.c -o /tmp/repairloop-test && /tmp/repairloop-test`. Node firmware sources are portable illustrative logic; ESP-IDF/nRF Connect SDK board builds are not yet supplied or verified. Backend: `python -m pip install -r software/dashboard/requirements.txt` then `uvicorn app:app --app-dir software/dashboard`. App is an unbuilt React Native UI stub. ML demo uses only deterministic synthetic data and is not an operational prediction model.

## BOM estimate and power
Exact variants and prices are procurement-dependent; TBD means not selected, not a fabricated MPN. USB-powered nodes avoid battery-life claims. Current draw depends on radio duty cycle and selected sensor modules; measure before field deployment. Every referenced sensor and module is itemized in per-node BOM CSVs.

## Privacy, safety, accessibility
No audio, images, names, or visitor identifiers are collected. Data is event-level device telemetry, retained locally by default; cloud sync is optional and requires an operator-controlled authenticated broker. Users can disable individual nodes and erase local records. Large-text display and non-color-only status labels are recommended. Ventilation values are informational only: users must follow site procedures and never treat this prototype as a safety monitor. No medical, compliance, or certification claims.

## Validation limits
Host protocol test and Python smoke tests are provided. No physical prototype, RF/regulatory assessment, KiCad ERC, mobile build, security audit, or actual sensor calibration has been performed. See [architecture](docs/architecture.md), [API](docs/api.md), and [run report](docs/validation.md).

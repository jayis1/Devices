# PowerBankLoop

**A community-managed, anonymous power-bank lending cabinet for libraries, shelters, campuses, and neighborhood hubs.** PowerBankLoop makes short-term phone charging available without an app account, payment processor, or borrower tracking. It inventories charge-ready packs and reports cabinet health; it does not unlock doors or control high-voltage mains.

Status: reference prototype, not a certified product. Firmware and backend slices are implemented for review; hardware descriptions are conceptual pin/net plans, not production KiCad schematics. Bench validation, enclosure/fire review, USB-C compliance, radio certification, and site approval remain required.

## Need and boundaries

A dead phone can cut off maps, transit tickets, translation, and contact. Existing public charging points require a user to remain beside an outlet; lending a charged USB battery makes charging portable. The system helps operators keep packs available and safely retire suspect units. No camera, microphone, account, phone identifier, or borrower identity is collected. Pack health telemetry is operational context, not a safety certification.

## System

Three hardware roles form one site system:

- Site gateway: ESP32-S3-WROOM-1, Wi-Fi uplink, SX1262 868 MHz radio, status display. Coordinates two or more cabinet zones, keeps an offline event queue, and exposes local status.
- Loan dock: RP2040, SX1262, per-slot presence switches and low-voltage USB-C power-path/load monitoring. Volunteers issue/return packs manually; the dock only counts slot presence and observes charging state. It cannot unlock or dispense packs.
- Pack tag: nRF52840 module, BLE connection to a dock for pack identifier and coarse battery/temperature telemetry. No GPS or user-device pairing. Pack label ID is random and replaceable; tags contain no personal data.

Gateway publishes aggregate site status to a FastAPI service over MQTT/TLS. A React Native operator app displays availability and maintenance advisories. ML pipeline is an offline baseline for aggregate demand and does not control charging or classify pack safety.

```text
Pack tags --BLE--> Loan docks --LoRa 868 MHz--> Site gateway --MQTTS--> FastAPI
                         |                             |                  |
                    slot sensing                 offline queue       operator app
```

## Electrical and operational safety

Prototype uses certified, enclosed USB-C power-bank modules and listed SELV 5 V supplies; no bare lithium cells are charged by this design. Charging module selection, per-port current limit, thermal cutoff, cable protection, enclosure ventilation, and local fire-code review are mandatory before deployment. A dock firmware fault defaults its output-enable signal low. Docks do not charge when their local temperature sensor is absent or outside configured bounds. Gateway/cloud loss does not affect the physical manual checkout process. Staff inspect packs and may quarantine them; software status is advisory. Never leave damaged, swollen, hot, wet, or recalled packs in service.

## Interfaces and pin plan

The target-specific pin plan is normative for this prototype, subject to board/module datasheet confirmation before PCB layout.

Gateway ESP32-S3-WROOM-1: SPI2 SCK=GPIO12, MOSI=11, MISO=13, SX1262 NSS=10, BUSY=14, DIO1=9, RESET=8; I2C SDA=GPIO5/SCL=GPIO6 for display and temperature; UART0 USB debug; Wi-Fi 2.4 GHz uplink. Use a module with onboard antenna or approved external RF layout, not both. 3.3 V domain; 5 V USB input via rated buck/LDO.

Dock RP2040: SPI0 SCK=GPIO18, TX=19, RX=16, radio NSS=17, DIO1=20, BUSY=21, RESET=22; I2C0 SDA=4/SCL=5; slot presence inputs GPIO6-9 with external pull-ups; current monitor alert GPIO10; temperature sensor GPIO11; output-enable GPIO12 (external pulldown); USB debug. 5 V input; logic 3.3 V. Each USB-C port has a separately current-limited, protected 5 V power path. Connector CC resistors and source role configuration must follow USB Type-C requirements.

Pack nRF52840 module: I2C SDA=GPIO26/SCL=27 to fuel-gauge and temperature monitor; status LED GPIO13; NFC disabled; BLE 5.0 only. Tag is powered by the pack's protected low-voltage rail; no charge-control connection. BLE privacy address rotation and dock allow-listing are required.

## Shared protocol

Radio is regional 868 MHz EU prototype configuration; use only a compliant module/antenna and local duty-cycle limits. LoRa point-to-point star (not mesh), hub-coordinated uplink, application-layer AES-CCM with per-site provisioned keys, 32-bit monotonic counter, CRC, bounded frame size, ACK/retry with jitter. The included protocol codec is a host-testable reference framing layer; crypto and radio-driver integration are explicitly not implemented. Do not deploy it as authenticated transport until reviewed crypto is integrated.

Every event includes `v`, `site`, `node`, `seq`, `ts`, `kind`, `payload`; timestamp is UTC epoch seconds or null when unsynchronized. Duplicate `(node,seq)` is idempotently ignored. Stale telemetry is marked stale, never interpreted as zero. Retained MQTT messages are status only, never commands. There are no remote actuator commands. See [protocol](docs/protocol.md) and [API](docs/api.md).

## BOMs

CSV files are per-node, indicative single-unit prototype selection lists; prices are estimates, not quotes. Battery packs are certified commercial modules supplied by site operators and are not included as bare-cell designs. Verify exact module revisions, stock, RF/USB compliance, thermal behavior, and footprints before procurement.

- [Gateway BOM](hardware/bom/gateway.csv)
- [Loan dock BOM](hardware/bom/loan-dock.csv)
- [Pack tag BOM](hardware/bom/pack-tag.csv)
- [Schematic status and conceptual net lists](schematic/README.md)

## Software and implementation

- `firmware/common/protocol.*`: bounded binary framing codec; host test in `firmware/common/test_protocol.c`.
- `firmware/gateway/main.c`, `loan-dock/main.c`, `pack-tag/main.c`: portable C role logic illustrating state, not board SDK projects. Hardware drivers, secure key provisioning, BLE/LoRa stacks, and OTA are not included.
- `software/dashboard/app.py`: FastAPI event ingestion with schema checks, bounded in-memory latest-state view, and health endpoint. `mqtt_bridge.py` is a TLS-configured MQTT-to-HTTP prototype; production API authentication, durable storage, broker ACLs, and end-to-end authorization are not included.
- `software/ml-pipeline/train.py`: deterministic demand baseline training on supplied synthetic sample; demonstrates pipeline only, no real-world model claim.
- `software/mobile-app/App.tsx`: React Native display scaffold; no authentication or checkout workflow.

## Validation and maturity

Run `./scripts/validate.sh` from this directory. Host protocol tests, Python API tests, Python compilation, CSV parsing, and reference ML smoke test are included. No embedded SDK build is claimed; no physical prototype, electrical-rule check, battery certification review, radio test, or mobile native build has been performed. Hardware pin/net documentation is not a substitute for KiCad schematics. Do not infer safety from software tests.

## Privacy, operations, and limitations

The service stores site/node health, aggregate slot counts, and telemetry timestamps only. Do not add checkout identities, phone MACs, or per-person histories. Site operators set a retention period and delete site data when decommissioning. Provision unique node keys out of band; rotate/revoke keys on loss. Offline gateway stores a bounded ring buffer and drops oldest telemetry with an explicit gap counter when full. Staff perform all physical pack handoffs and inspections. The system cannot verify pack provenance, capacity, or electrical safety.

Detailed rationale: [architecture](docs/architecture.md). All costs, ranges, uptime, and expected demand must be measured at the installation site; none are guaranteed.

*Designed by jayis1.*

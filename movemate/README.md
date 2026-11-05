# MoveMate — Find anything during a move

MoveMate is a local-first inventory and wayfinding system for household moves, renovations, and seasonal storage. A user labels each box with a reusable QR tag, records a short contents note, and scans it when it changes rooms. At the destination, the app answers “which box has the kettle?” and guides the user to the room and shelf. It does not infer who is present, record audio/video, or require cloud access.

This is distinct from the repository's home maintenance, routine, and shared-item systems: MoveMate tracks explicitly labeled, temporary containers through a relocation, not household tasks, appliance condition, or shared inventory.

## Prototype boundary

Three hardware roles: an ESP32-C3 gateway, two or more ESP32-C3 room beacons, and a battery QR scan dock (phone camera may replace the dock). Room beacons advertise signed-by-installation BLE service data to the gateway; the gateway timestamps observations and relays them over Wi-Fi/MQTT. QR contents identify a box only; room assignment requires an explicit scan/check-in, never proximity alone. RSSI is approximate context, not proof of location. No motorized or safety-critical actuation.

MVP targets: lookup by item phrase under 2 seconds on a local network; offline scan queue survives gateway/network loss; no raw camera image is stored; user can export and delete the inventory. These are design targets, not measured results.

## Nodes and design ledger

| Node | SoC / key parts | Role | Power and interface |
|---|---|---|---|
| Gateway | Raspberry Pi Zero 2 W, USB-C 5 V supply | BLE observer, local MQTT broker/API, database, sync | 5 V/2 A; Wi-Fi + BLE; USB service |
| Room beacon | ESP32-C3-MINI-1, SHTC3 optional room climate, status LED, button | BLE advertisement with rotating installation token and room ID | 1-cell protected Li-ion, 3.3 V regulator; BLE; USB-C programming/charge module |
| Scan dock (optional) | ESP32-C3-MINI-1, 2D UART QR engine, OLED, buttons | Explicitly scan/assign box and room without phone | 5 V USB; UART QR engine, I²C OLED, BLE |

Use a protected cell and a certified external charger module; do not charge an exposed cell from the MCU. The first prototype may run each beacon from USB power. Verify the selected module's regulator, antenna keep-out, battery protection and charger thermals against supplier datasheets before PCB release. The pin map and BOM below are prototype-level selections, not validated manufacturing data.

### Room beacon pin assignment (ESP32-C3-MINI-1)

| Signal | GPIO | Notes |
|---|---:|---|
| I²C SDA/SCL | 8 / 9 | SHTC3 at 3.3 V, 4.7 kΩ pull-ups; omit sensor if unused |
| Status LED | 3 | 1 kΩ series resistor, active high |
| User button | 4 | Internal pull-up, switch to ground |
| UART0 TX/RX | 21 / 20 | Programming/logging; preserve boot/download access |
| Supply | 3V3 / GND | Decouple at module; regulator sized for radio peaks |

Keep GPIO8/9 boot strapping implications in mind for the exact module/board; confirm the chosen board's boot behavior before assembly. Do not connect unknown external pull states without checking the module reference design.

### Power estimate

Beacon design budget: deep sleep 20–100 µA target (must be measured on the selected board), BLE advertising peak approximately 100–300 mA for brief bursts, average target below 1 mA at a 2-second advertising interval. A nominal 1000 mAh protected cell at 1 mA gives an ideal 41 days; regulator loss, temperature, retries, and cell aging reduce this. Measure sleep current and radio duty cycle before claiming battery life. Gateway is mains-powered and local functions continue when internet is unavailable.

## System flow and protocol

Phone or scan dock creates a box record with a random UUID, QR payload version, human-entered contents, and optional room. QR payload format: `MM1:<base32-uuid>:<crc32>`; it contains no personal data. Beacon advertisements carry protocol version, installation ID fragment, room ID, monotonic counter, and rotating 64-bit token. The gateway maps tokens only after local pairing. Tokens are not cryptographic authentication; deployments needing hostile-radio protection must add per-device authenticated encryption before use outside a trusted home.

Room assignment is explicit: scan box QR, choose room, then confirm at the beacon button/dock. Observation telemetry includes node ID, sequence, UTC time, room ID, RSSI, and firmware version. Duplicate sequence values are discarded; stale observations never override a newer explicit assignment. MQTT topics are `movemate/v1/<installation>/rooms/<room>/presence` (QoS 1, not retained) and `movemate/v1/<installation>/boxes/events` (QoS 1, retained state only on private broker). Broker binds to localhost/LAN, anonymous access disabled in deployment; TLS and unique credentials required for remote access. Commands are not part of the MVP.

Offline-first SQLite is authoritative on gateway; cloud sync is opt-in and exports only box UUID, user-entered item labels, and room assignment. Retention ends when the user closes/deletes a move; provide full export/deletion. No advertising IDs, face/person recognition, or raw camera uploads.

## Repository contents

- `schematic/`: per-node connection sketches and net/pin ledger. These are design notes, not KiCad electrical schematics or ERC-validated PCB designs.
- `firmware/`: portable C protocol codec and node examples; ESP-IDF integration remains a board bring-up task.
- `hardware/bom/`: prototype BOMs with designators and sourcing notes.
- `software/dashboard/`: FastAPI API and local SQLite event store.
- `software/ml-pipeline/`: reproducible synthetic lookup-ranking baseline demonstration; not evidence of real-world model quality.
- `software/mobile-app/`: React Native interface contract and stub screens.
- `docs/`: architecture, protocol, privacy, and assembly notes.
- `scripts/`: local development helper.

## Bring-up and verification

Backend: `cd software/dashboard && python3 -m unittest -v` (standard library only). ML demo: `python3 software/ml-pipeline/train.py`. Firmware codec: `cc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/movemate-test && /tmp/movemate-test`.

This repository run validates host-side protocol and backend logic only. No physical hardware, target firmware build, KiCad ERC, broker integration, cloud deployment, mobile dependency install/build, RF range, battery-life, or ML accuracy has been tested. No clinical, security certification, or product-readiness claim is made.

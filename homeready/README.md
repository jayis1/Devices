# HomeReady — household emergency-kit readiness coordination

Status: reference prototype specification. Electronics are unbuilt; firmware and software are starter implementations, not a certified emergency product. Invented and maintained by [jayis1](https://github.com/jayis1).

HomeReady helps a household notice missing or expired emergency supplies before an outage or evacuation. It is not an emergency alert receiver, weather predictor, medical device, or guarantee that supplies are safe or usable. It never tracks household members. Users scan a kit and voluntarily record quantities, expiry dates, and inspection checks.

## Why multiple nodes
A sealed kit may be stored away from the hub; a pantry/utility cache can be in another room; an entry dock supports a quick manual departure checklist. The hub combines their inventory events locally and presents a household checklist. When internet or cloud service is down, local scans, reminders, and reports continue. No autonomous dispensing or access control is provided.

## Architecture and data flow

```text
[Kit Tag Dock: ESP32-C3 + NFC] -- BLE GATT --> [Home Hub: Raspberry Pi Zero 2 W]
[Cache Check-in: ESP32-C3 + buttons] -- BLE GATT --> [local SQLite event store]
[Entry Checklist Dock: ESP32-C3 + e-paper] -- BLE GATT --> [FastAPI local service]
                                                        | MQTT/TLS optional
                                                        v
                                      [cloud API + opt-in ML analysis]
                                                        ^
                                          [React Native app stub]
```

The hub is the coordinator, clock authority, local queue, deduplication point, and optional MQTT bridge. Nodes do not speak MQTT. Use BLE 5 LE GATT with one service UUID per prototype; application data is UTF-8 JSON capped at 384 bytes. Pairing/enrollment in this prototype is manual and not a production identity system. Never reuse a fleet credential. Cloud sync must use TLS and per-household authorization; no credentials are included.

## Hardware and pin assignments
Pin numbers are proposed for the named modules and must be checked against the exact revision and board schematic before fabrication. All signals are 3.3 V logic. No mains wiring, life-safety sensors, or actuators.

| Node | SoC and components | Interfaces / proposed pins | Power and role |
|---|---|---|---|
| Home hub | Raspberry Pi Zero 2 W; USB BLE adapter; microSD | USB host; Wi-Fi; BLE | Certified regulated 5 V USB-C supply; SQLite, BLE coordinator, local API |
| Kit tag dock | ESP32-C3-MINI-1; PN532 NFC module; 2 buttons; LED | PN532 I²C SDA GPIO4/SCL GPIO5, IRQ GPIO6; buttons GPIO7/8; LED GPIO10 | 5 V USB, 3.3 V regulator; explicit item scan and add/remove event |
| Cache check-in | ESP32-C3-MINI-1; 3 buttons; SSD1306 OLED | buttons GPIO4/5/6; OLED I²C SDA GPIO7/SCL GPIO8 | 5 V USB, 3.3 V regulator; manual inspection and count confirmation |
| Entry checklist | ESP32-C3-MINI-1; 2.9-inch SPI e-paper; 2 buttons | SPI SCK GPIO4, MOSI GPIO5, CS GPIO6, DC GPIO7, RST GPIO8, BUSY GPIO9; buttons GPIO10/18 | 5 V USB, 3.3 V regulator; locally cached checklist, no lock or door sensor |

Use 100 nF local decoupling at each module supply and a 10 µF bulk capacitor per node. USB supplies must be current-limited; validate peak e-paper and radio current against the selected supply. E-paper refresh is user-triggered to limit peak draw. No battery charging circuit is included. USB connector shell/ground and ESD protection must be finalized in the PCB design. The GPIO assignments are a reference, not validated KiCad netlists.

## Event protocol
Version 1 event fields: `version` (integer 1), `node_id` (enrolled identifier), `seq` (monotonic uint32), `event_id` (UUID), `kind`, `item_id` (random household-local ID), `quantity` (nonnegative integer), `expiry_date` (optional YYYY-MM-DD), `utc_ms` (optional), `quality` (`known` or `estimated`). Supported kinds: `item_scanned`, `quantity_set`, `inspection_done`, `expiry_updated`, `checklist_ack`. Reject unknown versions/kinds, invalid dates, negative quantities, duplicate event IDs, payloads above 384 bytes, and out-of-order sequence values; duplicate delivery is idempotent. Hub receipt time is stored separately when node time is absent. Events never trigger emergency services or actuators. Retention defaults to local history until user deletion; cloud sync is opt-in.

Optional MQTT topic: `homeready/<random-household-id>/events`, QoS 1, TLS, broker ACL scoped to that household. Reject retained commands; this system publishes events only. The example backend is a local prototype and does not implement account auth, broker provisioning, key rotation, or secure OTA; do not expose it to the internet.

## Power budget and BOM
Typical estimates only; measure before selecting a supply. Hub: Pi Zero 2 W 0.4–1.2 A at 5 V (USB supply rated 2 A). Kit dock: ESP32-C3 30–250 mA peaks plus PN532 up to 100 mA, supply rated 5 V/1 A. Cache dock: ESP32-C3 30–250 mA peaks plus OLED under 30 mA, 5 V/1 A. Entry dock: ESP32-C3 30–250 mA peaks and e-paper refresh peak per display datasheet; use 5 V/1 A and verify. Estimates depend on module and display variants.

Each `hardware/bom/<node>/BOM.csv` lists prototype parts, quantity, manufacturer/part number where known, and notes. Alternates require electrical and layout review. Replaceable passive NFC tags are not included in dock BOM.

## Firmware, cloud, ML, app
- `firmware/common/`: bounded C event validator; it is not cryptographic authentication or a complete BLE stack.
- `firmware/*/`: C starter source for each ESP32-C3 node; integrate with ESP-IDF 5.x BLE GATT and board drivers before flashing.
- `software/dashboard/`: FastAPI local reference API with in-memory prototype storage; persistence, user authentication, and production MQTT integration are not implemented.
- `software/ml-pipeline/`: synthetic-data baseline to demonstrate expiry-priority sorting only. No trained model, real dataset, performance claim, or safety decision. Rule-based date sorting is the fallback.
- `software/mobile-app/`: React Native screen stub, not a functional app.

## Build, test, limitations
Run `python -m unittest discover -s software/dashboard/tests` from this directory. Run `python software/ml-pipeline/train_baseline.py --help` for the synthetic demonstration. C validator can be checked with `cc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/homeready-test && /tmp/homeready-test`. Firmware requires ESP-IDF 5.x and exact hardware drivers; no firmware build, BLE interoperability test, KiCad ERC, hardware test, or mobile build is claimed. The pin maps are design proposals and must be checked against module datasheets and a real schematic before fabrication. A checklist is not a substitute for official emergency planning, first-aid training, or inspection of supplies.

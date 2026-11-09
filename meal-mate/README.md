# MealMate — household meal-prep coordination

Status: engineering concept; firmware, electrical design, ML, and app are scaffolding, not validated or build-ready. Invented and maintained by jayis1.

MealMate helps a household answer two everyday questions: what ingredients are likely available, and which meal-prep jobs have been explicitly claimed? It combines passive shelf mass observations with voluntary scan/check-in events. It does not identify people, infer who ate food, assess food safety, or control appliances. Unlike PantryPath/MealBridge (shared food-access inventory) and FreshKeep (kitchen intelligence), MealMate is a private household planning aid focused on coordinated prep, ingredient uncertainty, and explicit task handoffs.

## System and behavior

Nodes: Wi-Fi hub/gateway, two shelf scales, a refrigerator temperature/door context node. ESP32-S3 nodes publish observations over authenticated BLE 5 LE to the hub; the hub queues locally and optionally forwards MQTT over TLS. App users explicitly confirm, edit, or dismiss suggested inventory changes. Weight changes are not treated as proof of a particular item or person. On network loss the hub stores a bounded 7-day event queue and local display/API remain available; after reboot duplicate event IDs are idempotently discarded. No actuator or safety-critical behavior exists.

### Hardware / prototype design

Hub: Raspberry Pi Zero 2 W, 5 V USB-C supply, local SQLite queue, Mosquitto-compatible MQTT client, status LED and physical pairing button. Shelf scale (×2): ESP32-S3-WROOM-1, NAU7802 24-bit load-cell ADC, four 50 kg load cells in a Wheatstone bridge, HX711-compatible calibration weights are not assumed; 3.3 V logic, 5 V bridge excitation, regulated buck/boost supply. Fridge context node: ESP32-C3, SHT31 temperature/RH sensor and reed switch; battery powered, sensor only. Prototype measurements are not certified for food safety or commercial weighing.

The ESP32-S3 I²C bus connects SDA GPIO8 / SCL GPIO9 to NAU7802 (7-bit 0x2A), with 4.7 kΩ pullups to 3.3 V; four load cells connect to ADC differential inputs through the documented bridge terminals. ESP32-C3 I²C SDA GPIO4/SCL GPIO5 connects SHT31 (0x44), 4.7 kΩ pullups; reed switch GPIO6 to ground with internal pull-up. UART0 GPIO43/44 remains available for S3 programming; C3 uses USB Serial/JTAG. These are reference assignments, require board-specific review before fabrication.

Power: S3 shelf node runs from a listed 5 V adapter, never a Li-ion cell in this prototype; bridge excitation current and regulator thermal performance must be measured. C3 sensor node uses two AA cells and deep sleep; no charging circuit. Pi hub uses a certified 5 V USB supply. Separate sensor and logic grounds meet at the ADC module ground. Add input fuse and reverse-polarity protection to any custom powered board.

## Repository contents

- `firmware/common/protocol.h`: versioned bounded event envelope.
- `firmware/shelf-node/main.c`, `fridge-node/main.c`: ESP-IDF scaffolds; sensor drivers and pairing are placeholders.
- `software/dashboard/`: FastAPI ingest/read API scaffold with MQTT forwarding disabled by default.
- `software/ml-pipeline/`: reproducible synthetic-data baseline only; no claims of predictive accuracy.
- `software/mobile-app/`: minimal React Native screen scaffold.
- `hardware/bom/*.csv`: prototype part lists.
- `schematic/`: pin/bus design notes; not native KiCad schematics and not fabrication data.
- `docs/`: architecture, protocol and security notes.

## Build and evidence

Firmware is ESP-IDF C scaffolding and has not been target-built or tested on hardware. Backend uses Python/FastAPI; run `python -m pytest` in `software/dashboard` after installing requirements. Mobile source is a stub and has not been built. ML synthetic baseline is not a deployed model and must not guide food-safety decisions. All BOM pricing/availability is unverified. No electrical safety, calibration, radio compliance, or food-safety validation is claimed.

## Privacy, security, and limitations

Pairing must be physical-button initiated; per-node credentials and encrypted BLE bonding are required in a production implementation. MQTT requires TLS, broker ACLs scoped per household, rotating credentials, bounded payloads, and no anonymous access. Do not expose the API publicly without authentication and authorization. Inventory records are household data: opt-in collection, configurable retention, export/delete, no raw audio/video, no occupant tracking. Signed firmware updates and rollback are future work. Users remain responsible for checking ingredients and food handling.

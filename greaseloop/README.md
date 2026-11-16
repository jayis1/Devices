# GreaseLoop

GreaseLoop is a local-first household used-cooking-oil collection system. It helps people keep cooled frying oil out of sinks and drains, maintain a clean sealed container, and hand full containers to a participating community collection point. Unlike water, food-waste, or pantry systems in this repository, GreaseLoop tracks only explicitly deposited oil and collection handoffs. It never connects to plumbing, heats oil, or dispenses anything.

Status: prototype reference design; no physical hardware has been built or tested. Schematics are pin/net design documents, not fabrication-ready KiCad projects. Firmware drivers, secure provisioning, RF performance, and real-world collection logistics require implementation and validation.

## Nodes and data path

- Kitchen Dock (ESP32-C3-MINI-1, Wi-Fi/BLE): load cell + HX711 measures container mass; a sealed reed switch detects lid closure; a user button confirms a deposit or collection. Powered by isolated, certified 5 V USB supply. It does not sense people or identify food.
- Storage Sentinel (nRF52840 module, BLE 5): measures enclosure temperature and leak-tray wetness; reports a stale-data-capable local status. Coin-cell operation is a design target only; battery life is unmeasured.
- Home Hub (Raspberry Pi Zero 2 W): coordinates node identity, local event store, BLE/Wi-Fi bridge, MQTT, and REST API. Internet is optional; pending events remain local.
- Collection App (React Native scaffold): displays household-owned estimated fill and a manual handoff checklist. No user accounts or remote notifications are implemented.

`Kitchen Dock --Wi-Fi/MQTT--> Home Hub <--BLE--> Storage Sentinel`; optional secured MQTT/TLS sync is a deployment extension, not part of the prototype. The dock is the mass measurement source; the sentinel provides independent enclosure context. Oil is measured by mass, not volume; a configurable density conversion is explicitly approximate.

## Protocol v1

JSON event schema: `{\"v\":1,\"node\":\"dock-01\",\"seq\":12,\"ts\":1730000000,\"kind\":\"deposit\",\"mass_g\":230}`. Supported kinds: `deposit`, `collection`, `container_status`, `environment`. Timestamp is UTC Unix seconds, sequence is monotonically increasing per node, and event identity is `(node,seq)`. Hub rejects unsupported versions, unknown nodes, out-of-range mass (-500..20000 g), oversized messages (>512 bytes), and duplicates. MQTT events are QoS 1 and non-retained. Duplicate deliveries are idempotently discarded. Offline queues are bounded to 64 events; overflow sets a visible gap flag and never fabricates measurements. CRC or transport checks detect corruption, not sender identity. Production requires per-device credentials, TLS, broker ACLs, signed OTA, and documented revocation; this prototype is trusted-LAN only.

## Hardware design summary

The load-cell bridge connects to HX711 differential inputs; HX711 DOUT/SCK use ESP32 GPIO4/GPIO5. A 4-wire 5 kg load cell is mechanically mounted beneath the vessel platform; tare and calibration are explicit service operations. GPIO8 is the deposit button (internal pull-up), GPIO9 the lid reed input (external 10 kΩ pull-up to 3.3 V), GPIO6/7 are I²C SDA/SCL for optional OLED (address 0x3C). Do not connect 5 V logic to ESP32 pins. HX711 is powered at 3.3 V. USB 5 V feeds a 3.3 V regulator sized for radio peaks; the ESP32 module antenna keep-out must be observed.

Storage Sentinel uses nRF52840 module with TMP117 at I²C 0x48, SDA/SCL on module GPIO26/27, and a resistive leak strip on ADC GPIO2 through a 100 kΩ series resistor; BLE advertising identifies a rotating local token. Sensor installation stays outside the oil container and away from hot surfaces. A CR2477 option is illustrative only; verify pulse-current capability or use a supported reservoir capacitor and measure battery life.

Hub: Pi Zero 2 W, USB-C 5 V/2 A supply, microSD, no exposed actuator. Maintain electrical separation from household mains; only certified external supplies are allowed. Suggested BOMs and design notes are under `hardware/bom/` and `schematic/`. Component values are prototype selections; verify datasheets, footprints, mechanical load limits, creepage, and actual sourcing before fabrication.

## Software

- Firmware C: portable protocol encoder/validator and node event examples in `firmware/`; hardware drivers and ESP-IDF/nRF Connect integration are marked integration points.
- Backend: FastAPI endpoints in `software/dashboard/main.py`; MQTT integration is represented by an injectable event-ingest function. Local SQLite persistence and broker security are deployment work.
- ML: `software/ml-pipeline/train.py` is an explicitly synthetic-data pipeline smoke-test, not a trained fullness or pickup model. Operational decisions use thresholds and user confirmation, not ML.
- Mobile: `software/mobile-app/App.tsx` is a UI scaffold only.

## Validation

Run `gcc -std=c11 -Wall -Wextra -Werror firmware/tests/protocol_test.c -o /tmp/greaseloop-test && /tmp/greaseloop-test` and `python3 -m unittest discover -s software/dashboard -v`. Run `python3 software/ml-pipeline/train.py --smoke` to exercise the deterministic synthetic-data path. These checks validate host-side logic only. KiCad/ERC, firmware target builds, MQTT broker integration, RF, electrical safety, load-cell calibration, environmental sealing, and user testing have not been performed.

## Safety, privacy, and limitations

Let hot oil cool fully before pouring. Use a stable, sealed, oil-compatible container and follow local collection rules. GreaseLoop cannot certify oil temperature, container compatibility, leak prevention, or disposal suitability. Never pour oil into a drain. Keep electronics outside the vessel and away from spills; disconnect USB power before cleaning. The system stores coarse mass and explicit actions, not audio, images, location, or identity. Hub retention/deletion is a household responsibility until a retention interface is implemented. Production pairing, authentication, software updates, and data deletion are required before internet exposure.

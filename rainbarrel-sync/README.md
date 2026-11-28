# RainBarrel Sync

**Prototype concept — local-first rainwater harvesting observability and stewardship.** RainBarrel Sync helps households understand captured rainwater, tank capacity, collection performance, and garden demand without controlling plumbing or claiming water is potable.

## Problem and scope
Rainwater barrels are often forgotten until they overflow, run dry, or develop a maintenance issue. This system combines independent observations at the roof collection point, storage tank, and garden-use point. It provides capacity and collection context, maintenance reminders, and conservative availability estimates. It never actuates a valve or pump. Water is non-potable unless separately treated and tested.

This is a concept/prototype specification, not a validated product. Sensor accuracy in real installations, RF range, enclosure durability, and all electrical designs require bench and field validation.

## Nodes and architecture
- **Gateway** (ESP32-C6 + SX1262): receives LoRa telemetry, timestamps with monotonic uptime, caches bounded events locally, serves local status over Wi-Fi, and bridges MQTT/TLS when configured. Internet loss does not stop sensing.
- **Tank node** (ESP32-C3 + ultrasonic ranging module): measures distance to water surface from a fixed, protected mount; stores tank geometry and reports raw distance and quality flags. A manual dipstick remains the reference.
- **Collection node** (ESP32-C3 + tipping-bucket rain gauge + reed switch): reports tip count and local rainfall context. It is mounted on the downspout collection path only if a suitable overflow-safe installation exists.
- **Demand node** (ESP32-C3 + inline low-voltage flow pulse input): observes garden draw at a dedicated gravity-fed outlet. No mains or plumbing valve control.

All leaf nodes send compact versioned packets over region-appropriate LoRa to the gateway. Direct radio is a star topology, not a mesh; gateway placement and link survey are required.

## Data flow and safety
Each packet carries protocol version, node ID, boot ID, monotonic sequence, sample uptime, type, measurements, and CRC. The gateway rejects malformed lengths, unknown versions, duplicates, and stale samples; lost acknowledgements trigger bounded retry with jitter. Leaf nodes retain only the latest sample and a small ring buffer. On gateway/network loss, local measurements continue; no inferred “available” estimate is shown as fresh. The app displays raw values, freshness, confidence/quality flags, and user-entered tank geometry. Calculations are estimates. Never drink harvested water based on this system. Use an independently designed overflow route that cannot flood a building.

Privacy: no identity, audio, image, or occupant-presence data. Local history defaults to 30 days; cloud publication is opt-in. No production credentials are stored here. Configure unique device keys and TLS in deployment; example firmware is not production-secure.

## Hardware and initial pin map
Parts are candidates only; exact module variants and electrical limits must be verified against supplier documentation before PCB design. SX1262 requires 3.3 V logic, RF matching, and antenna per module reference design. Never operate without its antenna. Regional frequency/duty-cycle limits apply.

| Node | MCU / sensing | Interface map (candidate) | Supply concept |
|---|---|---|---|
| Gateway | ESP32-C6 module; SX1262 module | SPI SCK=GPIO6, MOSI=7, MISO=2, NSS=10, BUSY=3, DIO1=4, RESET=5; UART0 debug | regulated 5 V USB-C, 3.3 V regulator sized for radio peaks |
| Tank | ESP32-C3 module; ultrasonic sensor with echo level-shifted to 3.3 V | radio SCK=4, MOSI=6, MISO=5, NSS=7, BUSY=3, DIO1=10, RESET=2; TRIG=GPIO0, ECHO=GPIO1 via divider | protected 5 V input; 3.3 V regulator; outdoor enclosure |
| Collection | ESP32-C3 module; reed-switch tipping bucket | radio pins as tank node; reed input GPIO0 with pull-up and RC debounce | replaceable protected battery; deep sleep between samples |
| Demand | ESP32-C3 module; pulse flow sensor | radio pins as tank node; pulse input GPIO1 with pull-up and debounce | protected low-voltage supply; sensor supply per exact model |

GPIO mappings are proposed and must be checked against module strapping, flash, and board variants. Echo/pulse interfaces must be conditioned; do not apply 5 V logic directly to an MCU. Use strain relief, drip loops, UV-resistant cable, and IP-rated enclosures. No mains wiring.

## Software layout
- `firmware/`: host-portable C protocol and node-loop examples. Hardware drivers are interfaces/stubs, not flashed product firmware.
- `software/dashboard/`: FastAPI local ingest/API with input validation and in-memory prototype state.
- `software/ml-pipeline/`: transparent estimator requiring real calibrated measurements; no synthetic performance claims.
- `software/mobile-app/`: React Native screen stub for status/freshness and manual tank setup.
- `hardware/bom/`: per-node candidate BOMs.
- `schematic/`: pin/net connection notes, not fabrication-ready KiCad schematics.

## Build and test
Host protocol checks: `gcc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/rainbarrel-protocol && /tmp/rainbarrel-protocol`

Backend: `python -m pip install -r software/dashboard/requirements.txt` then `pytest -q software/dashboard`. Mobile: `cd software/mobile-app && npm install && npx tsc --noEmit` (requires Node/React Native environment).

## Maturity / limitations
Protocol and API prototype; hardware nodes are firmware scaffolds. No KiCad ERC, physical prototype, environmental certification, RF compliance, MQTT broker integration test, mobile build, or field calibration has been performed. ML is intentionally a transparent rule-based capacity baseline; do not use it for flood protection, water quality, or safety decisions.

Invented 2026-11-28 by [jayis1](https://github.com/jayis1).
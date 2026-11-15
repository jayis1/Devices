# DishLoop — shared-kitchen reusable dish circulation

Status: prototype reference design, not a food-safety or sanitation certification. Invented and maintained by jayis1.

DishLoop helps volunteer-run community kitchens see how many reusable dish sets are at the clean-rack, serving station, and return point, and prompts staff to reconcile circulation. It addresses the handoff gap between serving and washing, not meal service or food safety. Unlike ShareTable (serving observations and container returns), CookShare (cooking-station readiness), and BorrowBench (tool lending), DishLoop tracks aggregate dish-rack capacity and staff-confirmed wash-batch handoffs. It never tracks diners, controls appliances, or claims a dish is sanitary.

## System topology

Three cooperating nodes report to a local-first Site Hub:

- Rack Sentinel: ESP32-C3, four load-cell channels through NAU7802, measures aggregate rack load; it cannot identify individual dishes.
- Return Gate: ESP32-C3, break-beam sensor and a physical acknowledge button, counts passages as an uncertain directional estimate. Staff resolve ambiguity at the touch UI.
- Site Hub: Raspberry Pi Zero 2 W, BLE gateway plus local FastAPI/SQLite service; receives events, shows counts with confidence/freshness, and records explicit staff actions (wash batch started/completed, rack reset).

Nodes connect over BLE 5 LE; the hub can optionally publish site-scoped summaries over MQTT/TLS. Local operation remains available without internet. No cloud service is required for core operation. Loads and passage counts are operational hints only; staff remain responsible for handling, washing, and sanitation procedures.

## Electronics proposal

Rack Sentinel: ESP32-C3-MINI-1 module, 3.3 V logic; four 10 kg half-bridge load cells wired as a full bridge to NAU7802 breakout (I²C SDA GPIO4, SCL GPIO5, DRDY GPIO6); status LED GPIO7 through 330 Ω. Calibrate with a known mass on the assembled rack, tare only when staff confirm empty, and reject readings outside configured range. Sensor and rack mounting require mechanical validation.

Return Gate: ESP32-C3-MINI-1; 5 V modulated IR break-beam receiver powered from protected USB input, open-collector output pulled to 3.3 V on GPIO4; debounced acknowledgement button GPIO5 to ground with pull-up; status LED GPIO7 via 330 Ω. A single beam does not reliably establish direction or distinguish stacked items; counts are explicitly approximate.

Hub: Raspberry Pi Zero 2 W with 5 V USB supply, BLE, SQLite on managed storage, and local Wi-Fi. Use a dedicated site network, host firewall, and no inbound internet exposure. Prototype pin and module choices require datasheet and carrier-board review before PCB design. No mains wiring or powered kitchen equipment is involved.

## Protocol and data flow

BLE application payload is UTF-8 JSON, maximum 384 bytes: `{"v":1,"node":"random-id","seq":12,"kind":"rack_mass_g","ts_ms":123456,"value":1234.5}`. Kinds are `rack_mass_g`, `return_passage`, `heartbeat`; staff events are created only at the hub. Hub validates version, node enrollment, finite numeric values, sequence monotonicity, and timestamp freshness; duplicate or malformed events are discarded. Sequence persistence and LE Secure Connections pairing are required for production and are not implemented by the reference firmware. MQTT, when explicitly configured, uses TLS and per-site ACLs; never publish borrower or diner data. Broker loss queues a bounded local backlog and must not affect local service.

## Software and contents

- `firmware/`: portable C event validation and producer examples; sensor drivers and BLE transport are integration stubs.
- `hardware/bom/`: per-node prototype BOMs; availability, footprint and compatibility require review.
- `schematic/`: wiring references, not KiCad fabrication designs.
- `software/dashboard/`: FastAPI local event ingest and staff-action API, with MQTT boundary documented.
- `software/ml-pipeline/`: synthetic-data baseline for flagging inconsistent aggregate counts; not a sanitation classifier.
- `software/mobile-app/`: React Native staff dashboard stub; not a production app.
- `docs/`: architecture, API, protocol and operations.
- `scripts/`: local development/test helpers.

Run the backend test with `cd software/dashboard && python3 -m unittest -v`. Compile the portable protocol test with `cc -std=c11 -Wall -Wextra -Werror firmware/common/protocol_test.c -o /tmp/dishloop-test && /tmp/dishloop-test`. Hardware, RF, sanitation, security, mobile build, and electrical validation are not claimed. Retain only aggregate operational events, set a short site-configured retention period, and provide a staff export/delete procedure before deployment.

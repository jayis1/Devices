# PlayShare — community sports-equipment library readiness

PlayShare helps parks departments, schools, and volunteer groups keep shared balls, rackets, bats, and protective gear available without tracking borrowers. It reports aggregate stock, return-bin fullness, and staff-confirmed inspection status so a volunteer can restock or quarantine gear. It does not unlock cabinets, identify people, certify protective equipment, or determine that an item is safe to use.

Novelty: unlike ToolShare (general tools and their availability), PlayShare addresses high-turnover recreational equipment and its return/inspection workflow. Unlike CourtSide (court surface condition), it monitors shared gear rather than a playing surface. The primary user is a volunteer equipment steward; the job is to know what needs replenishing or a human inspection before a scheduled activity.

## System overview

```text
Rack scale (ESP32-C3) ─┐
Return dock (ESP32-C3) ─┼─ BLE advertisements ─ PlayShare gateway (Raspberry Pi Zero 2 W)
Inspection beacon ─────┘                           │ local SQLite + FastAPI
                                                   └─ optional operator mobile web view
```

The gateway timestamps received telemetry and exposes a local operator dashboard API. Nodes buffer the latest state and retry advertisements; no cloud is required. No person-level or borrower records are collected. Site operators enter inspection outcomes manually; sensor readings are context, not inspection substitutes.

## Hardware

Three low-voltage node types; reference design only. Radio is BLE 5 LE in a local star topology, selected for short-range indoor racks and low payload rates. Walls, metal cabinets, and crowding can reduce range; a gateway placement survey is required. No regional sub-GHz radio is used.

| Node | Candidate parts | Interfaces and role | Power |
|---|---|---|---|
| Gateway | Raspberry Pi Zero 2 W, microSD, listed 5 V USB supply | BLE scanning, local API/database, Wi-Fi optional | 5 V, 2 A SELV supply |
| Rack scale | ESP32-C3-MINI-1, 4× 50 kg load cells, HX711, 3.3 V regulator | HX711 GPIO clock/data; BLE stock-change summary | 5 V USB; no battery claim |
| Return dock | ESP32-C3-MINI-1, VL53L0X ToF module, reed switch | I²C occupancy estimate + lid event, BLE | 5 V USB; no battery claim |
| Inspection beacon | ESP32-C3-MINI-1, large button, RGB LED | GPIO button, LED status, BLE staff-confirmed event | 5 V USB; no battery claim |

Candidate pin map for ESP32-C3-MINI-1 carrier: GPIO4 SDA, GPIO5 SCL, GPIO6 HX711 clock, GPIO7 HX711 data, GPIO2 button, GPIO8 status LED. Unused pins remain unassigned. This is a design schedule, not a verified PCB pinout; confirm the selected carrier/module revision, boot behavior, and peripheral voltage before fabrication. I²C pull-ups must terminate at 3.3 V. Provide USB serial programming, EN/BOOT access, input fuse, reverse-polarity protection for custom carriers, and local decoupling. ToF and load-cell modules vary; verify their logic levels and exact part variants.

### Power, safety, and cost

All nodes use listed external 5 V SELV adapters; there are no batteries, mains switching, motors, or locks in this prototype. Design estimates: gateway 5 V/2 A supply; each ESP node supply sized at 1 A to cover radio bursts and peripherals. No battery-life estimate applies. Load capacity and platform mechanical design require weighing-cell calibration and structural review. The ToF dock must be configured for its actual geometry and ambient light. Do not use sensor reports to approve PPE or override staff judgment.

BOMs are candidate purchasing lists with indicative, unverified cost ranges and source links in `hardware/bom/`. Prices vary by supplier and are not quotes. Validate supplier stock, module variants, and complete mechanical/enclosure costs before procurement.

## Firmware and protocol

C firmware entry points are hardware-integration skeletons; the shared portable C protocol codec is compiled and tested on the host. Messages are versioned JSON UTF-8 in BLE manufacturer/service data, capped at 160 bytes: `{v:1,id,node,seq,ts_ms,kind,value,unit}`. `id` is a random provisioned node ID, `seq` is monotonic modulo 2^32, `ts_ms` is node monotonic uptime (not wall time), and the gateway adds receive time. Duplicate `(node,seq)` messages are idempotently ignored; stale state is marked after 15 minutes. BLE advertisements are not authenticated; prototype deployment is restricted to a managed site, with no remote commands. Production requires authenticated provisioning and an authenticated link; do not treat CRC or node IDs as security.

## Software

- `software/dashboard`: FastAPI local prototype, in-memory demonstration event store, validated readiness endpoints; no MQTT/cloud service or production authentication.
- `software/ml-pipeline`: deterministic synthetic-data smoke test for stock-depletion trend features. No trained model, field data, or performance claim; a transparent threshold-based reorder rule remains the fallback.
- `software/mobile-app`: React Native shell placeholder, not built or device-tested. Operators can use the local API documentation in the MVP.

## Build and verification

From this directory run `scripts/test.sh`. The script runs host C codec tests, Python unit tests and syntax checks, validates BOM CSV shape and the app package JSON, and runs the synthetic feature smoke test. Firmware target SDK builds, radio interoperability, load-cell calibration, enclosure testing, accessibility evaluation, privacy/security review, KiCad capture, PCB layout, and physical validation have not been performed. The `schematic/` directory contains net schedules, not native KiCad schematics; this is an unvalidated prototype proposal, not fabrication-ready equipment.

## Documentation

- `docs/product.md`: user stories, scope, privacy, novelty, and outcomes.
- `docs/architecture.md`: data flow, degraded behavior, deployment and power assumptions.
- `docs/protocol.md`: message format and gateway rules.
- `docs/pinout.md`: candidate pin/interface schedules.

No borrower identity, camera, microphone, account, or location history is collected. Telemetry can reveal site inventory patterns; operators should keep data local and delete it when no longer needed. The system is advisory only and makes no safety, accessibility, or equipment-condition certification claims.

Invented and maintained by jayis1.

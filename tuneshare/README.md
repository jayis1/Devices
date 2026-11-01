# TuneShare

**Status: reference design / concept prototype; not a validated product.**

TuneShare helps volunteer-run community music rooms keep shared instruments findable and stored in reasonable environmental conditions. A volunteer can see which instrument bays appear occupied and whether the room has spent time outside a configurable temperature/humidity band, then make a human-confirmed maintenance note. It never identifies borrowers, judges instrument condition, or claims to protect valuable instruments.

## User value and scope

For a volunteer who opens a rehearsal room before each session, TuneShare replaces a manual rack-by-rack and room-condition check with a quick local status board. An estimated 3–5 minutes saved per opening (assumption: 8–12 shared instruments, one opening per day) is a design target, not measured evidence. A paper checklist and $10 hygrometer remain the cheapest alternative; TuneShare is justified only where a shared room has enough instruments and rotating stewards to benefit from aggregate status.

This differs from ToolShare and RepairLoop, which track community tools/workbench readiness; TuneShare is for shared musical-instrument storage and room microclimate, with no tool checkout workflow or repair-space monitoring. It also differs from SeedShare, which tracks seed storage context.

## System overview

Three cooperating nodes use a hub-and-spoke BLE link: a room gateway receives sensor and rack events and maintains a local dashboard; one or more climate nodes measure temperature and relative humidity at instrument storage locations; a rack node reports coarse bay occupancy using non-imaging break-beam sensors. A staff check-in dock has physical buttons for confirm/clear of maintenance reminders. Nodes work locally without cloud access.

```text
Climate node(s) --BLE--> Room gateway/local API --optional MQTT/TLS--> FastAPI service
Rack node ------BLE----/       |                                   |
Check-in dock ---BLE----------/    React Native steward app <-------+
                                  local SQLite queue
```

No cameras, microphones, personal identifiers, lock or actuator. Occupancy is aggregate per bay; the system does not infer who used an instrument.

## Hardware and pin assignments

Reference node designs use ESP32-C3-MINI-1 modules (3.3 V, BLE 5) on each node. This is a prototype choice; module antenna layout, RF performance, EMC, enclosure, and production sourcing require engineering validation. Common supply: regulated 3.3 V from USB-C 5 V input via a 500 mA low-noise regulator; no batteries in this reference revision. Use the module's recommended decoupling and antenna keepout from its datasheet. USB-C is power-only, with CC pull-downs; no charging circuit is present.

| Node | SoC / components | GPIO / bus map |
|---|---|---|
| Gateway | ESP32-C3-MINI-1; USB-UART bridge; USB-C; 3.3 V regulator | UART0 TX GPIO21/RX GPIO20 to bridge; status LED GPIO8; BLE central; USB 5 V input |
| Climate | ESP32-C3-MINI-1; SHT31-DIS-B temperature/RH sensor | I²C SDA GPIO6/SCL GPIO7, 4.7 kΩ pull-ups to 3.3 V; address 0x44; LED GPIO8 |
| Rack | ESP32-C3-MINI-1; 8× Vishay TCST2103-style optical interrupters via 8× 10 kΩ pull-ups | GPIO0–GPIO5 and GPIO9–GPIO10 inputs; active-low occupancy; LED GPIO8 |
| Check-in dock | ESP32-C3-MINI-1; two momentary buttons; LED | Confirm GPIO4, clear GPIO5, LED GPIO8; internal pull-ups |

Pin assignments are a logical reference and must be checked against the exact module revision, boot/strapping requirements, board layout, and selected UART/programming scheme before fabrication. All signals are 3.3 V only. The listed parts and nets are design intent, not a finished KiCad electrical design.

## Protocol and offline behavior

Transport: BLE GATT, gateway as central and nodes as peripherals. Application payload is a compact JSON object (UTF-8) with `v` (protocol version 1), `node_id` (provisioned random identifier), `seq` (uint32 monotonic sequence), `ts` (UTC seconds or null until synchronized), `kind`, and `data`. Kinds: `climate` (`temp_c`, `rh_pct`), `rack` (`occupied_mask` uint16), `checkin` (`action`: `confirm` or `clear`). Reject unknown versions, malformed values, non-finite numbers, and payloads over 256 bytes. Gateway deduplicates by node ID and sequence; readings older than 24 hours are shown stale. BLE pairing/bonding is required; device identity provisioning and key rotation are not implemented in this prototype.

During internet or broker loss, the gateway stores latest state and event queue in SQLite; the local dashboard remains usable. Cloud sync is optional. On gateway reboot, sensor nodes resume periodic advertisements/notifications; time remains uncertain until NTP is available. No update or remote control path is included.

## Power budget (estimates)

All nodes are USB powered. Climate node design target: 35 mA average, 120 mA radio peak; rack: 45 mA average, 140 mA peak with eight optical channels; dock: 25 mA average, 100 mA peak; gateway: 80 mA average, 250 mA peak. Use a regulated supply rated at least 500 mA per node, with margin. These are engineering estimates, not measured values. No battery runtime is claimed.

## Firmware and software

- `firmware/common/`: shared bounded protocol validation and node definitions.
- `firmware/{gateway,climate,rack,checkin}/`: portable C logic and HAL interfaces; hardware drivers are integration stubs, not ESP-IDF applications.
- `software/dashboard/`: FastAPI local/cloud API with SQLite and optional MQTT bridge. Cloud deployment must use TLS and per-device authorization; the included configuration is a development scaffold.
- `software/ml-pipeline/`: optional threshold-baseline evaluation on explicitly synthetic sample data. It is not a trained instrument-health model; no ML inference is required for core function.
- `software/mobile-app/`: React Native/Expo concept scaffold for aggregate status. Not a complete connected application.

## Build and validation

This repository revision provides logical pin/net documentation and BOMs; it does not include native KiCad schematics or PCB layouts, so it is not fabrication-ready. Firmware logic is intended for host compilation with a C11 compiler; target hardware integration requires ESP-IDF and drivers. Python backend and ML scripts are independently syntax-checked. Test commands: `./scripts/test.sh`; `python3 -m compileall -q software`.

## Privacy, safety, and maintenance

Only environmental readings, coarse bay occupancy, and steward-entered maintenance actions are collected. No borrower identities or audio/image data are collected. Local data is retained for 30 days by default; export and deletion are operator-controlled. Cloud sync is opt-in and requires TLS; configure unique credentials outside the repository. Room conditions can be misleading due to sensor placement or calibration drift; staff should inspect instruments directly. TuneShare is not a conservation, safety, or security system and must not replace professional instrument care. Keep optical emitters low-power and enclosed; use certified isolated USB supplies, protect cables from trip hazards, and do not connect to mains-actuated equipment.

Calibration: compare each SHT31 node with a traceable reference at room conditions before installation; record offset and date. Inspect monthly and re-check after relocation. Clean rack optics and verify each bay manually. The physical check-in buttons remain the authoritative record.

## Roadmap

1. Validate selected module and sensor variants against manufacturer datasheets and create native KiCad schematics.
2. Implement ESP-IDF BLE GATT services and secure provisioning.
3. Bench-test RF range, duplicate/reordered event handling, reboot recovery, and sensor offsets.
4. Add accessible, connected mobile screens and deployment hardening only after local prototype evaluation.

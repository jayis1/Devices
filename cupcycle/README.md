# CupCycle — shared reusable-cup return and wash readiness

Status: reference prototype; firmware and software syntax-checked, not hardware- or wash-safety-validated. Invented and maintained by [jayis1](https://github.com/jayis1).

## User and value

A volunteer-run event venue or community cafe loses time counting reusable cups, locating clean stock, and deciding when a wash batch is ready. CupCycle gives staff a simple return count and wash-cycle evidence without identifying patrons. Its earning moment is a low-stock alert before the next service, backed by an auditable count rather than memory.

This is not a dishwasher controller, food-safety instrument, or proof of sanitization. Staff retain responsibility for cleaning, inspection, and local food-code compliance. No camera, face recognition, payment, or patron identity is collected. ML is intentionally omitted: deterministic counting and threshold logic are more appropriate than an unvalidated classifier.

## System (3 hardware nodes)

```
Return Chute --BLE--> Site Hub --Wi-Fi/MQTT--> optional FastAPI service / app
Wash Rack   --BLE--/                 \\-- local display + offline queue
Staff Button --BLE-------------------'
```

| Node | Function | Main parts | Power |
|---|---|---|---|
| Site Hub | Aggregates counts, staff wash-ready acknowledgements, stock thresholds; local status display | ESP32-C6-WROOM-1, SSD1306 128x64 OLED, 3 buttons | 5 V USB-C SELV supply |
| Return Chute | Counts cup drops using break-beam; debounce and reports batch count | ESP32-C3-MINI-1, 5 V IR break-beam sensor, 3.3 V regulator | 5 V USB supply |
| Wash Rack | Staff manually confirms rack inspection and wash completion; no automatic temperature claim | nRF52840 module, 2 buttons, status LED | 2×AA with protected 3.3 V regulator |

BLE 5 LE advertisements carry small telemetry frames; hub connects for acknowledged control/configuration. Wi-Fi/MQTT is optional. Nodes cache a bounded queue and resume with exponential backoff. Radio commands are advisory and do not actuate equipment. BLE pairing/security must be enabled in a production deployment; sample frames have CRC for accidental corruption only, not authentication.

## Power and interfaces

All logic is 3.3 V. USB nodes use regulated 5 V input and local 3.3 V LDO; never expose mains wiring. Hub I2C OLED: GPIO6=SCL, GPIO7=SDA, 0x3C. Chute beam input GPIO4 with external 10 kΩ pull-up to 3.3 V; sensor output must be open-collector/open-drain and 3.3 V safe. nRF52840 rack buttons use GPIO P0.11/P0.12, active-low internal pull-ups. GPIO assignments are prototype targets; verify against the selected module pinout and board schematic before layout. Include SWD/JTAG pads, reset, decoupling, reverse-polarity protection, and ESD protection in production hardware.

Estimated average power: hub 0.25 W (USB); chute 0.12 W average at 10 counts/min; rack 0.3 mA sleep / 8 mA active, 1% active duty gives ~0.38 mA average and ~6 months from 2000 mAh AA pair at 70% usable capacity. Measure actual current; figures exclude battery self-discharge and cold derating.

## Protocol

Shared frame is little-endian: `version:u8, node:u8, kind:u8, sequence:u16, unix_s:u32, value:u16, crc16:u16` (13 bytes, CRC-16/CCITT-FALSE over first 11 bytes). `value` is event count or status enum; units depend on kind and are defined in `docs/protocol.md`. Reject unknown versions, invalid CRC, stale timestamps (>24h where clock is available), and duplicate sequence numbers. Counts saturate at 65535 and are never interpreted as patron counts. Commands have an expiry and are idempotent. Offline state is explicit; hub retains at most 7 days of events then drops oldest and records a gap.

## Software

- `firmware/common/protocol.[ch]`: frame encode/decode and CRC.
- `firmware/*/main.c`: portable application logic with platform HAL hooks; these are integration scaffolds, not vendor-SDK firmware builds.
- `software/dashboard`: FastAPI status and event-ingest API with optional MQTT publication. Set `MQTT_URL` and `MQTT_TOPIC` by environment; no default credentials.
- `software/ml-pipeline`: omitted by design; no justified ML task.
- `software/mobile-app`: minimal React Native status/readiness screen scaffold.

## Build and operation

1. Review schematic notes and verify module pin mappings against current vendor datasheets.
2. Build each node with its vendor SDK after implementing the documented HAL bindings; validate BLE security, input conditioning, EMC, enclosure ingress resistance, and battery behavior.
3. Run backend: `python -m pip install -r software/dashboard/requirements.txt`; configure MQTT via environment; `uvicorn main:app --app-dir software/dashboard`.
4. Staff put the return chute on a stable surface, empty/count-check the collection container each shift, and use rack buttons only after the venue's normal wash process and inspection.
5. When network is unavailable, local counts and hub display continue. Loss of chute or hub shows unknown/stale rather than zero. Staff can override counts and should physically reconcile stock.

## Safety, privacy, and limitations

No mains switching, automatic detergent/water control, sanitization measurement, or patron tracking. Keep electronics outside wet wash zones; use SELV supplies and a splash-resistant enclosure only after environmental validation. Do not mount electronics where they can contaminate food-contact surfaces. Retain aggregate operational events only; configure service retention and access controls before deployment. MQTT requires TLS, unique credentials, topic ACLs, and broker-side device authorization in production. No OTA/update mechanism is implemented in this prototype. No KiCad ERC, physical calibration, food-code, RF, or hardware-in-loop validation has been performed.

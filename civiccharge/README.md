# CivicCharge

CivicCharge is a small, local-first readiness network for public device-charging shelves in libraries and community centers. A visitor can see which outlet groups are available without trying multiple sockets; staff get actionable fault reports instead of discovering dead outlets one complaint at a time. Target prototype BOM: roughly US$100–160 per site, excluding installation and enclosure; cost is an estimate, not a quote.

## Problem, user, and value

The primary user is a library visitor who relies on a phone for transit tickets, interpretation, job applications, or family communication and needs a safe place to recharge. Today they ask staff, occupy a seat while testing sockets, or discover a dead outlet after unpacking. Staff walk the room to reproduce failures. The intended measurable outcome is fewer staff minutes spent locating failed charging points and faster identification of an unavailable outlet group; this prototype makes no quantified savings claim.

A phone app alone cannot determine whether an outlet group is energized, overloaded, or manually reported damaged. Distributed current/voltage sensing and a gateway are needed to correlate status across separate furniture locations while keeping all sensing local. No cameras, microphones, occupancy inference, user identifiers, or remote power switching are used. A physical service button and status LEDs work without a phone.

Different from LiftLink (lift availability), RefillReady (water fixture readiness), QueueKind (service wait bands), and LibraryLoop (seat availability): CivicCharge measures electrical service readiness at shared charging furniture; it neither tracks people nor controls mains power.

## System

```text
[outlet group A monitor] --BLE 5.0--> [site gateway] <--BLE 5.0-- [outlet group B monitor]
                                             | Ethernet/Wi-Fi TLS MQTT (optional)
                                             v
                                  [local API + staff app]
```

| Node | Hardware | Radio / power | Function |
|---|---|---|---|
| Site gateway | Raspberry Pi Zero 2 W; USB-C 5 V/2.5 A supply | BLE 5.0; Wi-Fi/Ethernet adapter optional; mains-certified supply only | Local aggregation, API, MQTT bridge |
| Outlet monitor ×2+ | nRF52840 module (Raytac MDBT50Q-1M); isolated, certified AC measurement front-end specified below | BLE 5.0; 5 V isolated supply from certified module | Reports RMS voltage, group current, temperature, enclosure/tamper switch; no switching |
| Service button | Maintained low-voltage contact to monitor MCU; large labeled button | BLE via associated monitor | Staff marks “needs inspection”; local LED confirms |

Prototype is advisory, not an electrical safety tester, outlet certification, or substitute for inspection. An electrician must design and approve mains wiring and enclosure. Do not assemble exposed mains circuits from this document.

## Hardware design and schematics

The monitor prototype uses an isolated, certified AC measurement module (select a jurisdiction-approved module with documented isolation ratings; exact module selection is an open engineering gate), a calibrated low-voltage current sensor interface, TMP117 temperature sensor (I²C 0x48), and a normally-open service button. The nRF52840 module is powered from an approved isolated 5 V-to-3.3 V supply. GPIO assignments below are a proposed module-level contract and must be checked against the selected module revision and board layout before fabrication.

| Monitor signal | nRF52840 GPIO | Connection |
|---|---:|---|
| I²C SDA / SCL | P0.26 / P0.27 | TMP117; 3.3 V pull-ups, address 0x48 |
| RMS voltage ADC | P0.02 (AIN0) | Isolated measurement output, bounded 0–3.0 V |
| Current ADC | P0.03 (AIN1) | Isolated sensor output, bounded 0–3.0 V |
| Service button | P0.11 | Active-low, internal pull-up, debounce |
| Status LED | P0.13 | Series resistor, active-high |
| Enclosure switch | P0.15 | Active-low, pull-up |
| SWDIO / SWDCLK | SWDIO / SWDCLK | Programming/debug header; retain test access |

The gateway uses Raspberry Pi USB peripherals only; mains sensing is never connected to Pi GPIO. BLE advertisements contain no personal information. See `schematic/` for per-node authoritative conceptual netlists; these are not KiCad schematics or production-approved electrical drawings.

## Power, safety, and adoption

Gateway supply: listed/certified 5 V USB-C supply sized for Pi Zero 2 W peak demand; do not power from an unverified furniture USB port. Each monitor requires an approved isolated supply and enclosure rated for the installation environment. Monitor electronics target under 100 mA at 3.3 V; verify actual peak draw and thermal rise on the chosen module. No battery life applies (mains-powered through certified isolated supply). Loss of gateway leaves outlets unchanged; monitors buffer the latest state and show local fault indication. Loss of monitor produces “unknown/offline,” never “available.” A dead monitor is replaceable by staff; installation/inspection belongs to qualified electrical staff. No relays, contactors, or remote energization are in scope.

Risks/open decisions: exact certified isolated metering module, its accuracy and output transfer function, outlet-group boundary, electrical code, fire enclosure, calibration interval, and local radio compliance need professional review and physical validation. This design makes no electrical safety or regulatory compliance claim. Site staff must keep manufacturer-approved outlet protection in place.

## Protocol and software

BLE GATT application service: UUID namespace reserved by the prototype; production UUID assignment and authenticated bonding are open tasks. Frames are bounded JSON-like CBOR payloads, maximum 128 bytes: `v` (uint8 protocol version=1), `node_id` (8-byte stable provisioned ID), `seq` (uint32 monotonic), `uptime_s` (uint32), `voltage_v_rms` (uint16 centivolts), `current_ma` (uint16), `temp_c_centi` (int16), `flags` (uint16; bit 0 sensor-valid, bit 1 service-request, bit 2 enclosure-open). Gateway rejects unknown versions, malformed lengths, duplicate sequence numbers, out-of-range measurements and stale records. No wall-clock time is required at the sensor; gateway timestamps reception. Detailed sample codec is in `docs/protocol.md`.

FastAPI local service currently holds latest state in process memory (SQLite persistence and MQTT bridge are not implemented). The intended production MQTT bridge is outbound-only over TLS with per-installation credentials supplied out-of-band; no anonymous broker, actuator commands, or secrets in the repo. API: `GET /health`, `GET /v1/outlets`, `POST /v1/outlets/{node_id}/inspection` (staff note, local auth required in deployment). The sample app is a UI scaffold and must not be exposed beyond localhost without authentication and TLS.

The ML folder contains a synthetic-data-only baseline demonstrating fault triage feature plumbing. It is not trained on real facilities, not validated, and rules-based status is authoritative. The React Native scaffold displays availability, unknown/offline state, and a staff inspection action; physical button/LED remains available without a smartphone.

## Build, test, and calibration

1. Review node netlists and resolve the isolation module, enclosure, ratings, and applicable electrical code with a qualified electrical engineer.
2. Assemble and test low-voltage electronics on an isolated bench supply first. Never connect a prototype directly to building mains.
3. Install FastAPI with `software/dashboard/requirements.txt`; run `uvicorn app.main:app --host 127.0.0.1` from that directory.
4. Run `python3 -m unittest discover -s software/dashboard/tests` and `python3 -m unittest discover -s software/ml-pipeline/tests`.
5. Firmware is portable C scaffolding; vendor SDK integration, BLE GATT transport, calibrated ADC drivers, and hardware tests remain unimplemented. See firmware README and netlists.
6. Calibration requires a qualified technician using traceable instruments and a safe isolated source; store calibration constants/version per monitor. Do not use the monitor to certify outlets.

## Privacy, security, and limitations

Data is limited to outlet-group electrical summaries and service flags. No occupancy, device identity, or visitor data is collected. Keep data local by default; optional cloud reporting needs site consent, access controls, retention/deletion policy, TLS, unique device credentials, and key revocation. Production BLE provisioning, authenticated bonding, signed firmware updates, and backend staff authentication are not yet implemented; the sample backend is local-only and not production-secure. Treat tamper indication as maintenance information, not evidence of intrusion.

Limitations: conceptual electrical design; no valid KiCad schematic, PCB, enclosure, certification, verified component-specific metering design, vendor SDK build, RF test, or physical validation. It reports measured conditions only; it cannot guarantee that a socket is safe or usable for every charger.

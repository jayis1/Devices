# PrintShare

Status: experimental reference design. Firmware, electrical design, and physical performance have not been validated on hardware; this is not a certified fire-safety system.

## Problem brief

Community workshops often waste volunteer and member time discovering that a shared 3D printer is occupied, paused, out of filament, or has wet material. PrintShare provides local, anonymous printer readiness and queue context. The measurable target is reducing avoidable walk-up checks; this design makes no measured savings claim.

## Novelty and scope

Unlike WorkshopSync (general tool/PPE/air readiness), PrintShare monitors additive-manufacturing job state and filament storage conditions. It does not control printers, identify users, publish camera images, or claim print quality/safety. The device is a workshop operations aid, not an unattended printer controller.

## System

```
Printer optical/USB observer --BLE--> ESP32-S3 gateway --local MQTT--> FastAPI / React Native
Filament dry-box sensors ------BLE----/       |-- local SQLite queue/cache
```

Observer nodes attach non-invasively to printer status LEDs / enclosure sensors; no mains wiring. Gateway coordinates BLE advertisements and local telemetry. Nodes report only a configured pseudonymous printer ID, state, sequence, battery, and timestamp. BLE link-layer security must be provisioned per deployment; prototype firmware uses a bounded demo frame and is not secure for hostile environments.

### Nodes

| Node | Components | Supply / job |
|---|---|---|
| Gateway | ESP32-S3-DevKitC-1, Wi-Fi, BLE 5 | USB 5 V; local aggregation and MQTT |
| Printer observer | nRF52840 module, phototransistor status input, reed enclosure switch | 3.3 V USB supply; observes status only |
| Filament sentinel | nRF52840 module, SHT31, reed door switch | 3.3 V USB supply; dry-box RH and door state |

Pin assignments, block-level schematics and bring-up notes are in `schematic/README.md`. BOMs are estimates in `hardware/bom/`. No PCB layout is supplied.

## Protocol and operation

Portable C frame: version, message type, node ID, sequence, monotonic uptime, state, humidity in centi-%RH, temperature in centi-degrees C, battery mV, CRC-16/CCITT-FALSE. Type/state interpretation and MQTT mapping: `docs/protocol.md`. The C codec validates version, frame length, type, and CRC; the JSON API validates field ranges only and does not decode C frames. MQTT bridge deployment is described but not bundled as an always-on service.

No-cloud behavior: gateway and API can run on the same LAN; loss of WAN does not affect local observation. Gateway outage loses live status; nodes resume on reconnect and report current state (not a complete event history). Sequence wrap is modulo 65536; stale status expires after 10 minutes. Duplicate sequences are ignored in the gateway integration contract. No actuation is possible.

## Quick start

1. Install Python 3.11+ and `pip install -r software/dashboard/requirements.txt`.
2. Run `uvicorn app:app --app-dir software/dashboard --host 127.0.0.1 --port 8000`.
3. POST a sample observation per `docs/api.md`; open `/health`.
4. Install ESP-IDF 5.x for gateway and nRF Connect SDK 2.x for observer nodes; these sketches require board-port integration before flashing and are not represented as build-verified firmware.
5. Mobile shell: `cd software/mobile-app && npm install && npm start` (starter UI only; not device-tested).

## Data, safety, and limitations

No account or person identifier is required. MQTT credentials and TLS certificates must be supplied out-of-band; never use the example placeholders. Configure broker ACLs so each gateway publishes only its site topic. The API sample has no authentication and must remain loopback/LAN-only behind an authenticated gateway in any deployment. Retain telemetry locally only as long as the workshop needs; implement deletion in the production store.

A wet-filament estimate is not a material-quality certification. Humidity readings are contextual and require sensor calibration. A printer's own thermal runaway protection and supervision remain mandatory; PrintShare neither changes printer controls nor replaces smoke alarms. On sensor fault or lost radio, UI must show unknown/stale, never ready. The ML folder contains a deterministic baseline, not trained or evaluated model results.

## Verification status

Portable protocol C and backend tests are included. Hardware, radio interoperability, KiCad ERC, firmware SDK builds, mobile build, security, and field performance remain unverified. See `docs/architecture.md` and `docs/api.md`.

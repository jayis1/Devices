# ChargeShare

Status: prototype reference design; not certified, electrically reviewed, or hardware-tested. Author: jayis1.

## Problem, user, and wedge

In apartment common rooms, volunteer events, and community centers, people arrive with low phone batteries but cannot tell whether shared USB charging points are usable or which ports have failed. ChargeShare gives the site host an at-a-glance port availability and fault report. Wedge: unlike a consumer smart plug or public charging network, this is local, anonymous maintenance telemetry for low-voltage USB outlets; it neither identifies users nor handles payments.

Persona: Sam, who opens a neighborhood center before a weekly evening class, checks the e-ink hub display and sees that one charging bay is unavailable before visitors arrive. A reasonable pilot outcome is avoiding one 5-minute manual port-by-port check per opening; this is an assumption to test, not a measured saving. Day 1 shows current port health; day 30 can show recurring port faults; day 365 supports replacement planning from local history. No ML is required for core value.

## Architecture

```
USB bay monitor (ESP32-C3, current/voltage sensing) -- BLE -->
USB bay monitor (ESP32-C3) -------------------------- BLE --> ESP32-S3 Hub
                                                               | Wi-Fi/MQTT
                                                               v
                                                        FastAPI + SQLite
                                                               ^
                                                        React Native status
```

Two independently powered bay nodes are installed at physically separate USB charging locations; the hub aggregates their telemetry and retains the last status during internet outage. Nodes monitor only 5 V output presence and current within a certified, off-the-shelf USB supply; they do not switch or modify mains wiring. They cannot determine whether a person is charging or identify a device. Operators must provide approved USB power supplies and supervise access.

## Hardware contract

Hub: ESP32-S3-DevKitC-1, USB 5 V input, BLE 5.0 and Wi-Fi. Bay monitor: ESP32-C3-DevKitM-1, INA219 current/voltage monitor (I2C), inline USB-C breakout rated for intended current, powered from the USB bay supply. Use isolated measurement only if a future design touches mains; this prototype is strictly USB SELV. Exact nets and pins: `docs/design.md`. Schematic folder contains honest netlist/design notes, not KiCad projects or fabrication files.

BLE star telemetry, hub Wi-Fi/MQTT topic `chargeshare/v1/{site}/{node}/telemetry`. Payload fields and failure behavior are specified in `docs/protocol.md`. No actuation: a failed node is marked stale, hub remains usable offline, and backend outage does not affect local display.

## Software and ML

Portable C frame codec and node acquisition skeleton are under `firmware/`. FastAPI accepts measurements and exposes latest status; the ML script is a synthetic-data demonstration only and is not used for decisions. React Native app is a minimal status stub. No credentials are shipped; prototype API is unauthenticated and must bind to loopback or a trusted test LAN. Production deployment requires unique credentials, TLS, broker ACLs, and access control.

## Power and limits

Hub and monitors are USB-powered from approved 5 V supplies; no battery-life claim. The current sensor range and inline connector must be selected to match the supply's rated current; do not exceed component/connector ratings. Sensor readings indicate electrical output only, not device compatibility, charging speed, or safety. A failed reading is “unknown,” never “available.”

## Build and verification

See `docs/design.md` for pin map and bring-up and `hardware/bom/` for per-node BOMs. Build the portable codec with the command in `firmware/README.md`; run the API tests with `python -m unittest discover -s software/dashboard/tests`. KiCad capture, target SDK builds, mobile builds, electrical review, security review, and physical testing have not been completed.

## Completeness

| Artifact | Status |
|---|---|
| Architecture, pin/power/net notes | Implemented design specification; unreviewed |
| Shared C protocol | Host-testable implementation |
| Node firmware | Acquisition/transport integration stub |
| FastAPI service | Implemented, host tests included |
| ML | Synthetic demonstration only |
| Mobile app | Stub |
| Schematics | Text netlist specs; no KiCad capture |
| BOMs | Reference parts and estimates; verify before procurement |

## Privacy and safety

No audio, images, identity, or client-device identifiers are collected. Telemetry is limited to node ID, sequence, voltage/current sample, and timestamp. Operator-controlled retention; erase local database to delete prototype history. Do not connect this prototype to mains or expose service publicly. It is not a certified charging product.

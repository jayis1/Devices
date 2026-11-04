# RepairCafe — community repair intake and bench coordination

Invented and maintained by jayis1. Prototype reference design only; hardware is unbuilt and not safety-certified.

## Why
Volunteer repair cafés lose time sorting donated small appliances, locating free benches, and tracking item workflow. RepairCafe gives a coordinator a local FIFO queue and staff-declared bench states. Unlike RepairLoop (tool presence/ventilation) and CommunityRepair (public bicycle repair infrastructure), this is for event intake and work routing. It collects no visitor identity and never powers, diagnoses, or repairs equipment.

## System

```text
Ticket Dock (ESP32-C3) -- BLE event --> Gateway (Pico W + host/USB BLE)
Bench Beacon(s) (ESP32-C3) -- BLE --> Gateway SQLite -> localhost FastAPI
                                                  -> optional operator mobile UI
```

Three node roles: one gateway, one intake ticket dock, and one or more bench beacons. The gateway coordinates event sequence acceptance, deduplication and local display/API. A Raspberry Pi Pico W alone cannot run the documented FastAPI/SQLite host service: in this prototype it is the coordinating radio/USB bridge and requires a separate Linux host. The USB BLE bridge is unimplemented. Do not mistake this concept diagram for a finished system.

## Hardware and candidate interfaces

| Node | SoC/components | Candidate assignments | Power/failure |
|---|---|---|---|
| Gateway | RP2040 Pico W; USB BLE adapter; Linux host for API/SQLite | USB serial/bridge interface TBD; Wi-Fi only for optional upstream | Certified SELV USB supply; host outage -> paper queue; local event store should survive reboot once persistence/backup implemented |
| Ticket dock | ESP32-C3-MINI-1; button, LED | GPIO4 button, GPIO5 LED; BLE 5 LE | USB 5 V to regulated 3.3 V; on power loss issue paper ticket |
| Bench beacon | ESP32-C3-MINI-1; 3 buttons, LED | GPIO4/5/6 buttons, GPIO7 LED; BLE 5 LE | USB powered; loss or stale heartbeat means UNKNOWN, never available |

GPIO assignments are tentative module-level candidates, not verified against a specific carrier PCB. Use input pull-ups and debounce; confirm boot-strapping constraints, board routing, RF keepout, USB-C CC resistors, regulator thermal margin, ESD/overcurrent protection and datasheet limits before schematic/PCB release. Nodes have no hazardous actuator and no mains connection. BOMs are indicative, not procurement-confirmed.

## Protocol / reliability

`firmware/common/protocol.h` defines a version-1 logical event: version, type, node ID, monotonic sequence, ticket ID, state, CRC-16/CCITT-FALSE. Maximum serialized frame 48 bytes; wire endianness is little-endian when serialization is implemented. Supported events are ticket-created, bench-state, ticket-state. Receiver must validate ranges, CRC, version, duplicate sequence and age. No ACK/retry, BLE GATT service, persistent sequence allocation, encryption, provisioning or replay protection exists yet. Reference C currently builds only as portable logic, not target firmware.

Production should use per-device credentials, authenticated encrypted BLE provisioning, TLS MQTT with broker CA verification and per-device topic ACL, revocation/rotation, signed OTA and bounded retry with jitter. MQTT/cloud are optional and not implemented. Offline work should remain local. If queue storage is full, alert the coordinator and require paper/export; never silently discard. Cloud loss must not stop intake. Missing hub means paper fallback.

## Software and API

`software/dashboard/main.py` provides localhost demo endpoints: `POST /v1/events` validates a bounded schema and returns 409 on duplicate `(node_id,sequence)`; `GET /v1/events?limit=1..500` returns newest events. It has no authentication, MQTT listener, BLE integration, rate limit, retention/deletion controls, TLS, or object-level authorization. Bind only to loopback; never expose to a network as-is. SQLite is local and demo-grade.

Mobile UI is a React Native display stub; package lacks a full RN build setup. ML script creates synthetic queue-duration rows only; it does not train a model. FIFO is the deterministic baseline and only intended dispatch behavior. No wait-time accuracy or field benefit is claimed.

## Privacy, accessibility, safety

No cameras, microphones, personal data, contact details, serial numbers, tracking, remote commands, electrical testing, or repair advice. Use random event-scoped ticket IDs; avoid writing identifying details on the device. Define short retention and operator deletion/export before real deployment. Provide paper/large-print and audible alternatives; UI accessibility is not yet validated. Volunteers must follow their own electrical and battery safety procedures. System is not a safety inspection or certification device.

## Bring-up and build status

1. Install Python 3.10+ and dashboard requirements; run `uvicorn main:app --host 127.0.0.1 --port 8000` from dashboard directory.
2. Compile logic examples with `gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only firmware/ticket-dock/main.c` and likewise bench beacon. ESP-IDF drivers are TODO.
3. `python software/ml-pipeline/train.py` generates synthetic-only data.
4. Schematics are not yet KiCad electrical designs; see `schematic/README.md`. No ERC, target firmware link, mobile build, hardware assembly, BLE interoperability, security audit, or user study has been performed.

## Readiness

Architecture: specified. Reference source: implemented. Host API: basic local ingestion. Host compile/syntax: verifiable. Target firmware/radio integration: not implemented. CAD/ERC: not implemented. Physical prototype, regulatory review, accessibility testing and security validation: not performed. Author: jayis1.

# CivicRest — public restroom readiness
A facilities worker should not have to make repeated blind walks to discover empty soap or paper. CivicRest proposes maintenance advisories for libraries and community centers. Target: fewer avoidable inspection trips; no measured savings are claimed. Multiple nodes are needed because fixtures are physically distributed.

## Architecture
Door Sentinel + Supply Scale → BLE → Hub Gateway → optional MQTT/API → staff checklist. No visitor identity, camera, microphone, or occupancy estimate. See [architecture](docs/architecture.md), [protocol](docs/protocol.md), and [API](docs/api.md).

## Hardware and BOM
Door Sentinel: ESP32-C3-MINI-1 + reed contact. Supply Scale: ESP32-C3-MINI-1 + load-cell ADC. Hub: Raspberry Pi Zero 2 W. BOM files in hardware/bom; rough estimate $36 total (unassembled, excludes enclosure, batteries, shipping and tooling). Prices require supplier confirmation. No verified electrical schematic exists. Pin maps, current draw and runtime are uncharacterized.

## Software status
Common C framing/CRC is implemented. Per-node drivers and radio adapters are TODO. FastAPI ingestion is a prototype; MQTT transport/authentication is not implemented. ML script is synthetic plumbing only, no trained model. Mobile app is a scaffold.

## Safety, privacy, limits
Advisory maintenance only; human inspection remains authoritative. Not for emergency, access control, hygiene compliance, or safety certification. Low-voltage only; product and regional radio compliance need review. Proposed 30-day coarse-event retention is not implemented. Cloud loss prevents remote alerting. No verified KiCad design, bench testing, or measured benefit.

## Checks
Compile shared protocol with `gcc -std=c99 -Wall -Wextra -Werror -Ifirmware/common -fsyntax-only firmware/common/protocol.c`. Backend dependencies are in software/dashboard/requirements.txt; run `uvicorn main:app` from that directory after install. Physical calibration and complete system testing remain outstanding.

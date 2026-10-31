# CookShare — shared-kitchen appliance readiness

CookShare helps residents and volunteers in apartment or community kitchens avoid unnecessary trips and interruptions by showing whether shared cooking stations appear occupied. Unlike package-room and pantry systems, it observes the active state of a cooking station, not packages, food stock, or people. It has no camera, microphone, identity inference, mains sensing, or appliance control. It is a non-safety advisory: current draw from an approved, isolated low-voltage DC supply is only a proxy for appliance use.

## System

```text
2 × Station Observer (ESP32-C3 + INA219 + isolated DC input + reed switch)
       │ ESP-NOW encrypted unicast, bounded binary v1 frames
       ▼
Kitchen Gateway (ESP32-C6, Wi-Fi, local state cache)
       │ MQTT over TLS (deployment responsibility)
       ▼
FastAPI service → SQLite event/state store → React Native status client
                            └→ rule baseline / optional synthetic-data exercise
```

The three nodes are independent hardware units: one gateway and two observers (expandable). Place an observer beside each station; only connect its INA219 to an isolated, extra-low-voltage DC output specifically approved for the appliance/supply. Never connect this design to mains, internal appliance wiring, or an unisolated supply. The reed switch is a low-voltage enclosure/door context input, not a safety interlock.

| Node | Compute/radio | Inputs | Power | Role |
|---|---|---|---|---|
| Observer ×2 | ESP32-C3-MINI-1, ESP-NOW 2.4 GHz | INA219 current/voltage over I²C; reed switch | USB 5 V; regulator on module | Publish calibrated active/idle/unknown observation |
| Gateway | ESP32-C6-DevKitC-1, Wi-Fi + ESP-NOW | Observer frames; status LED | USB 5 V | Deduplicate, retain latest state, MQTT forward |
| Service | Linux/Python | MQTT JSON | Site host | Persist state and serve authenticated-by-network-boundary prototype API |

## Prototype status and safety

Design/prototype, not production-ready. Firmware files are portable C protocol and application logic with a narrow platform adapter boundary; vendor SDK radio, sensor, Wi-Fi, and MQTT bindings are explicitly TODO. The mobile app is a UI stub. No KiCad schematic capture or PCB layout is supplied: node pin/net schedules are in `docs/architecture.md`. No device has been fabricated or bench-tested. Component costs are planning estimates, not quotes. This system cannot establish that a station is safe, clean, available, or suitable to use. Missing, stale, uncalibrated, or conflicting telemetry must display `unknown`, never `available`.

## Build and test

Host protocol test (C11):

```sh
gcc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/cookshare-test
/tmp/cookshare-test
```

Backend checks and local run:

```sh
python -m pip install -r software/dashboard/requirements.txt
python -m unittest discover -s software/dashboard/tests
cd software/dashboard && uvicorn app:app --host 127.0.0.1 --port 8000
```

Set `COOKSHARE_DB` to a writable SQLite path. API is a local prototype; production deployment must add user authentication, authorization, TLS termination, MQTT broker ACLs/TLS and rate limits. Configure MQTT only on a trusted site network. Do not expose it publicly.

## User flow

1. Site operator mounts observer beside station and records the station ID.
2. Calibrate idle/active current thresholds using the station's approved external DC supply; no appliance disassembly.
3. Gateway pairs observers using an out-of-band site key and forwards state. SDK pairing and key provisioning are not implemented in this prototype.
4. Residents view per-station state and observation age. State older than 90 seconds is unknown.

## Privacy and data handling

No occupant identity or audio/video is collected. The service stores station identifier, coarse state, sequence, and timestamp; configure site retention and deletion in deployment. The demo SQLite store is local and does not upload data. Internet loss leaves the UI stale/unknown; an actual local gateway cache requires platform integration. Do not use telemetry to monitor or discipline residents.

## Contents

- `docs/architecture.md`, `docs/protocol.md`, `docs/api.md` — implementation contract and limitations.
- `firmware/common/` — shared bounded frame codec, CRC-16, and host test.
- `firmware/observer/`, `firmware/gateway/` — C application logic and pin schedules.
- `hardware/bom/` — per-node planning BOMs.
- `schematic/` — honest pin/net schedules, not KiCad capture.
- `software/dashboard/` — FastAPI + SQLite API and tests.
- `software/ml-pipeline/` — synthetic smoke-data generation only; no trained model or accuracy claim.
- `software/mobile-app/` — React Native display stub.
- `scripts/` — local validation helper.

Invented and maintained by jayis1.

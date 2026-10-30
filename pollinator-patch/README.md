# PollinatorPatch — community pollinator-garden stewardship

PollinatorPatch helps volunteer stewards of small public pollinator gardens answer a practical question: which beds are dry, which are flowering, and where should the next volunteer visit? Today these decisions rely on infrequent visual checks and scattered notes. Moisture is measured at the bed, bloom observations are entered at the garden entrance, and a site gateway reconciles both; one phone cannot provide continuous bed context across multiple plots. Delight moment: a volunteer opens the app and sees a map of beds that need a human check, with the last moisture reading and a dated bloom note.

This is an advisory prototype for community gardens—not an ecological survey, pesticide recommendation, irrigation controller, or guarantee of pollinator health. Human observation remains authoritative. It does not identify individuals, capture images/audio, or autonomously actuate water or chemicals.

## System

```text
[Bed probe ESP32-C3] --BLE--> [Site gateway ESP32-S3] <--BLE-- [Bloom check-in ESP32-C3]
                                      | Wi-Fi/MQTT TLS (optional)
                                      v
                              [FastAPI + SQLite] --> [React Native volunteer view]
```

- **Bed probe (one per bed):** ESP32-C3 dev board, capacitive moisture probe, DS18B20 soil temperature, battery voltage divider. Reports sampled moisture proxy and temperature; calibration is bed/soil specific.
- **Bloom check-in:** ESP32-C3 + buttons/rotary input and small e-paper status panel. Volunteer logs a coarse bloom category and bed ID; no camera or species classifier.
- **Site gateway:** ESP32-S3, local queue and status display; validates node frames and stores observations locally during internet outages. Gateway is a prototype software role; its local persistence requires integration.
- **Cloud/edge:** FastAPI event intake, SQLite demo store, optional MQTT TLS adapter. Internet is not required for sensing or local check-in; the app/API are unavailable offsite during an outage.
- **Mobile:** React Native interface scaffold for bed status and volunteer notes; not a production app.

BLE is chosen for low-cost, short-range garden nodes and phone-like provisioning; it is not a guaranteed long-range mesh. Place the gateway within tested radio range. Pairing requires physical access; production deployments need per-device credentials, encrypted BLE bonding, TLS certificates, and key rotation. Prototype frame CRC detects corruption only—it does not authenticate senders.

## Power, parts and safety

All nodes use protected, replaceable USB power banks or certified USB supplies; battery runtime is not claimed until measured with the selected board, sensor duty cycle, and local conditions. No bare Li-ion cell charging circuit is specified. Probe electronics must be weatherproofed while keeping the sensing element in soil; do not bury an unsealed dev board. Use low-voltage isolated USB power only. No pumps, mains wiring, pesticide or fertilizer control. Moisture readings are relative proxies and must not be interpreted as plant-health measurements.

Indicative planning costs: gateway $18, probe $15, check-in $22 (USD, approximate single-unit parts estimates; enclosure, labor, shipping and taxes excluded). BOMs are in `hardware/bom/`; costs are estimates, not quotes. Pin maps and connection assumptions are in `docs/architecture.md`. The `schematic/` drawings are reference connection sheets, not KiCad schematics or manufacturing-ready designs.

## Software and maturity

| Subsystem | Status |
|---|---|
| C protocol validation and node loops | Reference firmware; board radio/sensor drivers are integration hooks, not a built ESP-IDF image |
| Gateway behavior | Architecture and message contract; durable offline queue needs implementation on target hardware |
| FastAPI + SQLite | Local demonstration API with input bounds; not production-authenticated |
| MQTT | TLS configuration contract; broker adapter is optional and deployment-dependent |
| ML | Deferred: threshold/rule summaries are more appropriate than unsupported model claims; synthetic evaluation scaffold only |
| Mobile | UI/API scaffold, not a complete release app |
| Hardware | Parts and pin references only; no ERC, PCB, RF or environmental validation performed |

## Quick checks

From this directory:

```sh
python3 -m unittest discover -s software/dashboard/tests -v
python3 -m compileall -q software/dashboard software/ml-pipeline
 gcc -std=c11 -Wall -Wextra -Werror -Ifirmware/common -fsyntax-only firmware/common/protocol.c firmware/bed-probe/main.c firmware/bloom-checkin/main.c firmware/site-gateway/main.c
```

The ML helper uses synthetic data and reports descriptive moisture summaries only; it is not trained or validated for ecological prediction. See `docs/` for protocol, API, architecture, and design rationale. No field, radio, battery-life, environmental, ecological, security, or regulatory validation is claimed.

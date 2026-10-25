# ParkPulse

Park stewards often learn that fountains are dry or bins full only after a visitor complaint or a manual patrol. ParkPulse provides fixture-level evidence and maintenance triage, not visitor surveillance. It differs from existing home water-protection and community-space systems by focusing on public-park asset condition and maintenance workflows.

## Nodes

| Node | SoC | Role | Link |
|---|---|---|---|
| fountain-sentinel | ESP32-C6-MINI-1 | pulse flow and leak observation | regional Sub-GHz |
| bin-sentinel | ESP32-C6-MINI-1 | fill distance and weight trend | regional Sub-GHz |
| gateway | Raspberry Pi Zero 2 W | local rules, queue, API, MQTT bridge | Wi-Fi/Ethernet uplink |

The gateway coordinates nodes. Core status remains local; WAN is optional. No actuation is performed. Open loop alerts require human inspection.

## System data and safety

See [architecture](docs/architecture.md), [protocol](docs/protocol.md), [API](docs/api.md), and [schematic status](schematic/README.md). Approximate sentinel runtime: 1500 mAh / 1.2 mA / 24 h = 52 days, before solar input; this is a design estimate, not measured. Device sensors do not certify water safety. Not a public-infrastructure safety device. Obtain facilities approval; do not connect to potable wetted paths.

## Build / validation

Firmware files use a host-testable protocol and HAL placeholders; no vendor SDK build has been completed. `gcc -std=c11 -Wall -Wextra` can compile protocol sources. Demo API: install `software/dashboard/requirements.txt`, run `uvicorn app:app --app-dir software/dashboard --host 127.0.0.1`. ML script uses synthetic samples only. Mobile app is a non-production UI stub. BOM files list candidate parts and approximate costs; verify variants and sourcing before purchase.

## Privacy and known gaps

No personal identifiers or raw imagery/audio. Quantize counts and retain demo telemetry 30 days. Demo API has no authentication and must remain localhost-only. CRC is not cryptographic security. No KiCad schematic, PCB, certification, radio-range test, or physical validation is included. Production needs radio authentication, access control, tested enclosure/power design, and labeled field data before any predictive model is useful.

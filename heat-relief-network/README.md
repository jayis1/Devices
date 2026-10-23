# Heat Relief Network

A prototype neighborhood heatwave coordination system for libraries, community centers, and pop-up cooling sites. It gives operators a trustworthy live view of indoor conditions, room occupancy bands, water availability, and site readiness without identifying visitors.

## User and value

Persona: a public-library facilities coordinator opening a cooling room during a summer heat advisory. Today they rely on phone calls and periodic walk-throughs to learn whether a room is hot, crowded, or out of drinking water. The system aims to reduce manual site checks; that outcome is a testable pilot target, not a measured result. Alternatives include manual checklists, standalone thermostats, and commercial building-management systems; none necessarily provide low-cost, privacy-preserving coordination across temporary sites.

## System

Three hardware roles: Site Gateway (ESP32-S3, Wi-Fi/Ethernet uplink and local coordination), Climate Node (ESP32-C6, temperature/humidity/CO2), and Supply Node (ESP32-C6, load-cell water-container estimate and door-counting beam). The climate and supply nodes report over encrypted BLE connections to the gateway; cloud MQTT is optional. Removing the gateway loses local aggregation and operator display; removing either sensor node removes a separate operational signal. No person-level tracking, camera, microphone, medical measurement, or autonomous HVAC control.

```text
Climate Node -- BLE GATT --> Site Gateway -- MQTT/TLS --> FastAPI dashboard
Supply Node  -- BLE GATT --> Site Gateway -- local cache --> Mobile operator app
```

The gateway remains usable without internet. Occupancy is an approximate count derived from paired beam crossings, not identity or a safety-grade capacity counter. ML is an optional synthetic-data demonstration of site-readiness anomaly scoring; transparent threshold rules remain the operational baseline.

## Build status

Prototype reference design. Pin/net assignments and BOMs are design drafts, not fabrication-validated. `schematic/` contains honest per-node netlist documents, not KiCad electrical schematics. Firmware application loops are portable C demonstrations; board SDK integration and radio commissioning remain unverified. The React Native app is a scaffold, not a production app. No field results or model performance are claimed.

## Contents

- [Architecture and trust boundaries](docs/architecture.md)
- [Protocol](docs/protocol.md)
- [API](docs/api.md)
- [Power assumptions](docs/power.md)
- [Safety and privacy](docs/safety.md)
- [Pin/net assignments](docs/pin-assignments.md)
- [Firmware](firmware/)
- [BOMs](hardware/bom/)
- [Backend](software/dashboard/)
- [ML demo](software/ml-pipeline/)
- [Mobile scaffold](software/mobile-app/)
- [Scripts](scripts/)

## Safety, security, and limitations

Use only low-voltage, SELV power supplies. Do not use this prototype for emergency dispatch, medical decisions, statutory occupancy limits, or life-safety control. Sensor readings can drift or fail; staff must follow local heat-response procedures and independently verify conditions. BLE pairing must be provisioned per device; MQTT is TLS-only when enabled, credentials come from environment variables, and no default secret is supplied. Firmware signing and secure boot are deployment requirements not implemented in this prototype. Raw occupancy counts are local ephemeral values; cloud telemetry retention should be configured to 30 days or less and deleted on site decommissioning. FCC/CE and local radio approvals are required before sale.

## User-visible failure behavior

| Failure | Behavior |
|---|---|
| Internet/cloud unavailable | Gateway continues local display and buffers bounded telemetry |
| Gateway power loss | Nodes stop reporting; staff use manual site checks |
| Climate sensor absent/out of range | Mark site data stale/invalid; never substitute a safe-looking value |
| Supply node battery low | Show water estimate unavailable and request manual check |
| Phone unavailable | Gateway status display remains primary |

## Bring-up

1. Review pin assignments and electrical limits before wiring.
2. Build firmware logic with a host C compiler; add vendor SDK/HAL integration for selected boards.
3. Configure dashboard with `software/dashboard/.env.example`; never commit real credentials.
4. Run `python3 -m pip install -r software/dashboard/requirements.txt` and `uvicorn main:app --app-dir software/dashboard/backend` from the repository root.
5. Generate explicitly synthetic training data with `python3 software/ml-pipeline/train.py --synthetic`.

Invented and maintained by [jayis1](https://github.com/jayis1).

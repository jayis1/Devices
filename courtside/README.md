# CourtSide

Local-first public court condition context for parks crews and volunteer stewards. A basketball or tennis player can check whether a court was recently observed dry, whether shared fixed equipment has been inspected, and whether a maintenance report is pending—without cameras, accounts, visitor tracking, or claims that the court is safe.

## User value

A parks crew gets one concise, timestamped maintenance queue instead of relying on scattered calls and stale paper notes. Success is measured by time from staff observation to a visible maintenance card and the fraction of cards closed after human inspection; the system does not certify playability.

## Fidelity and status

| Area | Status |
|---|---|
| Protocol and simulator | Host-buildable reference implementation; tests included |
| Firmware | ESP-IDF C reference sketches; hardware target build not run |
| Schematic | Pin/net reference tables; native KiCad capture pending |
| Backend | FastAPI demo API; MQTT adapter is a documented integration boundary |
| ML | No model: deterministic rules are more appropriate; collection/evaluation plan only |
| Mobile | React Native UI stub; not a deployable app |
| BOM | Candidate parts and indicative costs; sourcing/availability unverified |

## Architecture

```text
Surface sentinel (ESP32-C6 + wetness/temp) -- BLE GATT --> Site gateway (ESP32-S3)
Inspection beacon (nRF52840 + button + NFC) -- BLE GATT --> gateway
Gateway -- HTTPS/MQTT over TLS (deployment integration) --> FastAPI --> steward dashboard/app
```

Nodes are installed at a single court cluster. The gateway buffers observations during WAN loss and syncs later. BLE is star topology, not BLE Mesh; per-device allowlisting and authenticated gateway provisioning are deployment requirements. The demo simulator exercises the common JSON contract without radios.

## Hardware and electrical reference

All nodes are SELV, USB/regulated low-voltage powered; no mains switching or actuators. Example GPIO assignments are board-specific and require verification against the selected module pinout before PCB capture.

| Node | SoC / candidate components | Pin/interface allocation | Power / behavior |
|---|---|---|---|
| Site gateway | ESP32-S3-WROOM-1-N8, USB-C 5 V, status LED | GPIO18/19 USB D-/D+ (module board dependent); GPIO4 LED; BLE radio | 5 V input, 3.3 V logic; cache to flash when offline |
| Surface sentinel | ESP32-C6-MINI-1, SHT40, capacitive wetness electrode, ADS1115 | GPIO6/7 I2C SDA/SCL; ADS1115 A0 wetness; SHT40 0x44 | 3.3 V; sample every 5 min; report stale rather than zero |
| Inspection beacon | nRF52840 module, sealed momentary button, NFC tag | P0.11 button (pull-up); NFC-A onboard; BLE advertising/GATT | 3 V regulated; button event confirms a human inspection only |

Power estimates (prototype planning only): gateway 0.4 W average from USB; surface node 0.15 W average at five-minute interval; beacon under 1 mW average at 10-minute advertising interval, excluding button/NFC interactions. Enclosure must be UV-resistant, IP-rated by testing, mounted outside play surface. Sensor placement and wetness threshold require site calibration. No battery-life or ingress rating is guaranteed.

## Firmware, protocol, and demo

Wire data is versioned JSON: `version`, `site_id`, `node_id`, `seq`, `observed_at` (UTC RFC3339), `kind`, and `data`. Kinds: `surface_observation`, `inspection`, `maintenance_report`. Sequence numbers are monotonic per node; backend rejects malformed fields, deduplicates `(node_id,seq)`, and marks observations stale after 30 minutes. CRC is not an authentication mechanism; the demo assumes a trusted local gateway. Production deployments must use TLS, per-node credentials, replay protection, and role-based access.

Host simulator and tests:

```sh
python -m unittest discover -s software/dashboard/tests
python scripts/simulate_site.py
```

Firmware reference sources use ESP-IDF APIs and are not target-build verified. See `docs/protocol.md` and `schematic/README.md`.

## Software

FastAPI exposes `GET /health`, `GET /observations`, and `POST /observations`; in-memory storage is demo-only. MQTT ingestion, durable storage, authentication, and multi-site tenant isolation must be added before deployment. Mobile app is a visual stub and must label demo data clearly. ML folder documents a future evaluation dataset; initial triage uses transparent thresholds (surface wetness and age of observation), not a fabricated model.

## Cost / procurement

Indicative one-off estimates: gateway $15–25, surface node $12–20, beacon $8–18; total $35–63, excluding enclosure, installation, networking, and labor. BOMs list candidate MPNs, not procurement guarantees.

## Safety, privacy, limits

Advisory context only. No equipment is remotely controlled; never use this system to determine that a court is safe or open. Human inspection and existing maintenance procedures remain authoritative. No audio/video, personal identifiers, or visitor counts are collected. Site IDs should be coarse; retain raw telemetry no longer than locally configured operational need. Radio compliance and enclosure/weather validation are deployment responsibilities.

## Build and operations

Set site identity during commissioning, test sensor response against dry/wet reference samples, confirm timestamp and stale-data behavior, and physically inspect all reported conditions. `scripts/calibrate_surface.py` prints a guided calibration record; it does not write thresholds to devices. See `docs/architecture.md` and `docs/api.md`.

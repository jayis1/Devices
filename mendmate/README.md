# MendMate — household clothing repair companion

Status: engineering prototype; software-only validation. Hardware, RF, optical characterization, and textile recommendations have not been physically validated. Do not use this system as a sewing-machine safety device.

MendMate helps households turn a damaged garment into a manageable repair instead of discarding it. It combines a handheld optical swatch reader, a tool-and-thread station, and a local hub. The system records user-selected garment and damage details, offers explainable repair preparation, and tracks supplies; it does not diagnose fabric composition, guarantee repair suitability, or automate sewing. Unlike WasteSort (disposal sorting), MendMate supports repair and reuse before disposal.

## System

| Node | Prototype hardware | Role |
|---|---|---|
| Hub | Raspberry Pi Zero 2 W, Wi-Fi, local FastAPI service | Coordinates node reports, stores local repair cards, presents API |
| Swatch reader | ESP32-C3-MINI-1; AMS AS7341 11-channel spectral sensor over I²C; LED illumination | User-triggered reflectance sample with explicit user-entered fabric label; advisory color/reflectance feature only |
| Tool station | ESP32-C3-MINI-1; HX711 + 5 kg load cell under thread tray; reed switch on tool drawer | Reports coarse thread-stock estimate and drawer-open events; no person or garment identification |

Nodes use Wi-Fi/MQTT on a private LAN in this prototype. The hub is coordinator and local source of truth. MQTT topic and JSON contract are defined in `docs/protocol.md`; internet is not required. No actuation is present. The React Native screen is a UI stub.

## Data flow and operation

User opens a repair card and enters garment type, damage, and optional fabric label. A scan button collects a normalized spectral vector under fixed onboard illumination; the tool station reports tray weight and drawer state. The hub timestamps, validates, and retains the latest inventory and repair-card event locally. The API exposes only that deployment's records. Offline nodes show stale state; missing data is never interpreted as a successful scan or adequate supply. Event queue is bounded to 64 records; overflow drops oldest telemetry, never repair-card text. Prototype persistence is in-memory; restart loses state.

No raw images, audio, identity, or cloud upload are collected. Production deployment requires per-device MQTT credentials, TLS, broker ACLs, authenticated API access, retention/deletion policy, and signed firmware updates. Prototype has no authentication, commissioning, OTA, durable database, or multi-tenant boundary; keep it on a trusted isolated LAN and never expose it to the internet.

## Interfaces and power

Both ESP32-C3 nodes use 3.3 V logic and USB 5 V input with a regulated 3.3 V supply rated for radio peaks. Sensor reader: I²C SDA GPIO4, SCL GPIO5, active-low scan button GPIO3 with pull-up, status LED GPIO8 via series resistor. AS7341 uses 3.3 V and its documented I²C address; verify breakout pullups and exact module before connection. Tool station: HX711 DOUT GPIO6, SCK GPIO7; drawer reed input GPIO3 with pull-up. Load cell connects only to HX711 bridge inputs as labeled by its manufacturer. Confirm ESP32-C3 module pin availability/strapping and board mapping before PCB design. Raspberry Pi hub uses regulated 5 V USB supply. These are prototype pin assignments, not validated PCB layouts.

Power estimate (design target, not measured): each sensor node 5 V/1 A adapter, peak budget reserve 0.5 A for ESP32-C3 Wi-Fi bursts plus sensor/LED; hub 5 V/2 A supply. Avoid battery operation until peak-current, sleep-current, thermal and runtime tests are measured. Low-voltage only; no mains wiring.

## Build and verification

- Firmware: portable C11 shared packet codec and node-side measurement abstractions in `firmware/`; GPIO, Wi-Fi and MQTT adapters are integration boundaries, not complete ESP-IDF firmware images.
- Backend: FastAPI prototype, validated in-process event ingestion plus optional TLS-configured Paho MQTT subscriber (`mqtt_bridge.py`); deployment broker ACLs and runtime integration remain unverified.
- ML: synthetic-data training smoke test only. It does not establish material identification accuracy or recommend repairs.
- Mobile: React Native display stub, not device-tested.
- Schematics: `schematic/` contains wiring references, not native KiCad schematics or fabrication-ready design.

Run from this folder:

```sh
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/tests/test_protocol.c -o /tmp/mendmate-protocol && /tmp/mendmate-protocol
python3 -m unittest discover -s software/dashboard/tests -v
python3 -m py_compile software/dashboard/main.py software/ml-pipeline/train.py
python3 software/ml-pipeline/train.py
```

Physical sensor characterization, cross-node broker integration, authentication, native KiCad/ERC, ESP-IDF builds, and React Native device build remain unverified. See `docs/architecture.md`, `docs/protocol.md`, `docs/api.md`, and per-node BOM CSVs. Author: jayis1.

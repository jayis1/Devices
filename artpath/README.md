# ArtPath — accessible neighborhood public-art interpretation

ArtPath helps parks departments and community arts groups make outdoor sculpture trails usable by visitors who cannot reliably read small plaques or navigate phone-dependent tours. A local gateway serves short, curator-approved descriptions through a tactile NFC marker, a low-power audio endpoint, and an optional directional beacon. It measures equipment readiness, not visitors: no cameras, microphones, location histories, accounts, or analytics about people.

Novelty: this is the repository's first community cultural-access interpretation system. It differs from accessibility navigation projects (GuideSync, TactileSync) because it does not route or locate a person; its nodes make a specific artwork's authored content accessible at the artwork itself.

## System and data flow

```text
NFC/large-button artwork node ─┐
Audio beacon node ──────────────┼─ BLE to local ArtPath gateway ─ Wi-Fi (optional) ─ FastAPI
Weather/context node ──────────┘          │ local cache                     │ curator console/app
                                           └─ offline playback                └─ optional sync
```

MVP: one gateway and two artwork nodes, expandable to multiple nodes; the weather node is optional context for outdoor enclosure maintenance, never visitor sensing. Visitors tap NFC or use a large physical button; the node fetches a signed/content-versioned item from the local gateway and offers accessible text/audio. The reference implementation describes a prototype, not a finished public installation.

## Hardware and interfaces

| Node | Prototype parts | Interfaces / role | Supply |
|---|---|---|---|
| Gateway | Raspberry Pi Zero 2 W; USB-C 5 V supply; status LED | Wi-Fi; BLE 5; local SQLite/cache; API | regulated 5 V, 2 A supply |
| Artwork station ×N | ESP32-C3-MINI-1; PN532 NFC module; large normally-open button; MAX98357A I²S amp; 8 Ω speaker; 3.3 V regulator | I²C NFC (0x24 default PN532 I²C address; verify module straps), GPIO button, I²S audio, BLE | 5 V SELV USB, no battery in MVP |
| Context node (optional) | ESP32-C3-MINI-1; SHT40; protected enclosure | I²C 0x44; BLE environmental maintenance telemetry only | 5 V SELV USB |

Pins are starting assignments for the ESP32-C3 module carrier, not a validated PCB design. GPIO assignments must be verified against the exact carrier and board package before fabrication. Common station mapping: GPIO4 SDA, GPIO5 SCL, GPIO2 button (internal pull-up, switch to GND), GPIO6 BCLK, GPIO7 LRCLK, GPIO8 DOUT. Separate I²C bus share is safe for PN532 and SHT40 (addresses differ). Expose 3V3, GND, UART0 TX/RX and EN/BOOT for debug; keep antenna clearance per module datasheet. Exact PN532 breakout logic-level compatibility and amplifier gain require bench validation.

### Power and safety

Station's 5 V input is low-voltage SELV; use a listed external supply, fused input and reverse-polarity protection on any custom carrier. Design estimate: ESP32-C3 radio bursts 250 mA peak, audio amplifier up to 500 mA peak at loud output, PN532 50 mA peak; size supply and wiring for at least 1 A/node with decoupling. No battery-life claim: continuously available audio and radio make battery operation inappropriate. Set volume limit in software and provide an accessible physical stop. Outdoor enclosures, drainage, UV, vandalism, ingress, and mounting need site-specific review. No mains wiring, emergency guidance, or safety claims.

## Protocol and content

See `docs/protocol.md`. BLE service uses versioned UTF-8 JSON messages capped at 512 bytes; content payloads are fetched in bounded chunks from the gateway. Node identity is a provisioned random identifier, not a person identifier. Duplicate event IDs are idempotent. BLE is local and optional; content remains available from station flash during network outages. HTTPS sync is disabled until an operator configures TLS and authentication. No default credentials or remote actuator commands exist. Content must carry curator approval, language, plain-text transcript, audio asset checksum, license/credit and revision; never synthesize an unreviewed description as historical fact.

## Software

- `firmware/common`: portable framing and bounded validation C code; host test available.
- `firmware/artwork_station`, `firmware/context_node`: hardware integration skeletons with pin constants; not target-built or flashed.
- `software/dashboard`: FastAPI prototype with local content/readiness API and SQLite; cloud sync/MQTT is intentionally not connected in this local-first MVP.
- `software/ml-pipeline`: reproducible synthetic fixture exercise only. V1 uses explicit curator-selected tags and deterministic accessibility checks; no model makes interpretive or safety decisions. No real dataset or accuracy claim.
- `software/mobile-app`: React Native concept stub; not built/device-tested. MVP content access is web/API and physical station, so the app is optional.

## Build / verification

Run `scripts/test.sh` (host C test, Python unit tests, syntax checks, synthetic fixture generation). Backend run: `python3 -m pip install -r software/dashboard/requirements.txt`, then `uvicorn app:app --app-dir software/dashboard --host 127.0.0.1 --port 8000`. Bind to localhost unless an operator configures TLS, authentication, and network access controls. The app is a scaffold and requires React Native toolchain/dependencies before use.

## Readiness and limits

| Artifact | Status |
|---|---|
| Architecture, content schema, pin/net schedule, candidate BOMs | Designed; validate exact revisions/datasheets before purchasing |
| Native KiCad electrical schematics / PCB | Not included; `schematic/*/README.md` are explicit net schedules, not KiCad files |
| C firmware | Portable protocol tested on host; target radio/NFC/audio drivers are skeletons, unbuilt |
| Backend | Local prototype and unit tests; no deployed/cloud service, MQTT, authentication, or multi-tenant support |
| ML | Synthetic pipeline smoke test; no production model or real-world claims |
| Mobile app | Scaffold only, not built |
| Physical installation/accessibility evaluation | Not performed; test with local disabled visitors and access consultants before deployment |

Accessibility content should include concise plain language, screen-reader-ready text, transcripts, language selection, and tactile/large-button controls. Keep audio optional, avoid blocking paths, do not imply that a beacon guarantees navigation, and provide a non-digital interpretation fallback. Treat content as public and obtain rights/consent for recordings. No visitor telemetry is collected. Prototype is not a certified assistive device.

Invented and maintained by jayis1.

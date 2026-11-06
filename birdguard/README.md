# BirdGuard — window-collision prevention and evidence

Status: engineering prototype. BirdGuard helps homes and small buildings identify high-risk glass and apply visible, externally mounted deterrent markers. It does not claim to prevent every collision, identify species, or replace established bird-safe glazing. No adhesive, actuator, or powered device is fitted to the glass by this prototype.

## Need and design

Bird collisions with transparent or reflective glazing are a preventable source of wildlife injury. BirdGuard combines an outdoor weather/light node, per-window reflection/marker inspection nodes, and an indoor gateway. It reports site-level conditions and maintenance reminders; no camera, microphone, animal capture, or automatic window control is used. Compared with the repository's plant, home-safety, and community-readiness systems, its specific job is glass-risk inspection and retrofit upkeep.

Nodes: (1) Gateway: Raspberry Pi Zero 2 W, Wi-Fi, local MQTT broker/API and SQLite; 5 V USB supply. (2) Window Sentinel: ESP32-C3-MINI-1, OPT3001 ambient-light sensor, reed switch for marker-panel presence, BLE 5 advertising to gateway; USB 5 V, 3.3 V regulated. Place indoors, facing the window but shield its light sensor from direct sun. It detects missing/changed removable external marker panels, not collisions. (3) Weather Sentinel: ESP32-C3-MINI-1, BME280 temperature/humidity/pressure and VEML7700 lux, BLE 5 advertisements; sheltered outdoor enclosure, 5 V USB supply. Use one sentinel per monitored window plus weather node.

BLE advertisements carry compact, versioned, non-sensitive observations; gateway accepts only provisioned device IDs and freshness-bounds readings. Prototype transport is not authenticated encryption: do not deploy across untrusted radio environments or treat observations as tamper-proof. Production design must use BLE Secure Connections/GATT with per-device credentials, rotating keys, replay counters, authenticated gateway enrollment, and tested OTA rollback. Local operation remains available without internet; no remote actuation exists.

## Architecture and data flow

Sensors → BLE advertising observations → gateway validation and local SQLite history → FastAPI read-only status API → React Native status screen. Optional MQTT forwarding is disabled by default and requires operator-configured TLS broker credentials. Raw environmental observations are retained locally for 30 days; no household identity or location is required. User deletes history by removing the local database. Cloud forwarding, if enabled by a future deployment, must use consent and an explicit retention policy.

### Power and interfaces

Both ESP32-C3 nodes use regulated 3.3 V rails from USB 5 V buck/LDO modules; budget at least 500 mA supply per node for radio peaks. Sensor bus is I²C at 3.3 V, 100 kHz: SDA GPIO4, SCL GPIO5, 4.7 kΩ pullups to 3V3. OPT3001 address 0x44; BME280 0x76 (SDO low); VEML7700 0x10. Reed switch is GPIO6 to ground with internal pull-up, closed means panel present. USB-C is power-only; expose UART0 GPIO20/21 and EN/BOOT pads for programming. These pin choices are a reference for ESP32-C3-MINI-1 module carrier, not verified against a fabricated PCB. Use ESD protection at external connectors and a fused 5 V input; no battery or charger is included.

## Build and validation

The repository includes BOMs, pin-level connection notes, platform-neutral C observation codec, FastAPI/SQLite prototype, deterministic risk-scoring baseline, and mobile display stub. This is not production firmware: radio drivers, provisioning, authenticated link, OTA, and physical integration remain unimplemented. Schematics are documented connection schedules rather than KiCad CAD projects; no ERC, PCB, RF, EMC, environmental, or field validation is claimed.

Backend: `cd software/dashboard && python3 -m unittest discover -s tests` then `python3 -m pip install -r requirements.txt` and `uvicorn app:app --host 127.0.0.1`. API `GET /health`, `POST /v1/observations`, `GET /v1/status`; payload contract is in `docs/protocol.md`. The prototype API validates schema and timestamps but has no device authentication; bind locally and do not expose it to a network.

ML smoke test: `python3 software/ml-pipeline/score.py --input software/ml-pipeline/sample.csv`. The score is a transparent prioritization heuristic, not a trained model or ecological risk estimate. Mobile source is an Expo scaffold; install/build has not been verified here.

## Failure and safety limits

Sensor missing/stale/invalid data is displayed as unknown, never safe. Gateway outage loses live status; nodes do not control windows or lighting. Inspect glass and marker adhesion from a safe position; do not work at height based on this device. Follow local wildlife guidance and use tested bird-safe retrofit products. Do not modify glazing or building envelope without owner approval. Weather sensors require periodic comparison/calibration; marker detection is only panel-presence context. No emergency, collision-detection, or regulatory-compliance function.

See `docs/architecture.md`, `docs/protocol.md`, schematic schedules, and per-node BOMs. All hardware is preliminary; verify datasheets, module pinout, electrical protection, enclosure rating, radio rules, and mounting before construction. Invented and maintained by jayis1.

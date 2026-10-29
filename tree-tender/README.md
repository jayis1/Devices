# TreeTender — neighborhood street-tree care coordination

Status: concept / software prototype. Firmware and hardware are not yet build- or hardware-validated. This is a volunteer coordination aid, not an irrigation controller, arborist diagnosis, or municipal safety system.

## Problem and users

Volunteer street-tree stewards and municipal forestry teams often cannot tell which newly planted trees need a visit, who has taken responsibility, or whether a watering visit actually happened. TreeTender turns coarse root-zone observations and weather context into a prioritized, human-confirmed task list. A steward receives a weekly route rather than being asked to inspect every tree.

Unlike household plant-care products, TreeTender coordinates many public trees, volunteer assignments, and staff-confirmed visits across a neighborhood. It deliberately does not identify people, automatically dispense water, infer tree disease, or claim a tree is safe. Soil readings are advisory; a human checks the tree and local watering guidance.

## System

```text
Root-zone probe nodes --BLE--> Site Gateway --MQTT/TLS--> FastAPI + SQLite
Weather node ----------BLE--/                            | REST
Volunteer check-in app -----------------------------------+
                         local queue if gateway/internet is down
```

| Node | Role / components | Power and link |
|---|---|---|
| Site Gateway | ESP32-S3-WROOM-1, Wi-Fi, BLE, local queue, status LED/button | USB-C 5 V; Wi-Fi upstream, BLE local |
| Root-zone node ×3+ | nRF52840 module, SHT40 air temp/RH, capacitive soil probe analog input, sealed enclosure | 2×AA; BLE advertisement, sampled every 30 min |
| Weather node | nRF52840 module, SHT40, BMP390 pressure, tipping-bucket reed input | 2×AA; BLE advertisement |

The three node roles are intentional: one gateway gives a site-local queue and uplink; distributed root probes detect uneven conditions; a separate exposed weather node gives rain context without treating a sheltered soil probe as a rain gauge. No pumps/valves are included.

## Protocol and failure behavior

BLE advertisements carry compact versioned JSON-equivalent CBOR records: `{v:1,node,seq,ts,kind,soil_pct,temp_c,rh_pct,rain_mm,crc16}`; fields not applicable to a node are omitted. Values are engineering-unit observations; `ts` is UTC epoch seconds when available. Gateway rejects unknown versions, malformed ranges and duplicate `(node,seq)` records; sequence wrap is modulo 2^16 and stale samples are marked, not silently treated as current. The prototype transport is unauthenticated BLE advertising: do not deploy sensitive data; production provisioning, authenticated encryption, key rotation, and replay protection are required before public use.

Gateway caches at least 7 days of bounded observations locally and retries MQTT with exponential backoff. With no internet, local collection and queued steward check-ins continue; no cloud-dependent control exists. Missing nodes become visibly stale after two expected sample intervals. Sensor disconnects report a fault rather than zero moisture. Rainfall and soil thresholds are site-calibrated and never automatically trigger watering. MQTT uses TLS and credentials via environment variables; demo compose is local development only.

## Hardware and power notes

3.3 V logic throughout; nRF52840 ADC is used only within its supply range. Soil probe analog output is limited to 0–3.0 V before the ADC; verify chosen probe output before connection. SHT40 is I²C (3.3 V); BMP390 is I²C on weather node. Suggested prototype pin mapping and connector details are in `docs/pinout.md`. Add reverse-polarity protection, battery protection, and an input RC filter in the board design. No charging circuit: replace AA cells. Estimated node consumption is a design target, not a measurement: 3 mA active for 20 ms per 1800 s plus 8 µA sleep gives ~41 µA average; with 2000 mAh ideal capacity the arithmetic estimate is ~5.5 years before self-discharge, cold derating, and radio retries. Use a conservative 12–18 month service target and validate by current measurement. Gateway requires continuous USB power.

## Data, privacy, and retention

No audio, video, resident identity, or precise household data. Records identify a site-assigned tree node, not a person. Volunteer names/contact details are optional account data and separate from sensor telemetry. Default prototype retention is 90 days; operators can export/delete site data. Obtain site and participant consent before deployment.

## BOM estimate

Indicative small-quantity USD estimates; verify supplier and exact module/board before purchase. BOM electronics sum to approximately $79 for three probe nodes, one weather node, and one gateway; weatherproof enclosures, protection/passives, assembly, labor, and installation add an unquoted estimated $40–$110, so planning total is approximately $120–$190 excluding labor and installation. Per-node BOMs are in `hardware/bom/`; values are estimates, not quotes.

## Software and build

- Firmware: portable C sensor/protocol examples in `firmware/`; target integration requires Nordic nRF Connect SDK for sensor nodes and ESP-IDF for gateway.
- Backend: FastAPI API + SQLite and an optional TLS Paho MQTT bridge in `software/dashboard/`. Run `pip install -r requirements.txt`, set `MQTT_HOST` / `MQTT_USERNAME` / `MQTT_PASSWORD` / `TREE_TENDER_SITE`, then start `uvicorn main:app` and separately `python mqtt_bridge.py` from that directory. The unauthenticated prototype API must remain on localhost/trusted test networks; broker-to-API integration is not yet exercised.
- ML: synthetic-data generator and baseline urgency model in `software/ml-pipeline/`; synthetic data is not field evidence and predictions are advisory.
- Mobile app: React Native interface contract stub in `software/mobile-app/`; not yet a runnable app.
- Schematics: `schematic/` contains inspectable netlist-level starting points, not KiCad manufacturing schematics.

Run `python -m unittest discover -s software/dashboard/tests` from repo root. The tests exercise the API and validation without broker/hardware. See `docs/architecture.md`, `docs/protocol.md`, `docs/api.md`, and `docs/pinout.md`.

## Safety and limitations

Do not use this prototype as proof of tree health, watering adequacy, sidewalk safety, or regulatory compliance. Human inspection is required; follow local arborist/municipal instructions. No automated water, mains, or machinery control. Sensor drift, soil composition, placement, battery temperature, and BLE interference can invalidate readings. Firmware SDK builds, RF range, battery endurance, outdoor ingress protection, and physical calibration remain unverified.

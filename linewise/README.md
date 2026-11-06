# LineWise — shared outdoor laundry weather & line-readiness system

Status: prototype reference design. No PCB fabrication, weatherproofing, radio range, or field validation is claimed. LineWise helps apartment courtyards and shared housing decide whether a communal clothesline is usable and gives residents a timely rain/wind warning. It does not track residents, identify garments, or control a clothesline.

## System

Three nodes: (1) Site Gateway, ESP32-C6, receives signed-format telemetry over BLE advertisements, stores bounded local history, and forwards MQTT when online; (2) Line Sentinel, nRF52840, measures line tension with a load cell and HX711 plus local ambient temperature/RH, reporting occupied/clear as a coarse state; (3) Weather Sentinel, ESP32-C3, measures rain with a tipping-bucket gauge and wind with a reed-switch anemometer. Sensor nodes continue sampling during internet outage; gateway buffers at most 256 readings and drops oldest telemetry, never commands. The phone app shows status and configurable alerts.

## Data flow and interfaces

Weather and line nodes advertise a compact v1 record over BLE 5.0 advertisements; gateway validates length, node ID, sequence, units and CRC-16/CCITT-FALSE before forwarding. Advertisement data is not confidential or authenticated: deployment is limited to a physically controlled site, node allow-listing is not cryptographic identity, and fabricated packets remain a known risk. Do not use this prototype for safety decisions. MQTT topic: `linewise/v1/sites/{site_id}/nodes/{node_id}/telemetry`; JSON fields are `v` (1), `node_id`, `seq` (uint32), `ts` (UTC epoch seconds or null), `kind`, `value`, `unit`, `rssi_dbm`. Broker TLS and per-site credentials required in deployment; no anonymous broker configuration supplied. Backend rejects malformed payloads and exposes read-only status API.

The useful first vertical slice is the sensor sample → protocol encoding/decoding → gateway aggregation → API status response. The C protocol library is host-testable. Firmware peripheral/radio drivers are explicitly board-port integration points and are not represented as complete ESP-IDF/Zephyr projects.

## Hardware and power

All nodes use regulated 3.3 V logic. Gateway: USB-C 5 V input, protected 3.3 V regulator, ESP32-C6 module. Line Sentinel: 2×AA alkaline pack to low-quiescent 3.3 V buck/boost, nRF52840 module, HX711/load cell; calibrate tare and span at installation. Weather Sentinel: 5 V USB supply, ESP32-C3 module, rain gauge and reed anemometer contacts pulled up to 3.3 V. Sensor inputs are low-voltage only; keep outdoor installation SELV, use enclosure glands, drip loops, surge protection, and local electrical codes. Battery runtime and outdoor ingress rating are unvalidated.

## Pin assignments (reference, verify selected board variant)

| Node | MCU pin | Signal | Notes |
|---|---|---|---|
| Gateway | GPIO4 | I2C SDA | Optional display; 3.3 V |
| Gateway | GPIO5 | I2C SCL | 4.7 kΩ pullups |
| Line Sentinel | P0.26 / P0.27 | HX711 DOUT / SCK | HX711 powered at 3.3 V; load-cell excitation per selected cell |
| Weather Sentinel | GPIO2 | Rain reed input | Pull-up; interrupt/count with debounce |
| Weather Sentinel | GPIO3 | Wind reed input | Pull-up; interrupt/count with debounce |

Pin mapping is a proposed module-level allocation, not a routed PCB or verified vendor dev-board mapping. Verify boot pins, antenna keep-out, decoupling, ESD and input limits before fabrication.

## Firmware

`firmware/common/protocol.h` defines a bounded, versioned record and CRC helper. `firmware/common/test_protocol.c` exercises round-trip, malformed length and CRC rejection. `firmware/line-sentinel/main.c` and `firmware/weather-sentinel/main.c` show sampling/state logic with explicit hardware adapter boundaries; production radio drivers, authenticated provisioning, secure boot and OTA are not implemented. `firmware/gateway/main.c` demonstrates receive/validate/queue behavior.

## Software

`software/dashboard/` is a FastAPI read-only API and MQTT ingestion adapter. MQTT credentials and TLS are deployment configuration, never committed. `software/ml-pipeline/` trains a reproducible logistic baseline on generated, explicitly synthetic records to classify line-useful conditions; this is a demo only, not a validated weather or drying-time predictor. The React Native app in `software/mobile-app/` is a labeled UI stub showing status and alert preferences; pairing and push notifications are not implemented.

## Failure behavior, privacy, and limits

On sensor failure the node reports an invalid/unknown state, never clear. Internet failure leaves local sampling operational and bounds queue size. Gateway failure removes cloud visibility; residents use ordinary observation. Rain/wind thresholds are user-configurable advisory values and must not be interpreted as a guarantee that laundry stays dry. No cameras, microphones, user identifiers, or garment-level data. Retain raw telemetry only as long as a site operator configures; provide deletion at the broker/database layer. Physical inspection remains authoritative.

## Build and checks

Run `python3 -m unittest discover -s software/dashboard -v`, `python3 firmware/common/test_protocol.py` and `python3 software/ml-pipeline/train.py --help`. Build targets require ESP-IDF and Zephyr board projects, which are not included in this prototype. Schematics are pin/interface design records under `schematic/`, not KiCad ERC-checked designs. BOMs under `hardware/bom/` identify representative orderable parts; substitutions require electrical review.

## Repository layout

- `schematic/`: per-node connection and power design notes.
- `firmware/`: C node examples and common protocol.
- `hardware/bom/`: per-node CSV BOM.
- `software/dashboard/`: API and tests.
- `software/ml-pipeline/`: synthetic-data baseline.
- `software/mobile-app/`: React Native stub.
- `docs/`: architecture, protocol, API details.
- `scripts/`: local validation helper.

Invented and maintained by jayis1. MIT license; prototype only.

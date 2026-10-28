# ParcelPause

**Status: design concept; hardware, RF performance, and regulatory compliance are not validated.**

ParcelPause helps residents of small apartment buildings coordinate shared package-room capacity without identifying recipients or tracking people. A doorway sensor estimates room occupancy; a shelf scale detects when shared shelves are near capacity; a local hub shows an occupancy band and relays optional alerts. It does not inspect packages, identify residents, unlock doors, or guarantee theft prevention.

## Why this is distinct

Existing systems in this repository cover porch delivery security, shared laundry, tool libraries, pantry inventory, and public-service queues. ParcelPause addresses the building manager's recurring operational problem of an overflowing shared package room. It uses anonymous room-state sensing, not cameras, recipient tracking, or machine availability.

## System overview

```
Doorway ToF node ─┐
                  ├─ BLE advertisements ─ Parcel hub ─ Wi-Fi/MQTT ─ API ─ React Native
Shelf scale node ─┘                         └─ local display + bounded event queue
```

The hub continues to show the most recent estimates during internet loss. Sensors do not control locks or doors. A gap in telemetry becomes `unknown`, never an inferred empty room.

## Nodes

| Node | SoC / parts | Role | Power |
|---|---|---|---|
| Hub | ESP32-S3-WROOM-1, SSD1306 OLED | Aggregates measurements, serves local status, publishes MQTT | USB-C 5 V; regulated 3.3 V |
| Doorway sensor | nRF52840 module, VL53L1X ToF | Anonymous directional passage estimates; no image/audio | 2×AA with protected 3.3 V regulator (prototype assumption) |
| Shelf node | nRF52840 module, NAU7802 + load cell | Reports aggregate shelf load band | USB 5 V; regulated 3.3 V |

### Communication and protocol

Prototype transport: BLE 5 advertisements to hub; Wi-Fi/TLS MQTT from hub to broker. `firmware/common/protocol.h` is the fixed 20-byte little-endian payload contract. Version, node type, sequence, sample age, status, and quantized reading are bounded; CRC-16 detects accidental corruption only and is not authentication. Hub uplink requires TLS and broker credentials supplied out-of-band. Per-device provisioning, broker ACL configuration, and signed OTA are deployment requirements, not implemented here. See `docs/protocol.md`.

## Power, wiring, and bill of materials

Pin/net plans are in `schematic/*/netlist.md`; these are **not KiCad schematics**. All logic is 3.3 V. Validate module pin mux, pull-ups, regulator transient response, enclosure, strain relief, and RF placement against the selected board before fabrication. The doorway node's range-based sensing must be installed so the beam does not cover public space beyond the room. No mains wiring or actuators.

BOMs and rough nonbinding unit-price estimates: `hardware/bom/`. Estimated prototype total: about USD 90–160 depending on display, load cell, and enclosure. Estimates and availability have not been checked with suppliers. Battery life is not claimed; measure duty cycle and radio retries on the final assembly before deployment.

## Build and run

1. Assemble low-voltage prototypes from the netlists and BOMs; verify polarity and current-limited supply before connecting batteries.
2. Build firmware using the board vendor's SDK integration (board-specific HAL calls are intentionally isolated; host protocol test is provided).
3. Run backend locally with `pip install -r software/dashboard/requirements.txt` and `uvicorn main:app --app-dir software/dashboard`.
4. Configure MQTT URL, username, and password via environment variables; no credentials are included. MQTT subscriber integration is a deployment stub; REST ingest is executable locally.
5. Run `python3 software/ml-pipeline/train.py` for a synthetic-data smoke test only. The generated model is not evidence of real-world capacity prediction.
6. Mobile project is an interface stub; see its README.

## Safety, privacy, and failure behavior

No camera, microphone, occupant identity, or package contents are collected. Use only aggregate count/load bands; configure short retention and provide deletion controls in any production deployment. Stale sensor data is marked unknown; duplicates are discarded by node/sequence; radio and broker outages do not trigger door actions. Sensor misalignment, load drift, deliberate interference, and occlusion can produce incorrect estimates. The system is advisory and must not be used for emergency, security, or access-control decisions.

## Validation and limitations

Host protocol codec and Python backend tests are included. Hardware RF range, ToF placement, load-cell calibration, battery life, KiCad ERC, mobile build, broker TLS/ACL, and deployment security have not been validated. This is an engineering prototype, not a finished or certified product.

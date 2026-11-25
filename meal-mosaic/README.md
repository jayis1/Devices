# MealMosaic — shared-meal coordination without diner tracking

MealMosaic coordinates a community meal site's serving line, reusable-container returns, and kitchen context using three low-cost nodes and explicit volunteer check-ins. It helps a volunteer lead answer: are portions ready, is the return station backing up, and is the room comfortable enough to continue? It does not track diners, measure food safety, control appliances, or make allergen claims.

## Novelty and scope

This is a shared-meal service workflow, distinct from CookShare (shared cooking-station readiness), ShareTable (serving observations and container handoff), and DishLoop (rack/return flow). MealMosaic focuses on the transition from batch staging to a staffed serving line: aggregate tray counts, return-bin capacity, and room context are combined into a single volunteer checklist. The pilot outcome is time spent reconciling station state and number of missed restock/collection prompts—not food safety or service quality.

## Architecture

```text
Serving Node (tray count + button) ─┐
Return Node (bin load + button) ─────┼─ BLE advertisements → Gateway (ESP32-C3)
Room Node (temp/RH + status button) ─┘                         │ Wi-Fi / HTTPS
                                                     FastAPI + SQLite
                                                     React Native operator stub
```

Three battery/USB low-voltage observer nodes report to a continuously powered gateway. Prototype radio is BLE 5.x advertising in a star-like receive model, not BLE Mesh. The gateway assigns receipt time, rejects malformed/duplicate frames, and stores the latest event locally. Cloud/API sync is optional; this repository's gateway radio driver is not implemented. A volunteer confirms serving/return events physically; there is no camera, microphone, identity sensor, or automated appliance control.

| Node | Candidate parts | Interface/power |
|---|---|---|
| Serving Node | ESP32-C3-MINI-1, 4× load cells + HX711, illuminated pushbutton | HX711 DOUT/SCK GPIO4/5; button GPIO6; USB 5 V to module regulator |
| Return Node | ESP32-C3-MINI-1, 5 kg load cell + HX711, pushbutton | HX711 GPIO4/5; button GPIO6; USB 5 V |
| Room Node | ESP32-C3-MINI-1, SHT31-D, pushbutton | I²C SDA GPIO4/SCL GPIO5 (0x44); button GPIO6; USB 5 V |
| Gateway | ESP32-C3-DevKitM-1, Wi-Fi + BLE | USB 5 V; BLE scanner and local API bridge are prototype responsibilities |

Pin schedules are illustrative and must be checked against the exact module/board revision and SDK before fabrication. All sensor-side logic is 3.3 V; never apply 5 V logic to GPIO. Load-cell values indicate relative mass only until calibrated with known weights. Avoid placing food or containers on electronics; use sealed, cleanable fixtures.

## Protocol and behavior

`firmware/common/protocol.h` defines a fixed 20-byte v1 frame: magic, version, node kind, 32-bit node ID, 32-bit sequence, 32-bit uptime milliseconds, signed 16-bit value, status byte, and CRC-8/ATM. Values are centi-degrees C for room temperature, basis points RH for room humidity (one field per frame), and grams for mass nodes. Buttons produce status events. CRC is for accidental corruption only—not authentication. BLE advertising payload carries this frame; deployment must add authenticated enrollment and encrypted application transport.

Nodes sample no faster than once per 10 seconds and advertise updates; gateway marks observations stale after 10 minutes. Sequence duplicates are ignored. Missing data is `unknown`, never interpreted as zero stock or a completed task. When Wi-Fi is unavailable, the gateway should retain a bounded local queue; that persistence is not implemented in this prototype. No remote actuator command path exists. Site staff can override all prompts and must follow their established food-handling procedures.

## Software and ML

The FastAPI prototype accepts validated observations and serves latest status. It uses SQLite for a single-site local demonstration; authentication, multi-tenant isolation, MQTT, TLS deployment, retention/deletion workflows, and durable outage queueing are not implemented. The React Native screen is an operator-facing stub. ML is intentionally omitted: explicit check-ins, threshold bands, and simple stale-data rules are explainable and sufficient for a pilot. No model accuracy or food-safety inference is claimed.

## Files and verification

- `firmware/`: portable frame codec and unit tests, plus per-node C skeletons.
- `hardware/bom/`: per-node CSV candidate BOMs.
- `schematic/`: pin/net reference schedules; these are not KiCad schematics and are not fabrication-ready.
- `software/dashboard/`: FastAPI prototype and tests.
- `software/mobile-app/`: React Native display stub.
- `software/ml-pipeline/`: synthetic fixture generator only; no trained model.
- `docs/`: architecture, protocol, API and assembly notes.

Run from repository root:

```sh
gcc -std=c11 -Wall -Wextra -Werror -Imeal-mosaic/firmware/common meal-mosaic/firmware/common/protocol.c meal-mosaic/firmware/common/test_protocol.c -o /tmp/meal-mosaic-test && /tmp/meal-mosaic-test
python3 -m unittest discover -s meal-mosaic/software/dashboard/tests
python3 -m py_compile meal-mosaic/software/dashboard/app.py meal-mosaic/software/ml-pipeline/generate_fixture.py
```

Prototype readiness: protocol codec and host tests are implemented; backend and tests are a local prototype; node hardware drivers, BLE integration, persistent gateway queue, deployment security, physical calibration, KiCad schematics, and mobile device testing remain unverified/not implemented. BOM costs are planning estimates and availability is not checked. Verify all component datasheets, footprints, supply limits, and local RF requirements before purchase or deployment.

## Privacy, safety, and limits

No people are sensed or identified. Store only site/node IDs, measurements, sequence numbers, and timestamps; configure retention and access controls before deployment. Do not use these observations to establish food temperature, hygiene, allergen safety, or compliance. Nodes are low-voltage prototypes; keep electronics outside wash zones and use appropriate food-contact-safe fixtures. This is an advisory coordination aid, not a certified product.

Invented and maintained by jayis1.

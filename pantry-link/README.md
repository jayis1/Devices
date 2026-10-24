# PantryLink

PantryLink is a local-first stock and cold-chain coordination prototype for neighborhood food pantries and community fridges. It helps volunteer coordinators answer two routine questions before someone makes a trip: what is actually available, and is the chilled stock still within its recorded safe-temperature limits? It is not a food-safety certification system and never declares food safe to eat.

## Why this system

A volunteer-led pantry typically updates a shared spreadsheet or paper board between deliveries, pickup windows, and refrigerator checks. That information becomes stale precisely when volunteers are busiest; visitors may travel for unavailable staples, while unopened food sits undiscovered at another site. A multi-node design is justified because stock, temperature, and receiving operations occur at physically separate shelves, fridges, and intake areas. A hub at each site keeps working through internet outages; optional site-to-site synchronization shares only aggregate item counts and temperature events. The “I want that” moment: a volunteer can see, before a pickup run, that the nearby pantry has the requested staple and that its refrigerator had no unacknowledged excursion.

## System and nodes

```text
Shelf Scale Nodes (BLE) ─┐
Fridge Sentinel (BLE) ───┼── Site Hub (ESP32-S3; local queue + display)
Intake Dock (BLE) ──────┘             │ Wi-Fi / MQTT over TLS (optional)
                                      ├── FastAPI site service
                                      ├── Synthetic-data stockout trend demo
                                      └── React Native volunteer view
```

| Node | SoC / radio | Role | Power (design estimate) |
|---|---|---|---|
| Site Hub | ESP32-S3-WROOM-1; Wi-Fi + BLE 5 | Coordinates nodes, local status, bounded offline queue | Certified 5 V USB supply, <1.5 W typical |
| Shelf Scale (2+) | nRF52840 module; BLE 5 | HX711 + load cell reports calibrated mass changes; no person sensing | 2×AA, target 6 months at 1-min sampling (estimate; validate current) |
| Fridge Sentinel | nRF52840 module; BLE 5 | DS18B20 temperature probe, door reed switch, local buzzer | 2×AA, target 6 months estimate |
| Intake Dock | ESP32-C3 module; BLE + Wi-Fi disabled in normal mode | Barcode/manual item receipt, scale verification, volunteer action logging | Certified 5 V USB supply, <1 W typical |

BLE connections use authenticated bonding; enrollment requires a physical button press at the hub and a one-time local pairing window. No camera, microphone, face, visitor, or household-level tracking. Each site is a separate tenant. The prototype contains no real-account service or production key provisioning.

## What volunteers see

The app has four simple views: site status with last-seen age; searchable availability by item category and broad quantity band; fridge history with excursion acknowledgement; and intake/stock-adjustment actions with a reason and volunteer confirmation. Large text and non-color-only status are requirements. Published availability is deliberately coarse (e.g. “available / low / unavailable”), not a promise that an item is reserved.

## Protocol and behavior

BLE GATT carries compact versioned records. Each telemetry record has protocol version, node ID, monotonically increasing boot-scoped sequence, UTC timestamp when available, message kind, typed values, and CRC-16/CCITT-FALSE. The gateway validates sizes, ranges, version, node enrollment, and duplicate sequence IDs before storage. HTTPS/MQTT are optional uplinks; MQTT topic pattern is `pantry/v1/{site_id}/{node_id}/telemetry`, QoS 1, TLS, with per-device publish ACLs. No retained actuator command topics exist. Retry uses bounded exponential backoff with jitter; samples are capped at 24 hours locally and old readings are marked stale, never presented as current. BLE encryption depends on device bonding; the CRC detects accidental corruption and is not authentication.

## Software and ML scope

FastAPI provides health, current site inventory, and an authenticated-by-deployment-boundary telemetry ingest example. The in-memory reference service is for local development only; multi-tenant authorization, persistent database, user identity, MQTT TLS broker, credential rotation, and deletion workflow must be supplied before deployment. The training demo generates synthetic item-count histories and compares a seasonal-naive baseline with a small random-forest stockout classifier. It is not trained on pantry or beneficiary data and must not make allocation decisions. Mobile source is an interface scaffold, not a release build.

## Build, bring-up, calibration

1. Assemble only SELV low-voltage boards; review each node netlist and pin table before connecting hardware.
2. Build shared portable protocol code: `gcc -std=c11 -Wall -Wextra -Werror -Ifirmware/common -c firmware/common/protocol.c`.
3. Vendor SDK projects, radio drivers, secure boot, signed OTA, and board commissioning are not included; node `main.c` files illustrate the state machine, not flashable images.
4. Start the local API with dependencies in `software/dashboard/requirements.txt`, then run its tests from that directory with `python -m pytest`.
5. Generate synthetic training data and run the demo: `python software/ml-pipeline/train.py --synthetic`.
6. Pair each sensor during the hub's physical enrollment window. Verify its printed node ID and heartbeat before mounting.
7. Calibrate each shelf with a known 1 kg reference mass and empty tare; repeat after moving the load cell. Compare fridge probe against a traceable thermometer in an ice-water reference and record offset. Calibration does not certify food safety.

## Bill of materials (indicative prototype estimates)

Per-node CSVs are in `hardware/bom/`; estimates are component-only, exclude enclosure, labor, tax, and shipping, and must be re-quoted. Summing listed quantities gives about $7.10 for the hub, $14.90 per shelf node, $14.70 for the fridge sentinel, and $13.20 for the intake dock ($50.10 with two shelf nodes). Add roughly $25-40 for enclosure, wiring, and mounting hardware; rough prototype total $75-90. Verify exact module variants, load-cell capacities, connector footprints, and supplier availability before purchase.

## Safety, privacy, degraded modes

PantryLink is not a HACCP instrument, regulatory logger, or food-safety authority. Follow local food handling rules and use certified thermometers and staff procedures for decisions. Sensor failures, low battery, missing samples, clock errors, and out-of-range readings display “unknown”; no automatic discard or safety claim is generated. On hub/internet failure nodes keep local sampling and visible status; the hub queues a bounded history; staff use existing paper logs until service returns. Temperature alarms are advisory and require a staff check. No mains switching, refrigeration control, dispensing, or access-control actuation is present.

Store only item categories, approximate counts, equipment telemetry, and volunteer-entered changes. Do not record recipient identity or eligibility; do not add beneficiary data. Keep telemetry local by default; an operator opting into cloud sync should set a retention period (recommended 30 days), restrict site access, and support site-data deletion. TLS, unique device credentials, signed firmware, and per-site authorization are production requirements but not implemented here. Radio certification and applicable privacy/food regulations require review before deployment.

## Maintenance, limitations, and status

Check temperature probe placement monthly, verify against a reference quarterly, and replace batteries when low-battery warnings appear; the six-month target is unmeasured. Load cells require recalibration after mechanical movement. Onboarding target is under 15 minutes but has not been usability-tested. Current prototype has no field validation, KiCad electrical capture, production MQTT broker, user identity, signed OTA, or native mobile build. Netlists are conceptual wiring references only; no fabrication-ready PCB is provided.

| Subsystem | Status |
|---|---|
| Shared protocol C | Host-compiled and unit-tested in this run |
| Node firmware | Portable state examples; vendor target not built |
| API | Source and tests provided; FastAPI/pytest are absent in the execution environment, so tests were not run; not production-hardened |
| ML | Deterministic synthetic one-split-tree demo run; no real data/model claim |
| Mobile | Scaffold only; no native build |
| Hardware | BOM/netlist review only; no physical validation |

## Alternatives considered

- Shared tool-library checkout network — useful but inventory tags and lock hardware raise deployment complexity.
- Community water-point availability — high value, but needs flow-meter calibration and water-quality governance.
- Repair-café queue coordination — plausible software-only solution; multi-node justification weaker.
- Public cooling-site readiness — distinct but already covered by Heat Relief Network.

Invented and maintained by [jayis1](https://github.com/jayis1).

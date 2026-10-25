# RefillReady

Local-first public drinking-fountain and bottle-refill station readiness network. For park and campus maintenance teams, it turns broken, empty, or inaccessible refill points into actionable work orders without tracking visitors.

## Why this is different
The inventory already covers park fountain flow/waste-bin maintenance (ParkPulse), public facility supplies (CivicRest), heat-site water availability (Heat Relief Network), and private well water quality (WellSync). RefillReady instead detects operational failures at individual public bottle-filling fixtures: loss of flow, abnormal dispense duration, suspected leak, and a human accessibility check. It does not measure visitor identities, water chemistry, or park litter.

## User value and limits
A visitor finds a working, usable refill point; maintenance staff see a fault and can verify repair. Each fixture node detects flow and cabinet moisture, a route/access node records a staff-initiated accessible-path check, and a gateway buffers reports and sends work orders. The path-check is an inspection record, not automated accessibility certification. This is not a drinking-water safety monitor and never certifies water as potable.

Offline, nodes retain bounded event queues and the gateway shows local status; cloud loss delays remote work orders only. On sensor disagreement, report “needs inspection,” never “safe.” A physical manual inspection remains authoritative. No cameras, microphones, visitor counters, or geolocation are used.

## System architecture

```text
Fixture Flow Node (ESP32-C3, pulse + leak) --BLE--> Site Gateway (RP2040 + ESP32-C6)
                                                        | Wi-Fi/MQTT TLS
Path Check Node (nRF52840, staff button + IMU) --BLE----+--> FastAPI / SQLite
                                                        +--> Expo staff app
                                                          -> work-order API
```

BLE advertisements carry measurements only; paired GATT transfer is the prototype transport. Gateway caches 256 events in a ring buffer. Server deduplicates by node and sequence. The user can silence non-critical alerts locally. No valve or mains equipment is controlled.

## Nodes and interfaces

| Node | Compute/radio | Sensors / UI | Power | Role |
|---|---|---|---|---|
| Fixture node | ESP32-C3-MINI-1, BLE | YF-S201 hall flow pulse input; normally-open leak switch; service button | 5 V USB supply, 3.3 V LDO | Detect no-flow, unexpected continuous flow, cabinet moisture |
| Gateway | RP2040 + ESP32-C6-MINI-1 co-module, BLE/Wi-Fi | status LEDs, acknowledge button | certified 5 V USB-C adapter | Buffer, pair nodes, publish MQTT |
| Path-check node | nRF52840 module, BLE | button, LIS2DH12 accelerometer | 1-cell protected Li-ion pack | Staff records a periodic manual accessible-route inspection; advisory only |

Pin assignment and electrical notes: [hardware interfaces](docs/hardware.md). These are reference prototypes; no PCB, RF, electrical, or regulatory certification is claimed. Schematics are connection-level design notes, not verified KiCad captures.

## Power and BOM estimate
| Node | Estimate (USD) | Supply/runtime note |
|---|---:|---|
| Fixture | 18.80 | USB supply; low-voltage isolated fixture enclosure; not wired to mains |
| Gateway | 18.80 | certified external 5 V adapter, continuous |
| Path-check | 23.90 | protected 1000 mAh Li-ion; target >2 months at low duty cycle, requires measurement |
| Total | 61.50 | excludes enclosure, labor, certified installation |

Line-item BOMs: [fixture](hardware/bom/fixture.csv), [gateway](hardware/bom/gateway.csv), [path check](hardware/bom/path-check.csv). Estimates are illustrative, not quotations.

## Firmware and software
Shared wire codec is in `firmware/common/protocol.h` and `.c`; packet format is little-endian, versioned, bounded, CRC-checked for corruption (not authentication). Node state machines are reference C using a small HAL seam; vendor SDK compilation and physical testing have not been performed. See [protocol](docs/protocol.md), [architecture](docs/architecture.md), and [API](docs/api.md).

Backend: FastAPI + SQLite prototype (`software/dashboard/`); MQTT adapter is intentionally an integration seam, not an authenticated production broker. ML: synthetic-only fixture-fault scoring demo (`software/ml-pipeline/`); no accuracy claim and no role in safety decisions. Mobile: Expo staff dashboard starter, not production-ready.

## Build and use
1. Assemble only low-voltage prototype nodes; do not attach to plumbing or mains without qualified review and suitable potable-water-rated materials.
2. Calibrate flow pulse count with a measured container; record pulses/litre for each fixture.
3. Pair node IDs at the gateway using the physical service buttons; provision unique credentials before any public deployment.
4. Run the API locally using instructions in `software/dashboard/README.md`; configure broker and credentials out of band.
5. Staff confirm every alert physically, then acknowledge repair; path checks are manual observations with date and staff-selected status.

Calibration helper: `scripts/calibrate.py`. Deployment notes: `scripts/deploy.sh`.

## Safety, privacy, and security
This system cannot test contaminants or certify potability, and is not a substitute for scheduled maintenance or accessibility audits. No automatic shutoff or actuation. Flow sensing is installation-dependent and can misclassify low-flow fixtures. Use food-contact-compatible wetted components; this prototype flow sensor is not assumed potable-water certified. Keep electronics physically separated from water and use an appropriate enclosure and protected low-voltage supply. Follow local radio rules and use certified modules.

Only fixture telemetry, node ID, fault state, and staff-entered path-check status leave site. No visitor data is collected. Production requires mutually authenticated TLS, per-device authorization, signed firmware, key rotation/revocation, retention/deletion policy, and security review; the prototype does not implement these controls.

## Validation status and roadmap
Implemented reference files and host-side codec/backend checks are the intended deliverable. Not validated: target SDK builds, KiCad ERC, sensor accuracy, radio range, flow-meter potable-water suitability, battery life, accessibility compliance, production authentication, or deployment. Next: obtain approved flow hardware, build schematics/PCBs, bench-test fault thresholds, then pilot with municipal maintenance and accessibility reviewers.

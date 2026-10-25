# ShelterSignal

Invented and documented by [jayis1](https://github.com/jayis1).

Local-first, privacy-preserving readiness network for public transit shelters. It helps a transit-maintenance crew find actionable problems—failed lighting, water pooling, damaged seating, blocked step-free approaches—without monitoring passengers or claiming that a shelter is safe.

## Product brief

- **User:** a small-city transit maintenance dispatcher responsible for 50–300 stops.
- **Recurring friction:** crews learn about dark or flooded shelters through scattered calls and arrive without knowing what to inspect.
- **Today:** rider reports, paper inspections, and periodic route sweeps; ShelterSignal does not replace those channels.
- **Why multiple nodes:** a gateway aggregates independently powered shelter and approach observations across a stop; one sensor cannot distinguish a failed lamp from a dead sensor or a blocked path from a shelter fault.
- **Outcome target (not measured):** reduce unnecessary inspection visits and shorten time-to-triage; evaluate against a 4-week baseline before making any savings claim.
- **Kill risk:** installation/maintenance cost and false alerts may exceed the value for low-ridership stops.
- **Closest alternatives:** rider-report apps and periodic manual inspections. Difference: a fixed, anonymous readiness checklist with sensor evidence and explicit human confirmation; not passenger counting or automated enforcement.

## System architecture

Three cooperating hardware units: one site gateway, one shelter-condition node and one step-free approach check node. Nodes report status, not identity. The gateway retains a rolling local queue and provides a locally visible fault state when internet is unavailable. Cloud service is optional for daily operation.

```text
[Shelter node: light + water ingress + temperature]
                 \\ BLE advertisements / GATT
                  > [Site gateway: ESP32-S3, local rules, queue]
                 /         | HTTPS/MQTT over TLS (optional)
[Approach node: manual clear/blocked button + tilt]   [API + MQTT + dashboard]
                                                        [React Native crew app]
```

BLE is selected for short-range, low-rate observations within a stop; no mesh or range guarantee is claimed. The gateway must be mounted within tested radio range. Internet loss queues bounded events locally; node loss becomes `stale`, never silently `clear`. Cloud and ML are advisory. A manual inspection remains authoritative.

## Nodes and pin assignments

| Node | SoC / radio | Signals (prototype GPIOs) | Power / behavior |
|---|---|---|---|
| Site gateway | ESP32-S3-WROOM-1; Wi-Fi + BLE | I²C SDA 8/SCL 9 to optional OLED; status LED GPIO 4; service button GPIO 0 (boot strap, external pull-up and commissioning caution) | Listed 5 V USB-C input; enclosed, fused prototype supply. Always-on; no mains switching. |
| Shelter condition | nRF52840 module; BLE 5 | I²C SDA P0.26/SCL P0.27 to BME280; phototransistor ADC P0.04; reed/contact input P0.11 | 2×AA primary cells; radio sleep between 5-minute samples; runtime depends on measured cell chemistry and temperature. |
| Approach check | nRF52840 module; BLE 5 | CLEAR button P0.11, BLOCKED button P0.12, enclosure tilt switch P0.13, status LED P0.15 | 2×AA; staff-operated buttons; no camera, microphone, occupancy or location tracking. |

Pin assignments are a design proposal, not a verified PCB mapping; check the selected module carrier pinout before wiring. Sensors are environmental/maintenance indicators only. Light level cannot prove that illumination meets a standard; contact sensing cannot establish a water hazard's extent. The approach node records a human-declared inspection state, not autonomous accessibility certification.

## Data and protocol

Frames are bounded, little-endian, versioned records defined in `firmware/common/protocol.h`. Each record carries node ID, monotonically increasing sequence, event type, sample age, and fixed-point values; CRC-16 detects corruption but is **not authentication**. BLE link encryption/bonding and per-node commissioning are required for deployment; the demo frame alone must not be treated as secure. Backend rejects unknown versions, invalid lengths, and out-of-range values. `docs/protocol.md` is normative.

No personal data is collected. No audio, images, device MAC addresses, or passenger counts are stored by the application. The demo backend uses MQTT TLS in deployment; credentials/certificates are injected at runtime and scoped to a site. Example configuration contains placeholders only. Default retention target is 30 days of event summaries; operators can shorten it.

## Power and cost estimate

Indicative prototype BOM totals (USD, budgetary estimates; availability/prices vary): gateway $34.80, shelter node $17.20, approach node $17.30; total $69.30 before installation, tax, labor and cellular/backhaul. BOM totals are budgetary line estimates, not quotes.

Battery life is not asserted until radio duty cycle, sensor leakage, cold-temperature capacity, and cell self-discharge are measured. Design target: 5-minute sampling, event-driven BLE, bounded advertising, and a low-battery warning; replace cells at a conservative service interval established by field testing. Do not use rechargeable cells with a non-rechargeable holder or leave primary cells exposed to weather.

## Software and implementation status

| Component | Status |
|---|---|
| Firmware/protocol | Protocol codec and host test included; MCU peripheral integration is a prototype stub, not built against vendor SDKs. |
| Schematics | Per-node connection-reference files included; **not KiCad schematics or fabrication-ready**, no ERC/PCB performed. |
| Dashboard | FastAPI local ingestion/status API with MQTT adapter contract; demo only, authentication and persistent database deployment remain operator work. |
| ML | Synthetic-data training demonstration only; no trained/validated field model. Rules are the fallback and primary alert logic. |
| Mobile | React Native screen/API stub; not store-ready, no account or push-notification service. |
| Hardware | No boards assembled or environmental/radio/regulatory testing performed. |

ML is not required for core behavior. Optional `train.py` demonstrates a synthetic-data smoke path only; do not use its output for safety decisions. Deterministic age and threshold rules remain authoritative.

## Build and first 15 minutes

1. Review the node connection references and verify module-carrier pinouts (2 steps).
2. Assemble low-voltage sensor prototypes indoors; do not connect to transit infrastructure (2).
3. Configure unique node IDs and BLE bonds using a commissioning tool (not included; manual demo configuration only) (1).
4. Start API with `python -m pip install -r software/dashboard/requirements.txt` then `uvicorn software.dashboard.main:app --host 127.0.0.1` from this directory (2).
5. Run host protocol tests with `cc -std=c11 -Wall -Wextra firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/ss-test && /tmp/ss-test` (1).

This is 8 conceptual steps; actual field commissioning requires a qualified transit operator, site permissions, weatherproof enclosure, validated mounting and accessibility review. App setup is a mock interface only.

## Calibration, safety, and regulatory limits

Compare light sensor readings against a calibrated lux meter in daylight/night conditions; inspect and document the sensor's spectral/angular bias. Verify the water contact sensor with dry/wet checks without claiming flood depth. Staff must inspect the approach visually and use the agency's accessibility standard. A `BLOCKED` report prompts human assessment; absence of a report is not evidence of clearance.

Low-voltage SELV prototype only. No connection to streetlight mains, transit controls, or emergency systems. Outdoor deployment needs ingress, UV, vandalism, temperature, EMC, battery containment, RF coexistence and mounting tests. BLE/Wi-Fi radio and enclosure require region-specific compliance review; no FCC/CE certification is claimed. This system is not an emergency alert, safety certification, or replacement for maintenance inspections. Provide tactile buttons, high-contrast labels and a non-app reporting workflow.

## Deployment and known limitations

See `docs/architecture.md`, `docs/api.md`, `scripts/calibrate.py`, and `scripts/deploy.sh`. Deployment script is a local-development helper, not hardened fleet provisioning. Until authentication, device identity, persistence, signed firmware updates, and retention controls are implemented, keep the API on loopback/trusted lab networks. Never publish an unauthenticated broker or API to the internet.

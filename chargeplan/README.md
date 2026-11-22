# ChargePlan

ChargePlan helps a household with an EV understand when charging occurred, what it cost under its own tariff, and whether household demand overlapped with the session. It is an observability and scheduling-advice system, not an EV charger controller. It does not switch mains, alter vehicle settings, or promise savings.

Status: early prototype reference design. Host-side protocol and backend are executable; the electrical designs, target firmware, MQTT deployment, ML pipeline, and mobile UI are not hardware- or production-validated. Nothing here is a certified EVSE design.

## System contract

Users: households with an existing, independently certified EVSE and a utility tariff they can enter. Benefit: a local session history, estimated tariff cost, and transparent off-peak suggestions. Inputs are charger-reported session events (preferred) or manually entered records; household import power readings are optional context only. No utility credentials, vehicle location, VIN, or driving history are collected.

Nodes: (1) Charge Gateway, Raspberry Pi Zero 2 W, local MQTT broker/API and SQLite; (2) EVSE Observer, ESP32-C3, isolated low-voltage opto-input that observes a certified charger's dry-contact session output; (3) Home Load Observer, ESP32-C3 + split-core CT interface, installed only on a safely enclosed, isolated low-voltage metering module by a qualified electrician. Observer devices do not control charging. The mobile client presents gateway data. The gateway is the coordinating hub.

Data path: observer event -> versioned JSON over Wi-Fi/MQTT QoS 1 -> gateway validation and idempotent SQLite insert -> FastAPI read API -> mobile scaffold. LAN operation is sufficient; internet is not required. Offline node queue is bounded (64 records); overflow is surfaced as a gap, never silently treated as complete data. Losing gateway or power disables collection, not charging. A charger must retain its own safety protections and controls.

## Interfaces and electrical notes

Both observer reference designs use an ESP32-C3-MINI-1 (3.3 V logic, Wi-Fi 2.4 GHz) and an external certified 5 V USB supply with a regulator rated for radio current peaks. EVSE Observer: isolated contact-input module output to GPIO4; GPIO5 status LED through 1 kΩ; GPIO9 pairing button to ground with pull-up. Do not connect EVSE conductors or mains to MCU pins. Contact input polarity and isolation ratings depend on selected module and require verification against the specific charger's output.

Load Observer: use a commercially enclosed, isolated CT energy-monitoring module with a documented SELV UART output; module TX -> ESP32 GPIO20 (UART RX), module RX -> GPIO21 through 3.3 V-compatible interface, common SELV ground only; GPIO5 status LED. Do not build an exposed CT/mains circuit from this concept note. CT installation and enclosure are electrician-only; never disconnect a live CT secondary. This reference interface does not name a validated metering module, so no accuracy claim is made. BOMs are selection guidance, not procurement-ready.

Power: low-voltage USB adapters only; no battery or actuator. Verify regulator thermal/current margin, module antenna keep-out, isolation barriers and enclosure before implementation. Example KiCad folder contains engineering notes, not native CAD schematics or fabrication data.

## Protocol v1

UTF-8 JSON, maximum 512 bytes: `{"v":1,"node":"evse-01","seq":42,"ts":1730000000,"kind":"session","state":"started","energy_wh":0}`. `v` is protocol version; `node` is provisioned local identifier; `seq` monotonically increases per node; `ts` is UTC Unix seconds; event key `(node,seq)` makes QoS 1 duplicates idempotent. Supported kinds are `session` (state started/ended, nonnegative energy_wh) and `power` (nonnegative watts). Reject unknown version, node, kind, invalid units, oversized input and duplicate event IDs. Events are advisory observations, never commands. Production requires TLS, unique device credentials, broker ACLs, secure enrollment, key revocation and signed updates; the example is trusted-LAN only and implements none of those controls. Reboot sequence persistence and clock trust remain target integration requirements.

MQTT topic: `chargeplan/v1/<node>/events`, QoS 1, non-retained. Gateway subscribes only to provisioned node IDs. API: `POST /events` for telemetry ingestion, `GET /health`, and `GET /events?limit=100`; there is no control/actuation API. Default backend binds loopback. Do not expose it publicly without authentication and TLS.

## Software and ML

`firmware/common/protocol.c` provides bounded C JSON event validation; `firmware/tests/protocol_test.c` exercises valid, malformed, oversize, and version rejection. Drivers, radio stack, persistent sequencing and target SDK projects are not implemented. FastAPI stores accepted events in process memory in this prototype; SQLite durability and MQTT broker integration are deployment work. The mobile app is a disconnected UI scaffold. ML training uses generated synthetic session data only to smoke-test feature plumbing; it cannot demonstrate savings or forecast quality. Schedule suggestions are deterministic tariff arithmetic with user-entered rates, not ML decisions.

## Validation and limitations

Run `gcc -std=c11 -Wall -Wextra -Werror firmware/common/protocol.c firmware/tests/protocol_test.c -o /tmp/chargeplan-test && /tmp/chargeplan-test` and `python3 -m unittest discover -s software/dashboard -v`. Hardware, RF, target firmware builds, MQTT integration, React Native builds, model quality, energy-meter calibration, cybersecurity, and electrical compliance are unverified. This is not a bill, energy audit, EVSE safety mechanism, or substitute for the charger manufacturer's instructions. Users retain full control of charging; manual charger controls always win.

## Project layout

- `schematic/`: node interface notes (not KiCad schematics)
- `hardware/bom/`: per-node selection BOMs with unresolved metering module marked
- `firmware/`: portable C event validator and node integration boundary
- `software/dashboard/`: FastAPI reference API
- `software/ml-pipeline/`: synthetic-only training smoke test
- `software/mobile-app/`: React Native display scaffold
- `docs/`: API, threat boundary, and architecture
- `scripts/`: local validation command

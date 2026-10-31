# DrainWatch — community street-inlet maintenance observations

DrainWatch helps a neighborhood association or public-works crew prioritize inspections of curb inlets after leaf fall and heavy rain. A water-level node in a monitored catch basin and a separate rain/context node report coarse observations to a nearby gateway; a staff inspection beacon records human confirmation. It reports evidence for inspection only: it does not predict flooding, certify drainage, or control infrastructure.

## Novelty and user

A volunteer coordinator currently walks every inlet after storms or waits for resident calls. The target user is a municipal maintenance lead managing a small catchment; the measurable pilot outcome is inspection dispatch time and proportion of flagged inlets confirmed obstructed, not flood prevention. This is distinct from StormSync (private sump/pump failure), RainGarden (garden soil and basin stewardship), and DrainSync (building plumbing/sewer backup): DrainWatch observes public curb-inlet inspection context and deliberately has no actuator, occupant sensor, or safety claim.

## System topology

```text
Curb inlet node ×2 ── LoRa point-to-point ──┐
Rain/context node ── LoRa point-to-point ───┼─ Gateway (RP2040 + SX1262)
Inspection beacon ── BLE / manual input ────┘       │ Wi-Fi MQTT (optional)
                                                   FastAPI + SQLite
                                                   React Native status stub
```

MVP contains a gateway, two curb-inlet nodes and one rain/context node. The inspection beacon is an operator-held USB/BLE device represented by a manual API event in this prototype, not a fabricated electronic design. Nodes send one compact observation every five minutes and on threshold crossing; LoRa is sparse telemetry, not a mesh or guaranteed link. Gateway buffers recent readings locally in the intended deployment; firmware radio drivers are not implemented here.

| Node | Candidate electronics | Power | Design intent |
|---|---|---|---|
| Curb inlet ×2 | Raspberry Pi Pico W (RP2040), VL53L0X ToF range sensor, SX1262 868/915 MHz module | Protected 18650 + regulated 3.3 V; prototype target 1 report/5 min | Distance to water surface through a protected stilling tube; report level trend and sensor validity |
| Rain/context | Pico W + tipping-bucket reed contact + BME280 + SX1262 module | USB 5 V or protected solar/battery subsystem (not designed) | Rain pulse count and ambient temperature/humidity context |
| Gateway | Pico W + SX1262 module + status LEDs | USB 5 V | Receive/validate telemetry, retain last state and relay to API |

Part choices are candidate modules; check exact board/module revisions, RF region, antenna, datasheets and supplier stock before purchase. See per-node BOM and pin/net schedules. Water-facing housing, condensation, debris, maintenance access, and sensor fouling require field engineering. No part of this reference is certified for installation in public infrastructure.

## Data and failure behavior

Frame format and Python mirror are specified in `docs/protocol.md`. Invalid CRC, unsupported version, duplicate sequence, impossible range, or stale telemetry is rejected or surfaced as `unknown`; it never means clear. A node cannot establish that an inlet is blocked from one range reading: the UI labels an advisory trend and requires a field inspection. The service stores only site/node identifiers, measurements, sequence and timestamps. No personal data, images, audio or location of people is collected. Pilot operator must choose retention and access controls.

LoRa loss: gateway reports age and missing data; no remote command path exists. Internet loss: readings remain local only if deployment gateway buffering is implemented; current gateway C is a portable skeleton, so that behavior is planned. Battery depletion and sensor fouling present as stale/invalid, not clear. Manual inspection and public works procedures remain authoritative.

## Readiness

| Area | Status |
|---|---|
| Shared binary protocol codec + host tests | Implemented; CRC-16 detects accidental corruption, no authentication |
| Node firmware | Portable C data/validation logic; target radio, sensor, sleep and drivers are not implemented |
| Native KiCad / PCB / enclosure | Not implemented; pin/net schedules only |
| FastAPI + SQLite API | Implemented prototype; MQTT transport is documented but not connected |
| ML | No model. Threshold/trend rules suffice; synthetic fixture generator only |
| Mobile app | Minimal React Native display stub; not device-tested |
| Physical, RF, environmental validation | Not performed |

## Build and verify

From repository root:

```sh
gcc -std=c11 -Wall -Wextra -Werror -Idrainwatch/firmware/common drainwatch/firmware/common/protocol.c drainwatch/firmware/common/test_protocol.c -o /tmp/drainwatch-test && /tmp/drainwatch-test
python3 -m unittest discover -s drainwatch/software/dashboard/tests
python3 -m py_compile drainwatch/software/dashboard/app.py drainwatch/software/ml-pipeline/generate_fixture.py
```

API quick start: install `software/dashboard/requirements.txt`, then run `uvicorn app:app --app-dir software/dashboard --host 127.0.0.1 --port 8000`. API binds localhost by default; production needs authentication, TLS, per-site authorization, MQTT ACLs, rate limiting, backup and retention policy. Example config contains placeholders only. The mobile stub is not a production app.

## Safety, radio and regulatory limits

Prototype sensing only. Do not enter a roadway, confined space, or catch basin to install or service nodes; use trained public-works personnel and traffic-control procedures. Enclosure and battery design must be reviewed for water ingress, corrosion, thermal and fire hazards. No mains interface, actuator or autonomous traffic/drainage control is included. LoRa frequency must match the installation jurisdiction (e.g. 868 MHz EU / 915 MHz US); regional duty-cycle, antenna and radio certification rules apply. The selected radio module does not by itself certify the assembled product. No flood-risk reduction or safety performance is claimed.

Invented and maintained by jayis1.

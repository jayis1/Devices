# CarePath — Accessible Venue Entry Readiness

Status: engineering concept / host-simulated prototype; not fabrication-ready, certified, or validated for accessibility compliance. Invented and maintained by jayis1.

CarePath helps a small public venue keep its step-free entrance usable by coordinating a doorway observer, route-surface observer, and staff check-in hub. It provides fresh, evidence-based operational context—not a guarantee of access, a substitute for an accessibility audit, or an automated door controller. Unlike LiftLink (lift availability) and CurbCue/Waymark (sidewalk/crossing/wayfinding), CarePath covers the venue's own step-free entry path and staff-confirmed temporary obstructions.

## System contract

A venue operator mounts two battery/USB-powered low-voltage observation nodes: Entry Node (reed contact for a manually inspected door state plus pushbutton staff override) and Approach Node (time-of-flight distance sensor aimed at a fixed clear-path reference). A Hub aggregates observations over BLE, offers a local status display/API, and optionally publishes to a private MQTT broker. A React Native operator stub displays freshness and prompts staff to confirm; it does not collect visitors, images, audio, or identities. The ML component is an offline baseline that estimates sensor drift from labeled maintenance readings; it never decides access status.

Benefit: reduce time staff spend discovering a blocked/closed step-free route and make manual checks consistent. No sensors infer disability or detect people. A human must inspect and explicitly confirm usable access. If any input is missing/stale/ambiguous, report UNKNOWN, not READY.

```text
Entry Node --BLE GATT--> Hub <--BLE GATT-- Approach Node
                             | local API / optional MQTT over TLS
                         API service --> operator app
```

### Nodes and preliminary interface allocation

| Node | Prototype hardware | Interfaces / allocation | Power |
|---|---|---|---|
| Hub | Raspberry Pi Pico W (RP2040 + CYW43439 module), 2.4 GHz Wi-Fi/BLE | USB; local JSON API; BLE central (radio support depends on selected SDK/firmware build) | Regulated 5 V USB; no mains wiring |
| Entry Node | Raspberry Pi Pico W; magnetic reed contact; momentary staff-check button | GPIO 2 reed input with 10 kΩ pull-up; GPIO 3 button input with 10 kΩ pull-up; BLE telemetry | 5 V USB prototype; battery version deferred |
| Approach Node | Raspberry Pi Pico W; VL53L0X breakout, status LED | I²C0 SDA GP4/SCL GP5, 3.3 V logic; BLE telemetry | 5 V USB prototype |

These are logical pin proposals, not verified PCB schematics. Breakout board regulator/level-shifter variants must be checked before connection. GP2–5 are not reserved boot pins on the target but pin assignment and SDK configuration remain to be verified. Use a breakout whose I²C pullups go to 3.3 V; never apply 5 V logic to RP2040. Set sensor timeout and reject out-of-range values. No actuator, lock, or life-safety function exists.

## Protocol v1

UTF-8 JSON envelope: `{ "v":1, "node":"entry-01", "seq":7, "uptime_ms":1234, "kind":"entry", "door_open":false, "staff_check":true }`. Approach payload uses `clearance_mm` integer (0–5000) and `valid` boolean. Nodes send monotonically increasing uint32 sequence per boot; hub treats duplicate/out-of-order sequence as non-new, and freshness expires after 120 s using monotonic receipt time. Messages >512 bytes, unknown versions, missing fields, invalid node IDs, negative/out-of-range values are rejected. Wall-clock timestamps are optional metadata only. BLE pairing and encrypted link security are required for deployment but not implemented in the host prototype; do not use this prototype on a public/untrusted radio network. MQTT, if enabled in a future deployment, must use TLS, individual credentials and per-node ACLs; no public broker or embedded shared password.

Readiness state is `UNKNOWN` until both observations are fresh and a staff check-in says the route was physically inspected. Closed door, insufficient measured clearance, stale input, sensor fault, or absent human check yields `CHECK_REQUIRED`; this is operational guidance only and thresholds must be configured through an accessibility audit. No command topic exists.

## Repository contents

- `firmware/common/protocol.h`: C envelope bounds and state vocabulary.
- `firmware/entry-node/main.c`, `approach-node/main.c`, `hub/main.c`: portable host-simulatable C logic; hardware drivers/BLE integration intentionally not claimed.
- `hardware/bom/*.csv`: prototype parts list with variants to resolve before procurement.
- `schematic/README.md`: net/pin intent; no KiCad schematic files are supplied, so do not fabricate from this document without engineering review.
- `software/dashboard/`: FastAPI local prototype and tests. No authentication or MQTT integration: bind to localhost only.
- `software/ml-pipeline/`: reproducible synthetic-data baseline; no accessibility or obstruction classifier.
- `software/mobile-app/`: minimal operator display contract stub.
- `docs/`: architecture, protocol, API, threat/failure model.

## Run and verify

Python 3.10+: `cd software/dashboard && python -m pip install -r requirements.txt && pytest -q`; local run `uvicorn main:app --host 127.0.0.1`. Host C logic: `cc -std=c11 -Wall -Wextra -Werror firmware/entry-node/main.c -o /tmp/carepath-entry` (repeat for node files). Hardware tests, BLE integration, KiCad/ERC, mobile build, real-data ML evaluation, and physical accessibility testing are not performed. Do not represent this as an installed or compliant system.

## Safety, privacy, limitations

No cameras/mics, user tracking, remote access-control, emergency dispatch, or autonomous actuation. Sensor readings can be misleading (misalignment, reflectivity, weather, battery/power loss, tampering); device status is not proof of a clear route. Provide conventional staff checks and an accessible manual contact method. Store only the most recent observation in the prototype. No personal data or secrets belong in this repository. Firmware updates, provisioning, authenticated BLE, cloud account isolation, and secure device identity are unimplemented. Before deployment, complete datasheet and breakout review, enclosure/weather assessment, radio regulatory review, threat model, accessibility audit, and field trials.

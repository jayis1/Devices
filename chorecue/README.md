# ChoreCue — shared household routines

Status: concept prototype by jayis1. Not built, calibrated, usability-tested, or safety-certified.

ChoreCue helps a household coordinate recurring chores and shared supplies without turning home life into surveillance. People create tasks and mark them done with a physical button or local phone app; a supply dock reports when a refill bin is low, while optional room beacons show only a manually selected “quiet / available / do not disturb” state. The hub presents an agreed household board and workload history. It does not infer who is home, judge effort, assign chores automatically, or collect audio, images, location, or biometric data. Use is opt-in and all household members can see and delete the shared record.

## Nodes and data flow

```text
Task button (nRF52840, BLE) ----+
Supply dock (ESP32-C3, load cell) +--> Home hub (Raspberry Pi 4 / Python)
Room beacon (nRF52840, BLE) -----+       | local SQLite + web API
                                         +--> optional MQTT bridge
                                         +--> React Native client
```

The hub is the coordinator and source of task state. Nodes send observations, never commands. The button sends a task identifier selected during household setup and a monotonically increasing event counter; the dock sends weight in grams. The beacon broadcasts a short manually selected availability state. BLE transport and hub ingestion are currently illustrative interfaces, not implemented radio drivers. Cloud sync is disabled by default. Losing internet does not affect local operation; losing the hub leaves buttons unable to record events until it returns. Do not use this as an emergency, medical, or security system.

## Hardware reference design

| Node | Reference parts / interfaces | Power and safety |
|---|---|---|
| Hub | Raspberry Pi 4 Model B, USB-C 5 V supply, Wi-Fi/Ethernet; BLE adapter required for prototype | Certified enclosed USB supply only; no mains switching; UPS optional |
| Task button | Nordic nRF52840 module, momentary button to GPIO with pull-up, status LED via resistor, BLE | Protected 3 V coin cell or rechargeable module; avoid charging a primary cell |
| Supply dock | ESP32-C3 module, HX711-compatible load-cell ADC, 1 kg load cell, BLE/Wi-Fi | USB 5 V input, regulated logic supply; platform mechanically stable and not load-bearing |
| Room beacon | nRF52840 module, three-position switch, RGB LED through resistors, BLE | Protected battery supply; user-controlled state only |

Exact module variants, pin availability, battery life, radio range, and component compatibility are unverified. Select modules with exposed programming/debug pads and documented antenna keep-out. The load cell is a coarse refill-level indicator, not a scale. Use low-voltage enclosed supplies; no connection to appliances, locks, or building services.

## Event contract

Version 1 logical event: `{"v":1,"node":"button-kitchen","seq":17,"type":"task_done","task":"dishwasher-empty","value":1,"unit":"bool","utc_ms":0}`. `node` is an enrolled local identifier (1–48 characters); `seq` is uint32; event payload is at most 192 UTF-8 bytes. Supported types: `task_done` (value 0/1, unit `bool`), `supply_g` (0–100000, unit `g`), `availability` (0–2, unit `enum`). Reject unknown versions, types, units, ranges, malformed JSON, and duplicate `(node,seq)`. Sender time zero means unknown; hub receipt time is authoritative. Counter wrap requires node re-enrollment/reset while physically present. Events are idempotent; no remote actuation commands exist. Prototype radio is unauthenticated and must not be treated as secure. Production requires authenticated enrollment, encryption, revocation, protected credentials, TLS upstream, topic ACLs, and retention/deletion controls.

MQTT topic convention when explicitly enabled: `chorecue/v1/{household_id}/events`; QoS 1, no retained event messages. Mobile API: `GET /v1/tasks`, `POST /v1/events`, `GET /v1/summary`. The included backend validates a logical event only; it does not yet implement persistence, identity, household authorization, MQTT, or these endpoints. No production API or mobile integration is claimed.

## Fairness and privacy

Task completion is explicit, not sensed. A “fairness summary” reports only household-agreed task counts over a chosen period and must not be interpreted as effort or contribution quality. Allow task rotation and shared/anonymous completion; never require a named attribution. Do not persist beacon history beyond current state. Local database retention defaults to 30 days; household members can export and delete records. These policies are requirements, not implemented controls in this prototype.

## ML pipeline

`software/ml-pipeline/train.py` demonstrates a deterministic workload-smoothing baseline on clearly synthetic tasks only. It is not a trained or validated model and must not allocate chores, assess people, or make claims about fairness. Any future opt-in evaluation must use household-consented, pseudonymized task histories, household-grouped train/test splits, and explicit member review; rule-based rotation remains the fallback.

## Readiness and limitations

| Area | State |
|---|---|
| Requirements / architecture | Specified; design assumptions need household usability review |
| Schematics | Connection notes only; no native KiCad design or ERC claimed |
| Firmware | Portable C event-validation library; host-build tested if listed by repository CI (not radio firmware) |
| Backend | Local validation prototype; tests included; no auth, database, or MQTT integration |
| Mobile | React Native source stub; not install/build tested |
| ML | Synthetic demonstration only; no performance claims |
| Physical / RF / battery tests | Not performed |

Run backend tests from `software/dashboard` with `python3 -m unittest`; run the portable protocol test with `cc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/chorecue-test && /tmp/chorecue-test`. Repository must not be represented as build-ready: no native CAD tool is installed in the authoring environment, and module-level electrical review, battery measurements, RF/security testing, accessibility/usability evaluation, and privacy implementation remain required.

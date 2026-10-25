# CurbCue — accessible sidewalk hazard coordination

Status: research prototype / reference design. No physical prototype, KiCad ERC, RF, accessibility, or field validation has been performed. Not a certified safety or navigation aid; never directs a user to enter traffic. All reports are advisory and require human inspection.

## Need and users

Wheelchair users, people using walkers, parents with strollers, and maintenance crews often encounter blocked curb ramps, standing water, damaged tactile paving, or snowed-in sidewalk links. A route can be nominally accessible yet unusable at the exact crossing. CurbCue combines fixed, low-resolution condition sensing with optional human reports and sends work crews a prioritized location—not a person-tracking trail. Unlike general navigation apps it focuses on time-bounded, evidence-backed public right-of-way maintenance, and never asserts a route is safe.

Success hypothesis (not measured): reduce median time from verified obstruction report to inspection by 25% at a pilot district, assuming a participating maintenance team and baseline timestamps. No claim of improved accessibility until users and crews validate it.

## System

```text
Curb node (load + tilt + water) --BLE--> Site hub (local queue, display, Wi-Fi)
     Crossing button (manual report) --BLE--> | MQTT/TLS, opt-in sync
     Route beacon (crew acknowledge) --BLE-->| FastAPI + SQLite / MQTT
                                               Mobile web/RN stub
```

Four hardware roles are justified by location: curb node senses a specific ramp, button lets a passerby report without a phone, hub coordinates and buffers, beacon lets a crew acknowledge at the site. There is no camera, microphone, user identity, or automatic route guidance. BLE 5 LE star; hub supports Wi-Fi uplink; no long-range mesh claims. Hub remains locally useful without Internet. Nodes store only event counters and sensor summaries.

| Node | Parts / interface | Power / safe state |
|---|---|---|
| Hub | Raspberry Pi Zero 2 W; USB BLE adapter (CSR8510A10 reference); 2.9in SSD1680 e-paper SPI | 5 V USB-C supply; service continues local-only when WAN fails |
| Curb node | Seeed XIAO nRF52840 (nRF52840); HX711 + 10 kg load cell; LIS3DH tilt; resistive wetness pads via divider/ADC | 18650 protected cell + MCP73831 charger reference; enclosure IP65 target, not validated; no actuator |
| Crossing button | XIAO nRF52840; sealed momentary switch; RGB LED | 2×AA through regulated 3.3 V; debounced, no signal if held/stuck |
| Crew beacon | XIAO nRF52840; button + LED + piezo | 2×AA; acknowledgement is local and non-binding |

Pin assignments are board-level proposal (verify exact XIAO revision before PCB): sensor I²C SDA/SCL on D4/D5 (GPIO 6/7), SPI e-paper hub uses Pi SPI0; HX711 D2/D3; wetness ADC A0; LIS3DH I²C 0x18. No external high-energy actuator. External sensor connections need ESD protection and strain relief. Battery assembly and charger require qualified review; do not charge unattended. Use a gasketed, UV-resistant enclosure and test ingress/condensation before outdoor deployment.

## Protocol and behavior

BLE GATT prototype service transports compact JSON-compatible event fields: `v` (1), `node_id` (provisioned random identifier), `seq` (uint32), `kind` (`obstruction|wet|tilt|manual|ack`), `sample_ms` (monotonic uptime), `severity` (0–3), and optional bounded sensor summary. Maximum encoded event 160 bytes. Hub deduplicates `(node_id,seq)`, rejects unknown versions and invalid ranges, and queues events locally; wall time is attached only at hub receipt. Prototype transport does not yet provide production provisioning or authenticated payloads: never deploy on a public site until BLE pairing, per-device credentials, revocation and broker ACLs are implemented. MQTT requires TLS and per-site credentials in deployment; no secrets are included.

Manual report is never treated as confirmed. Hub marks evidence `unverified`; crew inspection can transition to `confirmed`, `cleared`, or `false_positive`. No remote actuation. Offline nodes show stale status locally; they cannot claim a ramp is clear. Bounded retry with jitter, max 10 queued events/node; on overflow keep newest and expose a loss counter.

## Power and cost (engineering estimates)

Curb node estimate: protected 3000 mAh cell, 0.12 mA sleep + 12 mA × 2 s / 300 s sampling = 0.20 mA average; ideal life 625 days; derate 50% for cold, conversion loss and aging => about 312 days. Prototype target only; measure real quiescent current and radio duty cycle. Button/beacon estimated 0.08 mA average on 2000 mAh AA pair => 1041 ideal days, 50% derated ~520 days. Hub mains-powered, 5 V/2 A supply.

BOM estimates, USD retail ranges, enclosure/cabling excluded: hub $35–55; curb node $32–55; button $12–20; beacon $12–20; total $91–150. Figures are planning estimates, not quotes. See per-node CSVs.

## Software and ML

Backend is a local FastAPI prototype with event validation and an MQTT adapter contract. Mobile app is an explicitly nonfunctional typed UI stub. `software/ml-pipeline/train.py` demonstrates reproducible synthetic feature generation and baseline evaluation; synthetic values are not evidence of hazard-detection performance. Production value likely comes from simple transparent thresholds and crew-confirmed outcomes, not an unvalidated model. No model can label a route safe.

## Safety, privacy, limits

No mains switching, locks, traffic signals, or medical function. No user tracking; random node identifiers are site-scoped and event retention should default to 30 days, then aggregate/delete. Do not collect free-text personal details. Manual indicators need accessible contrast, tactile identification, reachable mounting height and user testing. Enclosure IP65 is a design target, not a rating. Snow, debris, vandalism, sensor drift, false positives and connectivity loss can all defeat sensing; periodic human inspection remains required. Local signals and signage govern. Not for emergency response, legal compliance, or autonomous routing.

## Build outline

1. Assemble a bench node using a XIAO nRF52840 and sensors; confirm voltage levels and pin mapping against the exact board revision.
2. Install Zephyr SDK and build each `firmware/*/CMakeLists.txt` target; firmware currently provides portable event-state scaffolding, not a completed BLE stack.
3. Run backend: `python -m venv .venv && . .venv/bin/activate && pip install -r software/dashboard/requirements.txt && uvicorn main:app --app-dir software/dashboard`.
4. Run `python software/ml-pipeline/train.py`; generated data is transient and synthetic.
5. Connect an authenticated broker only after implementing provisioning/ACLs. Validate in a supervised closed-course pilot with disability-led review before outdoor installation.

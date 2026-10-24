# QueueKind — private, live wait estimates for community service sites

Inventor: jayis1. QueueKind helps walk-in clinic, advice-desk, and community-service visitors decide whether to join a queue now or return later, without cameras, face recognition, or named visitor records.

## User problem and why it is multi-node

A visitor with limited time or mobility often arrives at a busy walk-in service desk without knowing the wait. The current workaround is to travel, ask staff, and wait in a crowded room. QueueKind aims to show a coarse wait band (e.g. under 15, 15–30, over 30 minutes), reducing avoidable waiting and repeat questions; outcome targets require a site pilot and are not established results. Separate entrance counters, staff-operated service-desk controls, and a local hub are required because arrivals and service completions occur at different physical points. No single phone can observe both reliably.

Typical day: staff switch the display to open; a doorway node counts anonymous beam breaks; staff tap “next visitor” after completing service; hub estimates a wait band and updates a local display and optional web client. On internet loss, hub and display continue locally. If a counter fails, the estimate is marked stale rather than fabricated. At closing, staff stop collection and the hub deletes raw event timestamps after aggregation.

Non-goals: individual tracking, appointment scheduling, emergency triage, staff performance scoring, and guaranteed wait times. This is not an emergency service; visitors should follow site staff instructions.

## System overview

```text
Door beam node ─┐
Desk completion ├─ BLE (local) ─ QueueKind Hub ─ local display + SQLite/API
Staff presence ─┘                                  └─ optional authenticated MQTT sync
                                                   └─ React Native visitor/staff stub
```

| Node | Hardware | Role |
|---|---|---|
| Hub/display | Raspberry Pi Zero 2 W, 5-inch HDMI display | Local coordinator, estimate, API and cache |
| Door counter | ESP32-C6 module, paired break-beam sensors | Directional anonymous arrivals count |
| Desk button | ESP32-C6 module, two physical buttons | Service completion and pause/resume events |

Link: BLE GATT in a local star; event messages are bounded, versioned and sequenced. The hub rejects duplicate sequence numbers and stale reports. CRC-16 detects accidental corruption only and is not authentication. Prototype radio security is incomplete; do not deploy on an untrusted network. No cloud dependency exists for local operation.

## Electronics and installation

All nodes use listed development modules; this is a reference design, not a fabrication package. Door node: 5 V USB supply, 3.3 V logic, two 5 V-compatible break-beam receiver outputs through a verified level-shift/input interface, GPIO4/GPIO5; status LED GPIO8; setup button GPIO9. Desk node: 3.3 V ESP32-C6 module, buttons GPIO4/GPIO5 with internal pull-ups, LED GPIO8. Confirm exact module pin mux and board schematic before assembly. Hub: certified 5 V USB supply. Keep wiring low-voltage SELV, route cables to avoid trip hazards, and do not modify building wiring. Install requires staff permission and roughly one doorway mount; no visitor-facing camera or microphone.

### Power

Nodes are USB-powered, not battery operated, because reliable continuous counting matters more than wireless battery life. Hub and display require a separately rated supply; measure actual current and thermal behavior in the chosen enclosure. No runtime or energy-saving claims are made.

### BOM

Per-node candidate BOMs are in `hardware/bom/`. Prices are rough USD estimates at low quantity, not quotes; verify manufacturer revisions, logic levels, and availability before purchase. Generic development boards and optical sensors have variant-dependent specifications.

## Software

Firmware is C reference code with portable shared framing and host tests; vendor SDK integration remains unbuilt. FastAPI provides a local queue estimate endpoint and event ingestion skeleton; MQTT is optional and disabled unless credentials/TLS are explicitly provisioned. Synthetic ML script demonstrates a moving-average baseline only; it has no real queue data or validated performance. React Native app is an unbuilt scaffold. See `docs/architecture.md`, `docs/protocol.md`, and `docs/api.md`.

## Build and verification status

1. Review the pin maps and component compatibility notes before wiring.
2. Run `gcc -std=c11 -Wall -Wextra -Werror firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/queuekind-test && /tmp/queuekind-test`.
3. Install dashboard requirements in an isolated environment; run its tests and start the API locally.
4. Pair only owned nodes. Verify a known arrival and completion sequence, then test duplicate and stale events.
5. Disconnect internet and restart the hub; local operation should remain available after the implementation is integrated.

| Area | Status |
|---|---|
| Shared codec / host test | Host-validated |
| Node firmware | Reference skeleton; vendor SDK build not performed |
| Backend | Local API skeleton; deployment and MQTT security not production-ready |
| ML | Synthetic demonstration only |
| Mobile | Scaffold, not built |
| Schematics | Pin/net notes only; KiCad capture pending |
| Physical prototype | Not built or field-tested |

## Privacy, safety and limitations

No images, audio, names, device identifiers tied to visitors, or individual trajectories are collected. The hub aggregates counts into five-minute buckets; raw timestamps should be deleted within 24 hours, aggregate data retained locally for at most 30 days, and optional remote sync requires site consent and authenticated TLS. Provide a visible “estimate unavailable” fallback and staff-controlled pause. Queue estimates must never influence clinical priority or emergency response. The interface must not expose staff identities or performance rankings. Site signage and accessibility review are required before public use. This prototype has no safety, privacy, accessibility, or security certification; a pilot must be supervised by the site operator.

Author: jayis1.

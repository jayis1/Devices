# ToolShare — neighborhood tool-library coordination

**Inventor: jayis1.** ToolShare helps volunteer-run tool libraries answer the ordinary question “is the drill available, and when can I collect it?” without tracking borrowers or needing a cloud connection at the point of service.

## User value

Primary user: a volunteer steward at a neighborhood tool library; daily moment: confirming a tool’s availability before a resident makes a trip. Today this often means paper sheets, texts, and repeated visits. ToolShare shows stock and return status; it does not authorize loans, identify borrowers, or operate a lock. Three benefits: check stock before walking over; find an item’s labeled shelf bay; reconcile a return at the intake station. The multiple nodes are necessary because shelf presence, intake events, and shared coordination happen in different physical locations. Non-goals: automated lending, payment, surveillance, or safety inspection.

## System at a glance

```text
Shelf node A (load cells) ─┐
Shelf node B (load cells) ─┼─ BLE advertisements/GATT ─ ToolShare Hub ─ local Wi-Fi ─ FastAPI/MQTT
Intake node (button + tag) ┘                                      ├─ Mobile web/app client
                                                                  └─ optional ML, offline
```

Three distributed nodes: two shelf scales (ESP32-C6, HX711/load cell) and an intake/check-in station (ESP32-C6, buttons, NFC reader). The hub (Raspberry Pi Zero 2 W) collects node reports via BLE and caches inventory in SQLite; MQTT is optional for remote synchronization. Nodes never actuate locks or infer borrower identity. Wi-Fi/cloud loss leaves local inventory and intake available; each node buffers the latest event and retries with bounded backoff.

## Architecture and interfaces

- Shelf node: ESP32-C6 module; 5 kg load cell + HX711 on GPIO4/5; status LED GPIO8; user tare button GPIO9. 5 V USB supply; 3.3 V logic; HX711 breakout must be verified for logic compatibility before assembly. Load cells measure shelf-bay weight, not individual tool identity.
- Intake node: ESP32-C6; PN532 NFC module over I²C (GPIO4 SDA, GPIO5 SCL), two momentary buttons GPIO8/9, status LED GPIO10. 5 V USB input with regulated 3.3 V logic. NFC tags identify assets only; no personal tags are accepted.
- Hub: Raspberry Pi Zero 2 W, BLE, Wi-Fi, local SQLite; 5 V regulated USB supply. It provides the local API and optional MQTT bridge.
- Link: BLE GATT, short-range and low-power for indoor nodes; hub scans known node IDs. Each packet includes protocol version, device ID, sequence, event type, integer value, and CRC-16/CCITT-FALSE. BLE pairing/bonding is required; CRC detects corruption, not malicious tampering. Do not deploy outside a trusted test network until authenticated application-layer messaging and key lifecycle are implemented.
- Data: tool asset ID and coarse shelf weight/state only. No audio, video, borrower identity, or raw NFC UID retention. Local event log retention default 30 days; remote sync is opt-in.

## Power and operating assumptions

Shelf nodes use USB power to avoid unrealistic battery claims for continuously sampled scales. Intake uses USB power. Hub uses a certified 5 V USB supply. Per-node current and thermal behavior depend on selected boards and peripherals and must be measured on assembled hardware; no runtime or cost claim is made. Provide strain relief, protected cable routing, and a stable shelf platform. Calibration weights and load-cell capacity must suit each bay.

## Software and ML

FastAPI backend exposes the documented local endpoints. MQTT is an optional bridge; broker authentication/TLS must be configured for remote use. SQLite is the source of truth while offline. Mobile app is an Expo client scaffold showing inventory and intake actions; no borrower account is required. The ML pipeline is an optional synthetic-data demonstration for spotting unusual weight drift; it is not trained on real library data and must not decide whether a tool is available. Simple configurable thresholds remain the fallback.

## Build and bring-up

1. Build one shelf node and verify HX711 readings with known test masses; record empty-bay baseline and stable asset mass.
2. Build second shelf node; confirm unique node ID and non-overlapping BLE identity.
3. Build intake station and test a locally provisioned asset tag and both buttons.
4. Start hub services using `software/dashboard/README.md`; pair only owned nodes.
5. Check `/health`, post a synthetic event, and confirm inventory via `/inventory`.
6. Test Wi-Fi loss and hub restart; confirm node reconnect and duplicate sequence rejection.
7. Validate that weight variation, neighboring shelf movement, and load-cell creep do not produce misleading availability before public use.

## Safety, privacy, and security

Low-voltage advisory inventory system only. It does not inspect tools for condition, prevent unsafe use, or replace a checkout policy. Keep electronics away from damp floors and do not use this prototype to support load-bearing shelves. NFC reads must be limited to issued asset tags. Use unique node IDs, BLE bonding, local network isolation, and TLS/authenticated MQTT when enabled. Current prototype packet CRC and BLE transport are not a complete security design; public deployment requires authenticated messages, replay protection, key revocation, access controls, and security review.

## Implementation status

| Area | Status |
|---|---|
| Portable protocol codec and host test | COMPLETE — compiled and exercised |
| Per-node C source | SKELETON — hardware SDK integration unbuilt |
| FastAPI local inventory API | SKELETON — tested locally; MQTT adapter configuration required |
| ML synthetic drift example | SKELETON — synthetic only, no real evaluation |
| React Native client | SKELETON — unbuilt scaffold |
| Electrical design | SPEC-ONLY — pin/net notes, not manufacturing-ready KiCad |
| BOM | SPEC-ONLY — candidate parts; procurement and electrical compatibility verification required |

## Limitations

No physical prototypes, electrical-rule checks, field trials, or certified safety/security review have been performed. BOM prices are omitted because sourcing and board variants change. This is a design prototype, not a production-ready lending or inventory system.

# PantryPath

Design status: reference prototype; not a food-safety system. PantryPath helps volunteer-run community fridges coordinate shelf availability and restocking without tracking visitors or claiming food is safe. It focuses on anonymized shelf weight-change events and human-confirmed restock actions at shared refrigerators, distinct from recipient intake or behavior tracking.

## Scenario and nodes

A volunteer checking a community fridge wants to know which shelf needs attention before making a trip. Two shelf scales and a fridge sentinel report coarse changes and temperature context to an on-site gateway. A volunteer confirms item categories and dates at an intake dock; only that human-entered record changes stock status. No camera, microphone, user identity, access control, or food-safety decision is included.

| Node | Components | Power/link | Role |
|---|---|---|---|
| Site Hub | Raspberry Pi Zero 2 W, RFM95W regional ISM module, SQLite | USB 5 V; Sub-GHz star, optional Wi-Fi | Local view, deduplication, optional MQTT/TLS uplink |
| Shelf Scale ×2 | ESP32-C3-MINI-1, NAU7802, 5 kg load cell, RFM95W | USB 5 V; radio | Coarse stable-weight-change events; no item recognition |
| Fridge Sentinel | ESP32-C3-MINI-1, TMP117, reed switch, RFM95W | protected low-voltage supply; radio | Temperature and door observations |
| Intake Dock | RP2040, UART barcode scanner, encoder/buttons, RFM95W | USB 5 V; radio | Volunteer-confirmed category, quantity and date |

Radio module, antenna, supply details, and legal frequency must be selected for jurisdiction. See BOM and pin/net notes. Parts require datasheet, ERC, RF, enclosure, and power review before fabrication. No mains wiring or autonomous refrigeration control.

## Data flow and offline behavior

```text
Shelf load cells ─┐
Door/temp sensor ──┼─ Sub-GHz star → Site Hub/SQLite → optional MQTT TLS → API → app
Volunteer dock ───┘                       └─ local operation without internet
```

Human confirmation is authoritative: weight changes only create review suggestions. Duplicate and stale sequences are rejected; latest values display age and quality. Gateway outage leaves nodes sampling with bounded buffering; expired observations are discarded, never replayed as actionable stock changes. No remote actuator commands exist. Temperature readings are context only; operators follow applicable food-safety procedures and calibrated instruments.

## Privacy, security, and limits

No visitor data or personal identifiers. Store coarse sensor history locally for configurable retention (suggested 14 days); retain volunteer audit events per site policy. Production needs unique node keys, authenticated MQTT over TLS, topic ACLs, enrollment/revocation, and signed updates. Example CRC is error detection only; production crypto/provisioning are not implemented. ML uses synthetic data only and is not validated. Mobile app is a UI stub. Not a certified food-safety system.

## Build status

Includes C framing/state examples, BOMs, pin/net design notes (not native KiCad), FastAPI+MQTT scaffolding, synthetic ML script, React Native stub, and calibration/deployment notes. No physical hardware, target SDK, ERC, RF, safety, security, or production mobile validation is claimed. See `docs/`.

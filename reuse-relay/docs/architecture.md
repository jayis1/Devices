# Architecture and design rationale

## Selection
Existing registry spans home automation, health, community facility readiness, shared inventory (ToolShare), and waste sorting (WasteSort). This system targets a distinct workflow: time-bounded reuse of residents' unwanted move-out items within a building, with physical staging and pickup confirmation. Not a waste classifier or general lending library.

## Requirements trace
- Multi-node sensing and coordination: Staging/Pickup ESP32-C6 nodes report BLE telemetry to Hub; test protocol round-trip.
- Human-approved offers: local API accepts staff-confirmed entries; unit tests cover expiry and duplicate events.
- Outage tolerance: hub operates local-only, queues bounded optional sync; verify API without broker/cloud.
- Privacy: no identity, image, apartment number or audio field in schema; inspect API model.

## Components and interfaces
Hub: Pi Zero 2 W, local SQLite in production, USB-C powered, Wi-Fi uplink optional. BLE GATT is the intended node transport; firmware sample uses a transport-independent packet codec and does not implement BLE. Staging node measures load-cell changes and shelf reed-switch; Pickup node measures pickup shelf weight and a staff confirmation button. ESP32-C6 module, 3.3 V peripherals. HX711 10 SPS/80 SPS module selected; calibration and mechanical load limits must be determined for actual platform. Contact input uses internal pull-up and switch to ground. One common ground; battery divider only sampled while enabled to reduce drain.

No node independently publishes a resident offer. Event indicates “check shelf”; a person creates/edits/removes the offer. Presence signals are noisy and must never be interpreted as a particular item identity. Local hub maintains authoritative offer state and expires entries after 48 hours.

## Operation / degradation
Hub restart recovers local database and rejects expired offers. Network loss disables optional remote sync only. Node loss displays stale/offline after 5 minutes; manual staff check remains possible. Radio retry is bounded with jitter; events use random event IDs and are idempotent at the hub. No actuators, access control, or unattended decisions.

## Product constraints and readiness
Install removable sensor platforms only at a staff-managed shelf; no doorway or egress monitoring. Staff must manually inspect/reject unsafe goods. Optional cloud sync must use authenticated HTTPS/MQTT TLS, per-site authorization and deletion controls; these are requirements, not implemented. Prototype BOM estimates are indicative; PCB, enclosure, RF, charging and mechanical design need qualified review. Node pin mapping is provisional pending exact module datasheet confirmation. Firmware target integration and app release are not complete.
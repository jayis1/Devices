# CommonShelf — community pantry stock and cold-chain coordination

Status: reference prototype; hardware is an unverified design concept, not a food-safety monitor. Author: jayis1.

CommonShelf helps volunteer-run neighborhood pantries coordinate shelf-stable stock and identify refrigerator temperature excursions without tracking recipients. It complements PantryLink (site inventory) with a narrow, multi-node shared-fridge workflow: weigh shelves locally, log fridge temperature/door events, and let volunteers confirm intake and restock actions. A phone-only spreadsheet misses unpowered/unstaffed periods; one sensor cannot cover separated shelves and refrigeration; manual counts remain the authority. Estimated volunteer tally time saved: 5–15 min per shift (planning estimate, not measured).

## Nodes and topology

Two battery-backed shelf scales and a refrigerator sentinel send signed-by-installation identity telemetry over BLE 5 advertisements to a Raspberry Pi Zero 2 W gateway; the gateway batches data to FastAPI/MQTT over TLS when internet is available. Intake dock is the human-confirmation interface (USB-connected NFC reader/button); it does not identify recipients.

```
Shelf scale A/B (ESP32-C3 + load cell/HX711) --BLE--> Pi gateway --MQTTS--> API --> volunteer app
Fridge sentinel (nRF52840 + TMP117 + reed) -----BLE------^            ML (trend demo)
Intake dock (RP2040 + button/NFC) ------USB------^
```

Prototype radio advertises compact measurements; no mesh/range guarantee. BLE pairing/provisioning and authenticated encryption are deployment requirements; this reference code does not implement production key management. Nodes cache last readings and retry with jitter. Gateway performs local threshold alarms and buffers bounded telemetry during outages; cloud loss never disables weighing or local alarms.

## Hardware and interfaces

| Node | Main parts | Interface/pins (reference allocation) | Power |
|---|---|---|---|
| Shelf scale ×2 | ESP32-C3-MINI-1, HX711, 5 kg bar load cell, 3.3 V buck | HX711 DOUT GPIO4, SCK GPIO5; BLE 5 | 5 V USB; load cell 5 V excitation, analog ground star |
| Fridge sentinel | nRF52840 module, TMP117 (0x48), reed switch | I²C SDA P0.26/SCL P0.27; reed P0.11; BLE | 1-cell protected Li-ion, USB charge module; 3.3 V regulated |
| Intake dock | RP2040, button, optional PN532 NFC | I²C SDA GP4/SCL GP5; button GP14; USB serial | 5 V USB; no recipient card data stored |
| Gateway | Raspberry Pi Zero 2 W | BLE + Wi-Fi; MQTT TLS | certified 5 V supply |

Pins are design allocations, not checked against selected module revisions; verify datasheets and board schematics before fabrication. Load cells require calibration with known masses. BOM CSVs list reference parts and estimated costs; availability and alternates require sourcing review. Schematics are text pin-map design notes, not KiCad schematics or fabrication files.

## Data, API, and local behavior

Versioned JSON payload: `v,node,seq,ts,kind,value,unit,battery_mv`; accepted kinds: `mass_g`, `temp_c_x100`, `door`. API validates bounds and topic `commonshelf/{site}/{node}/telemetry`; commands are intentionally absent. Broker must enforce per-site ACLs and TLS; sample backend is development-only. Keep raw telemetry 30 days by default, then aggregate; site opt-in cloud sync, no identity/recipient data, no audio/video. Volunteers control enrollment and revocation.

Temperature thresholds are configurable advisory thresholds only, not proof that food is safe. On stale data, show UNKNOWN; never infer safe stock. Door-open/temperature warnings are locally visible at gateway. Human staff inspect, quarantine, and follow local food-handling rules. On sensor failure or battery depletion, alert and retain last value with timestamp; never display it as current.

## Day in a shift

- 07:00: volunteer sees fridge sentinel online and confirms its current reading against a calibrated independent thermometer.
- 07:10: shelf trend highlights low-stock bins; volunteer performs a visual count and records corrections.
- 12:00: internet outage; gateway buffers updates and local display continues to show node freshness.
- 16:00: an excursion is flagged; volunteer checks the appliance and applies local food-safety procedure, not an automated decision.

## ML and mobile

ML pipeline is an illustrative synthetic-data anomaly baseline only. It has no validated food-spoilage labels or safety performance and cannot make discard/serve recommendations. Threshold rules remain the transparent fallback. Mobile app is a small React Native/Expo status prototype; authentication, provisioning, accessibility review, and production builds are incomplete.

## Build/checks

See `docs/architecture.md`, `docs/protocol.md`, `docs/api.md`. FastAPI dev run: `uvicorn app:app --app-dir software/dashboard`. Protocol host test: `python3 -m unittest discover -s firmware/common/tests`. Train demo: `python3 software/ml-pipeline/train.py --out /tmp/commonshelf-model.json`. Do not deploy without completing threat review, secure enrollment, electrical verification, calibration, and food-safety workflow testing. No medical, food-safety, or regulatory certification is claimed.

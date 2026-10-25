# MealBridge — shared-kitchen coordination without tracking people

MealBridge helps a shared home avoid duplicate grocery purchases and food silently expiring. A kitchen hub combines anonymous bin-weight changes with explicit item check-ins; a fridge shelf node tracks load changes and temperature. A pantry display/button lets residents claim, reserve, or flag shared staples. It reports inventory confidence and expiry reminders, never infers who ate anything.

## Architecture

```
Shelf scale (ESP32-C3, load cell) ── ESP-NOW ──┐
Fridge sentinel (ESP32-C3, temp) ── ESP-NOW ──┼─ Kitchen hub (ESP32-S3) ─ MQTT/TLS ─ API
Pantry button/display (ESP32-C3) ── ESP-NOW ──┘                         └─ React Native client
```

Three low-voltage nodes plus hub; deployment does not control refrigeration or appliances. Hub rules remain usable offline; cloud and optional ML only improve household sync and demand estimates. Users explicitly name/confirm items; weight changes are suggestions, not identity or food-safety evidence.

## Hardware (prototype concept; not production-ready)

All nodes use ESP32-C3-MINI-1 modules (3.0–3.6 V, 2.4 GHz Wi-Fi/BLE; ESP-NOW prototype transport). Hub uses ESP32-S3-WROOM-1-N8. No mains wiring. Pin mapping and BOM are in `docs/hardware.md`; each schematic directory has a netlist/pin assignment, not a validated KiCad schematic. Power via certified 5 V USB supplies; battery operation is not specified.

## Protocol and software

Versioned JSON telemetry: `v`, `node_id`, `seq`, `ts_utc`, `kind`, `value`, `unit`. ESP-NOW frames are bounded and authenticated at the application layer in a production design; this prototype parser rejects malformed values and duplicate sequence numbers. The backend exposes health, node registration and telemetry endpoints, with SQLite persistence. MQTT deployment is an integration stub: configure broker TLS and per-hub credentials before use. ML training uses generated synthetic consumption data and is demonstrative only. Mobile app is a UI/API stub.

## Privacy, security, safety

No camera, microphone, occupant identifier, or inferred eater. Keep raw telemetry on the hub by default and transmit only household inventory events when enabled. Provide household consent, deletion, and key rotation before deployment. Never put production credentials in source. Shelf scales require calibration and do not certify package contents; temperature readings do not establish food safety. No appliance actuation. Prototype firmware, RF behavior, food-contact materials, EMC, and regulatory compliance are unvalidated.

## Build / validation

Run `python3 -m unittest discover -s software/dashboard/tests` and `python3 software/ml-pipeline/train.py`. Python backend uses FastAPI and SQLite (`software/dashboard/requirements.txt`). Firmware is portable C protocol logic; target board SDK integration and flashing remain TODO. Calibration: place a known mass on each shelf platform and record zero/span in local configuration; verify with at least three masses.

## Cost target

Estimated prototype BOM: hub $25, shelf scale $24, fridge sentinel $12, pantry display/button $22; total approximately $83 before enclosure, shipping, and assembly. Retail target below $180 is aspirational, not validated.

## Validation status

Software checks are executed in the repository run; no physical prototype, KiCad ERC, RF range, model quality, app build, security audit, or food-safety validation is claimed. This is an assistive inventory tool, not a food-safety system.

Invented and maintained by jayis1. MIT license, consistent with repository.

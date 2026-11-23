# ClosetLoop — local-first clothing care and repair coordination

Status: reference prototype specification; electronics unbuilt and not field-tested. Invented and maintained by [jayis1](https://github.com/jayis1).

ClosetLoop reduces the daily friction of finding, caring for, and repairing household clothing. It combines explicit garment check-ins with laundry-cycle observations and a repair dock. It does not infer who wore an item, use cameras/microphones, or claim to identify fabric or diagnose damage. The owner controls garment labels and retention.

## System journey
A household member taps a passive NFC garment label at the wardrobe dock to record a voluntary “worn” or “needs repair” event. A laundry node reports washer/dryer vibration and door state (observation only). The hub merges timestamped events into a household-owned care queue and the React Native app displays explicit tasks. Repair-dock buttons record “mended” or “retired.” No appliance is controlled.

## Nodes and interfaces
| Node | SoC / parts | Interface and reference pins | Power |
|---|---|---|---|
| Home hub | Raspberry Pi Zero 2 W, USB BLE adapter, Wi-Fi | USB host; Wi-Fi; BLE 5 | certified 5 V USB-C supply |
| Wardrobe dock | ESP32-C3-MINI-1, PN532 NFC reader, 2 buttons, status LED | I²C SDA GPIO4/SCL GPIO5; IRQ GPIO6; buttons GPIO7/8; LED GPIO10 | 5 V USB, regulated 3.3 V |
| Laundry observer | ESP32-C3-MINI-1, LIS3DH accelerometer, reed door sensor | I²C SDA GPIO4/SCL GPIO5; reed GPIO6 | USB 5 V; isolated from appliance circuitry; no mains connection |
| Repair dock | nRF52840 module, 3 buttons, e-paper status display (optional) | buttons P0.11–13; SPI display SCK P0.13/MOSI P0.15 (display omitted in minimal build; pin conflict resolved by alternate P0.17); BLE 5 | USB 5 V to 3.3 V regulator |

Pin assignments are proposed module-level choices and require board-datasheet review before PCB layout. Gateway is the coordinator; nodes use BLE GATT advertisements/connection for event delivery. Hub persists events locally and synchronizes to backend over MQTT when configured. Essential task list works without internet. No automatic garment identification beyond user-tapped NFC UID.

## Event contract
Version 1 JSON: `{version,node_id,seq,event_id,kind,item_id,utc_ms,quality}`. `kind` is `worn`, `needs_repair`, `washed`, `drying_complete`, `mended`, `retired`, `door_open`, or `vibration_cycle`; item IDs are random household-local identifiers. Event ID is UUID; node sequence is monotonic uint32. Duplicate event IDs are idempotently ignored; unknown versions/kinds and payloads over 512 bytes are rejected. Hub receipt time replaces missing device time and marks time quality uncertain. MQTT topic `closetloop/<random-household-id>/events`; TLS and per-device credentials required in deployment, not supplied here. Manual enrollment is prototype-only; cryptographic provisioning is not implemented.

## Software
- `firmware/`: ESP-IDF C node examples and shared C protocol validation.
- `software/dashboard/`: FastAPI event ingest/status API with SQLite; MQTT adapter is documented as optional, not production-secure.
- `software/ml-pipeline/`: illustrative baseline for estimating time-to-care from explicit event histories; synthetic data is smoke-test data only, not evidence of accuracy. Rule-based queue is fallback.
- `software/mobile-app/`: React Native stub; household-local event review and explicit check-ins.

## Power, BOM and safety
All nodes are low-voltage USB-powered. Laundry sensor attaches to the exterior cabinet only; never open an appliance, connect to mains, or infer electrical safety. The vibration observer cannot guarantee cycle completion. Button input is debounced; lost connectivity queues bounded events locally. Manual correction remains available. Estimate prototype electronics: hub US$25–45, wardrobe dock US$12–25, laundry observer US$8–16, repair dock US$15–30; unquoted estimates, exclude enclosure and labor.

## Validation/readiness
This repository contains a prototype design, not a build-ready certified product. Verify schematic/pin choices against exact module revisions, BLE pairing/privacy, enclosure and adhesive durability, and accessibility in a supervised household pilot. Firmware compilation, RF range, physical reliability, predictive performance, and mobile-device builds have not been validated. No personally identifying data or raw sensor traces are required; users can delete local history and disable cloud sync.

## Build outline
Install ESP-IDF 5.x for ESP32-C3; build from each firmware node directory with `idf.py build`. Backend: `python -m venv .venv && . .venv/bin/activate && pip install -r software/dashboard/requirements.txt`; start with `uvicorn main:app` from dashboard directory. App: install dependencies with npm and run Expo. These are reference instructions; verify against target toolchains before fabrication.
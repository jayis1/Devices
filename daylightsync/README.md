# DaylightSync

DaylightSync helps households and small shared workspaces use window daylight without persistent glare. It measures room-side illuminance and sun-facing outdoor conditions, then offers a bounded shade-position recommendation. A user may enable automatic positioning on a low-voltage, mechanically limited shade; the physical wall switch always overrides automation. It is not a building-code, eye-health, or energy-savings product.

## System and user journey

A window sentinel reports indoor lux and temperature, an outdoor node reports illuminance and weather context, and a shade node reports position and accepts a short-lived position request. The gateway correlates readings, applies a transparent hysteresis rule, stores a local history, and serves the phone dashboard. With internet absent, local sensing, rules, manual control, and history continue. Cloud sync is optional and disabled by default.

Four roles: gateway (Raspberry Pi Zero 2 W, Linux/Python); window sentinel (ESP32-C3, VEML7700 lux sensor, SHT31); shade node (ESP32-C3, AS5600 position sensor, DRV8871 H-bridge for a 12 V DC gearmotor); outdoor node (nRF52840, VEML7700, SHT31, rain switch). Nodes exchange CBOR-like compact JSON-compatible telemetry over authenticated BLE GATT to the gateway; MQTT is gateway-to-cloud only. This reference prototype uses BLE, not a proprietary mesh.

## Design parameters and safety boundary

Prototype target: one gateway and up to 12 nodes; 1 Hz sampling, 10 s normal telemetry, stale after 60 s; room lux target is user-configured (default 500 lx) with 15% hysteresis. Automatic shade movement is limited to 10 s per command and 60 s/hour, is disabled until explicitly enabled, and stops on limit switch, overcurrent, lost communication, or wall-switch input. The driver interface is intended for an isolated SELV motor supply only. Do not connect mains, fire/security equipment, or a window-opening mechanism. Use a mechanically limited shade and a certified motor supply. Hardware design, EMC, radio range, and mechanical safety remain unvalidated; this is not fabrication-certified.

Power: Pi gateway uses regulated 5 V / 2.5 A USB supply. Sensor boards use 3.3 V; outdoor node uses a protected 18650 cell and a solar charger sized after measured seasonal energy budget (no runtime claim). Shade motor uses a separately fused 12 V supply; logic and motor grounds join at the driver return, with bulk capacitance and transient suppression at the motor. Motor wiring, fusing, thermal behavior, and pinch hazards require bench review.

## Repository map

- `firmware/`: portable C protocol and node sampling/control state machines.
- `schematic/`: per-node connection and pin-assignment design notes; these are not KiCad schematics and are not fabrication files.
- `hardware/bom/`: per-node CSV bills of materials.
- `software/dashboard/`: FastAPI local gateway API and tests.
- `software/ml-pipeline/`: reproducible baseline, synthetic-only demonstration for daylight trend estimation; no real-world accuracy claim.
- `software/mobile-app/`: React Native integration contract and stub screen.
- `docs/`: protocol, threat model, and architecture.
- `scripts/`: local development and calibration guidance.

## Build and verification

Run `cd software/dashboard && python3 -m unittest discover -s tests -v`. Firmware is portable C intended to be built with a C11 compiler; run `sh scripts/test-firmware.sh` from this directory. The source describes interfaces but has not been built against vendor SDKs or exercised on hardware. No mobile build or ML validation is claimed. Use only de-identified, locally collected lux samples for future model work.

Invented and maintained by jayis1. Prototype reference design; electrical and field validation outstanding.

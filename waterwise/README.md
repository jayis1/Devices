# WaterWise — household water-use coach

WaterWise helps households notice avoidable water waste without controlling plumbing. A pipe-side flow node measures aggregate cold-water flow, a shower-side temperature node estimates when usable hot water arrives, and a local hub combines readings into privacy-preserving fixture-session estimates and practical coaching. It is advisory: no valve actuation, leak-safety claims, or health claims. Existing systems address leak damage, drain health, and well-water quality; WaterWise instead focuses on everyday water-use feedback and hot-water wait waste.

## System
- Flow Node: ESP32-C3, clamp-on ultrasonic flow sensor interface (prototype transducer module), pulse output fallback, temperature probe; USB-C 5 V SELV supply.
- Temperature Node: nRF52840, two DS18B20 probes on pipe surface (insulated contact), BLE 5.x advertisement with sampled temperatures.
- Hub Gateway: Raspberry Pi Zero 2 W, BLE scanner, SQLite store, local FastAPI/MQTT bridge; optional cloud export disabled by default.
- Mobile: React Native status/weekly review stub.

Nodes communicate using BLE advertisements carrying versioned compact telemetry; hub publishes validated readings to MQTT topic `waterwise/v1/<node_id>/telemetry`. No personal identity or audio/video is collected. Local data retention defaults to 30 days and can be deleted by the household.

## Interfaces and safety
Protocol JSON reference: `docs/protocol.md`. Flow is litres/minute (L/min), temperature is °C, timestamps are UTC epoch seconds. Duplicate sequence numbers are discarded; stale readings older than 120 s are marked unavailable. Hub remains useful offline. Sensors are observational; installation must not modify, pierce, or obstruct plumbing. Clamp-on flow sensing requires calibration on the actual pipe and is not a billing instrument. Low-voltage electronics only; keep mains power supplies certified and physically separated from wet areas.

## Power and design limits
Flow node 5 V USB, nominal <250 mA (sensor module dependent); temperature tag 2×AA with target months-long life, to be measured; hub 5 V USB, typical <1 A. These are design estimates, not measured budgets. Replace batteries when low; missing nodes are shown as stale rather than imputed. Hub outage causes local sensing to continue but no history; internet outage has no effect. No actuator commands exist.

## Build contents
- `firmware/`: portable C telemetry framing and node sampling examples.
- `schematic/`: pin/connection design notes; not native KiCad schematics, not ERC checked.
- `hardware/bom/`: illustrative per-node BOMs.
- `software/dashboard/`: FastAPI validation/API and MQTT publishing adapter.
- `software/ml-pipeline/`: synthetic-data demonstration only; no accuracy claim.
- `software/mobile-app/`: UI contract stub.

This is an engineering concept, not a validated product. SoC pin assignments and sensor interfaces require confirmation against the selected board revision; no physical prototype, calibrated flow accuracy, battery life, radio range, or regulatory compliance has been demonstrated. Firmware examples are portable C and have not been built for target hardware. ML is optional and must not trigger control actions.

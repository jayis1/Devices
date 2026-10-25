# LiftLink — accessible transit lift status

Status: design-stage reference prototype; not validated or safety-rated.

## User need and novelty

A wheelchair user, traveler with a stroller, or person unable to use stairs may arrive at a station only to find the elevator unexpectedly unavailable. LiftLink provides station staff and riders with a timestamped, confidence-qualified advisory status and a human-confirmed accessible-route disruption. It addresses transit elevator availability, distinct from CurbCue's sidewalk hazards and QueueKind's service queue estimates. Closest alternatives are transit agency service alerts and elevator-monitoring vendors; LiftLink's proposed delta is low-cost local sensing plus a public, consent-free status endpoint. No benchmark or cost comparison has been validated.

Success hypothesis: reduce time from lift outage onset to staff awareness; measure onset-to-acknowledgement against a manually logged pilot baseline. Do not use LiftLink as the sole basis for route planning.

## System overview

```text
Lift Sentinel A/B (door-state + vibration, read-only)
             | BLE GATT, site-scoped pairing
             v
Site Hub (local status cache, queue, e-paper) ---- Wi-Fi/TLS ---- FastAPI + MQTT
             ^                                               |             |
Landing Beacon (accessible report/ack button)                +-------- React Native
```

Three roles are spatially necessary: sensors sit at lift locations; the hub is in staffed/covered space and buffers offline; the landing beacon provides a tactile, reachable, phone-free human report. BLE star only, not mesh. Sensors do not connect to or control elevator circuits. Site Wi-Fi failure leaves local indicators and queued events available.

## Hardware and interfaces

| Node | Reference components | Interface and power |
|---|---|---|
| Site hub | Raspberry Pi Zero 2 W, USB BLE adapter, 2.9-inch SPI e-paper module | 5 V USB-C; BLE central, Wi-Fi uplink; local SQLite queue |
| Lift sentinel | XIAO nRF52840, reed switch, LIS3DH accelerometer, protected 18650 supply module | 3.3 V; I2C SDA/SCL D4/D5 (GPIO6/7), reed D2, IRQ D3; read-only, no lift wiring |
| Landing beacon | XIAO nRF52840, sealed momentary switch, LED, piezo | 2×AA regulated to 3.3 V; switch D2, LED D3, piezo D6; no actuator beyond low-voltage indicators |

Pin assignments are proposed for the named XIAO board family and must be confirmed against exact board revision before PCB work. I2C address and interrupt pull-ups follow the selected LIS3DH breakout variant. External wiring needs ESD protection and strain relief. See `schematic/` pin maps; KiCad capture and electrical review remain pending.

## Protocol and behavior

BLE event fields: version 1, site/node ID, uint32 sequence, event type, monotonic uptime ms, confidence 0–100, bounded status code. Hub rejects unknown versions, invalid ranges, duplicate `(node,sequence)`, and stale reports. Status states are `unknown`, `likely_available`, `likely_unavailable`, `staff_confirmed_unavailable`, `staff_confirmed_available`. Sensor disagreement or sensor loss means `unknown`, never available. Human reports are unverified until staff acknowledge. MQTT actuation topics do not exist; no commands to lift equipment.

Prototype BLE pairing is not a production security design. Deployment requires per-device identity, authenticated pairing, TLS MQTT, broker ACLs, revocation, and signed updates. No credentials are included.

## Power and cost assumptions

Planning estimates only, not supplier quotes: sentinel 3000 mAh protected cell, estimated 0.25 mA average => 3000/0.25/24 = 500 days ideal; apply 50% derating => about 250 days. Must measure actual current, cold behavior and radio duty cycle. Beacon 2000 mAh AA pair at estimated 0.12 mA => 694 ideal days, about 347 days at 50% derating. Hub 5 V / 2 A continuous. Approximate prototype totals: hub $35–60, sentinel $25–45 each, beacon $12–22, excluding installation. Not a validated price advantage over commercial systems.

## Software and ML

FastAPI validates event structures in a prototype in-memory service. An MQTT bridge and durable SQLite store are not implemented yet. The ML folder demonstrates a synthetic-data baseline only; no model accuracy or real-world outage detection claim is made. Deterministic rules and staff confirmation remain authoritative. Mobile app is a minimal API status client scaffold.

## Privacy, accessibility and failure handling

No camera, microphone, passenger count, identity, or location tracking. Site/node IDs only; default event retention 30 days, configurable shorter. Staff and riders see status timestamps and confidence; raw vibration samples are not uploaded. Tactile button, high contrast indicator, audible confirmation, and e-paper text need testing with disabled riders. False positive, dead battery, offline sentinel, stuck switch, and hub outage all produce stale/unknown status and visible maintenance notice. Never claim a path is accessible or safe based solely on sensor data.

## Safety and limits

Read-only external sensing; do not open, modify, or bridge elevator safety circuits. Installation near lifts requires facility approval and qualified personnel. Not an elevator controller, emergency alarm, certified accessibility system, or substitute for operator notices. No physical, RF, ingress, or accessibility validation has been performed.

## Build and verification status

Verified in this run: portable protocol host test compiled and passed with GCC; Python files passed `py_compile`; synthetic script ran on 100 samples; mobile package JSON parsed. Node firmware is scaffold-only and not hardware-built. Mobile scaffold is not yet a production app. Schematics are pin-map documentation, not KiCad capture or fabrication files. MQTT bridge, durable storage, hardware, BLE pairing, mobile build, calibration, and field pilot remain unverified.

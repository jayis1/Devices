# GlowRoute — community walkway lighting readiness

## Problem & rationale
Residents and maintenance teams often discover a dark public walkway only after a complaint. GlowRoute gives small parks, campuses and housing co-ops an auditable, privacy-preserving indication of which pedestrian lighting circuits appear unavailable, reducing time from outage to inspection. It does not claim legal lighting compliance or guarantee safe passage. Multiple pole sentinels provide spatial localization; a site hub buffers events and a gateway publishes a shared view, which a single phone or sensor cannot provide.

Different from ParkPulse (fountain/bin maintenance), FrostLine (surface ice advisories), CivicRest (facility readiness), and CivicCharge (outlet availability): this system monitors nighttime light availability along a route, with no cameras, people detection, or luminaire control.

## System
Three logical nodes: GL-1 gateway (Raspberry Pi Zero 2 W, local MQTT/API), GL-2 pole sentinel (ESP32-C3, photodiode + ambient lux sensor), GL-3 route marker (ESP32-C3, independent lux cross-check + status LED). One gateway coordinates BLE advertisements from battery-backed pole sentinels; Wi-Fi/MQTT is upstream only. Prototype reporting threshold is calibrated locally; no remote switching.

```text
Pole sentinel (BH1750 + ESP32-C3) -- BLE adv --> Gateway (Pi Zero 2 W)
Route marker (VEML7700 + ESP32-C3) -- BLE adv --> Gateway -- MQTT --> FastAPI --> App
```

### Privacy, safety, limitations
No imaging, audio, identity, or location tracking. Raw readings stay on-site; backend example retains latest value in memory only. Gateway remains useful offline and queues nothing in this demo. Alerts are inspection prompts, never safety certification. SELV USB power only; no mains wiring or luminaire control. Prototype, not a certified public-safety product. Install only with site-owner approval; calibrate per luminaire geometry and seasonal conditions.

## Interface and power
Protocol v1 JSON: `v`, `node`, `seq`, `lux`, `uptime_s`; lux is nonnegative illuminance in lx, sequence is unsigned monotonic per boot, MQTT topic `glowroute/v1/telemetry/<node>`. BLE demo payload and shared C parser use a fixed binary frame (version, node id, sequence, lux milli-lux, CRC-16/CCITT); backend API uses JSON.

| Node | Supply / estimated load | Interfaces | Pin assignment |
|---|---|---|---|
| GL-1 | 5 V USB-C, 0.5–1.2 A peak (Pi; estimate) | Wi-Fi, BLE | Pi Zero 2 W; no external sensor wiring |
| GL-2 | 3.3 V LiFePO4 protected pack, 10–25 mA active, target <100 uA sleep (not measured) | I2C, BLE | ESP32-C3 GPIO4 SDA, GPIO5 SCL; GPIO8 status LED |
| GL-3 | 3.3 V USB/pack, similar estimate | I2C, BLE | ESP32-C3 GPIO4 SDA, GPIO5 SCL; GPIO8 status LED |

Use 4.7 kΩ I²C pull-ups to 3V3, 100 nF local decoupling, protected battery module with regulated 3.3 V, and accessible USB-UART programming pads (3V3/GND/TX/RX/EN/BOOT). Verify exact module pinout before PCB layout; GPIO8 LED must not interfere with strapping on selected module. Battery life and thresholds are engineering targets, not measured claims. BOMs are preliminary procurement estimates.

## Getting started (simulation)
Python 3.10+: `cd software/dashboard && python -m pip install -r requirements.txt && uvicorn app:app --host 127.0.0.1 --port 8000`. `python -m pytest` runs backend checks. Publish JSON to `/telemetry` via API; `/telemetry` GET returns latest reports. Firmware is portable C protocol reference, not a complete ESP-IDF target. App is a presentation stub. ML script demonstrates synthetic baseline only. No real site data or trained model is included.

## Status
| Artifact | State |
|---|---|
| Shared protocol C + host tests | Implemented; host compiler checked |
| Sensor firmware | Minimal protocol/reading stub; no board SDK build |
| Gateway/backend | API ingestion demo; MQTT adapter not enabled by default |
| ML | Synthetic demonstration only; not a validated predictor |
| Mobile | React Native display stub; not built |
| Schematics | Pin/net design notes; KiCad electrical files intentionally not claimed |
| Hardware | Preliminary BOM and estimates; no physical validation |

## Bill of materials summary (USD estimates)
Gateway $25–45; each pole node $12–22; route marker $12–22. Approximate 2-node pilot total $49–89 excluding enclosure, installation, cellular/network, and labor. See `hardware/bom/` for part-level estimates.

## Bring-up
1. Verify the module datasheet, rail, antenna clearance and I2C address before fabrication.
2. Flash each ESP32-C3 with a board-specific ESP-IDF project (not supplied); confirm boot and sensor readings over serial.
3. Calibrate against a traceable lux meter at installed orientation; record night baseline and failure threshold.
4. Start API locally, post sample telemetry, confirm node and sequence; set up private authenticated MQTT only after adding TLS and per-node credentials.
5. Walk the route at night and compare every alert with direct human inspection. Do not publish public status until false positives and coverage have been evaluated.

## Failure behavior
Sensor absent/out-of-range → report invalid reading, no outage assertion. BLE loss → gateway marks last-seen stale; it does not infer darkness. Power/network loss → local measurements stop or API becomes unavailable; no actuation occurs. Replayed/out-of-order sequence should be rejected by production gateway; demonstration API currently validates basic ranges only. Updates, enrollment, authentication and retention controls remain deployment work.

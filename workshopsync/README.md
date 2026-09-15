# WorkshopSync — Home Workshop Safety and Skill Support

Designed and documented by [jayis1](https://github.com/jayis1).

WorkshopSync is a local-first, low-voltage system that helps home makers run safer, cleaner, more organized workshops. It joins a bench-mounted hub, tool docks, particulate/noise sentinels, a wearable PPE tag, and a pressure-sensing bench mat. The system turns observed conditions into explicit checklists and records—not autonomous tool control.

> **Safety boundary and validation status:** Concept/reference design; no physical, regulatory, electrical, radio, sensor-accuracy, ML-performance, or usability validation has occurred. It is not a substitute for guards, PPE, dust collection, lockout/tagout, training, or local code. It never switches mains power, defeats a tool safety feature, starts a tool, or treats a risk score as permission to operate. A qualified electrician must install any mains-adjacent sensing enclosure.

## Why it improves daily life

Home workshops are often one-person, noisy, dusty spaces where a missed guard, poor ventilation, uncharged PPE, or cluttered bench causes avoidable interruptions and injuries. WorkshopSync provides local, explainable readiness checks, post-session clean-up evidence, consumable reminders, and trends while remaining useful without the internet.

## Nodes and topology

| Node | SoC | Job | Link | Power |
|---|---|---|---|---|
| Workshop Hub | Raspberry Pi CM4 + ESP32-S3 | local broker, policy, event store, e-paper status | Ethernet/Wi-Fi, Sub-GHz | 5 V USB-C + UPS |
| Tool Dock ×N | STM32G0B1 + SX1262 | tool identity, guard/accessory check inputs, vibration/current-clamp feature telemetry | Sub-GHz TDMA | isolated 12 V |
| Air Sentinel ×N | ESP32-C6 + SX1262 | PM, VOC, temperature/humidity, dust-collector airflow | Sub-GHz TDMA | USB-C 5 V |
| PPE Tag | nRF52840 | BLE proximity, accelerometer, button acknowledgement, battery | BLE 5.3 | CR2477 |
| Bench Mat | RP2040 + SX1262 | pressure zones, clutter/load trend, emergency stop state input | Sub-GHz TDMA | USB-C 5 V |

```text
Tool Docks ─┐
Air Nodes ──┼─ Sub-GHz TDMA ─ Workshop Hub ─ local MQTT/FastAPI ─ mobile app
Bench Mat ──┘                         │
PPE Tag ─────────── BLE ──────────────┘
```

The hub operates offline: it accepts telemetry, shows stale-data status, and only offers non-binding guidance. Cloud sync is opt-in and no raw audio/video is captured.

## System contract

`docs/system-manifest.json` is the authoritative source for node IDs, pins, supplies, telemetry fields, topic names, and safety behavior. Frame encoding and validation live in `firmware/common/`. Each node publishes a versioned envelope: node ID, sequence, epoch, type, payload length, payload, CRC-32C. Hub commands carry an expiry and are rejected when stale, duplicated, malformed, or outside the local policy.

## Hardware and power design

- **Workshop Hub:** CM4 runs the local service; ESP32-S3 manages SX1262 radio/watchdog. USB-C 5 V feeds TPS2115A adapter/UPS mux, then a 3 A eFuse. S3 ↔ SX1262 SPI: GPIO11 MOSI, 13 MISO, 12 SCK, 10 NSS, 4 DIO1, 5 BUSY, 6 RESET. E-paper uses GPIO15–18.
- **Tool Dock:** STM32G0B1 measures an isolated SCT-013 current-clamp conditioner on PA0 and ADXL355 on I²C PA9/PA10; guard/accessory dry contacts enter PB7/PB8 with opto-isolation. SX1262 is SPI1 PA5/6/7, NSS PA4, DIO1 PB5. It has no mains switching path.
- **Air Sentinel:** ESP32-C6 I²C GPIO6/7 connects SCD41, SPS30, and SGP40; airflow pulse enters GPIO2; SX1262 SPI is GPIO10–13, DIO1 GPIO4. A 5 V USB-C input feeds AP2112K 3.3 V.
- **PPE Tag:** nRF52840 P0.26/P0.27 I²C BMI270, P0.13 button, P0.14 LED/haptic driver enable. CR2477 → TPS62743 3.0 V; no microphone or location capture.
- **Bench Mat:** RP2040 ADC0–ADC3 read four FSR zones through buffers; GPIO2 is an opto-isolated external e-stop *state input*; SX1262 uses SPI0 GPIO19/16/18, NSS 17, DIO1 20. USB-C 5 V → AP2112K 3.3 V.

Every board needs reverse-polarity protection, a fuse/polyfuse, local 100 nF decoupling at IC supply pins, 10 µF regulator bulk capacitance, labeled test points, and an antenna keep-out. Pin tables, connector details, and conceptual KiCad connection notes are in `schematic/`; they are not fabrication-ready designs.

## Readiness and failure behavior

1. A tool dock reports tool identity, guard contact, and a bounded vibration/current feature; it cannot authorize, start, or stop a tool.
2. The hub combines current PPE proximity, air quality, dust airflow, and bench clutter state into an advisory checklist. Missing or stale readings are shown as unknown—not healthy.
3. The user presses the PPE tag button to acknowledge a checklist. This is only a log entry.
4. During a session, air nodes issue local PM/VOC alerts and the hub recommends stopping work/ventilating. It never opens gates or controls machinery.
5. On hub, radio, cloud, clock, or model failure, endpoints continue sensing; controls fail to passive, alert-only behavior. Bench e-stop input is displayed but remains electrically independent of machine safety circuitry.

## Software, ML, and mobile

The FastAPI service exposes validated telemetry ingestion, local health, readiness cards, and immutable acknowledgement records. MQTT topic convention: `workshopsync/v1/<node_id>/telemetry` and `workshopsync/v1/<node_id>/command`; use device-scoped TLS credentials in deployment, never checked-in secrets.

The ML pipeline provides a reproducible, synthetic-data smoke baseline for anomaly scoring. It does not establish real-world detection performance. The React Native app is a deliberately small prototype screen that reads local cards; it must not be represented as an operational safety controller.

## Build and commissioning

1. Read `docs/architecture.md`, `docs/protocol.md`, and `docs/api.md` before assembly.
2. Assemble and bench-test low-voltage sensing only. A qualified professional must review all installations near mains-operated tools.
3. Provision a unique node ID and 32-byte key outside this repository; retain no default key.
4. Run `scripts/validate.py` and compile each firmware target with its vendor SDK after replacing the portable HAL adapters.
5. Start the local dashboard with environment variables from `.env.example`; test stale-data, duplicate-frame, radio-loss, and e-stop-display paths before any workshop use.

## Repository contents

- conceptual KiCad connection notes and per-node BOMs;
- portable C protocol and firmware state-machine references;
- FastAPI dashboard, synthetic ML smoke training, and React Native prototype;
- architecture, API, protocol, deployment, calibration, and validation documentation.

All repository artifacts are authored for this system by jayis1.
# MaintainSync — Predictive Home Maintenance and Repair Prevention

MaintainSync turns scattered household warning signs into early, practical maintenance work: a hub correlates appliance vibration, water/air utility behavior, thermal inspections, and consumable condition; small tags monitor equipment without cloud dependence; and an interlocked utility controller prevents only explicitly configured damage scenarios. It is designed to catch a failing refrigerator fan, blocked HVAC filter, loose washing-machine hose, degrading pump, and overheating outlet before a costly failure.

> **Safety boundary:** This is decision support, not a replacement for licensed electrical, gas, plumbing, or HVAC inspection. It must never energize mains circuits, bypass an appliance safety control, operate a gas valve, or perform repairs autonomously. The utility controller can only de-energize a *pre-approved low-voltage* accessory or close an approved water valve after a hardware interlock; every action is local, logged, and manually reversible.

## Nodes

| Node | SoC | Purpose | Link | Power |
|---|---|---|---|---|
| Maintain Hub | Raspberry Pi CM4 + ESP32-S3 | local MQTT broker, policy, encrypted event store, dashboard bridge | Wi-Fi/Ethernet, Sub-GHz | 5 V USB-C + LiFePO4 UPS |
| Condition Tag ×N | nRF52840 | vibration/acoustic features, surface temperature, leak/contact sensing | BLE 5.3 / Sub-GHz | 1000 mAh LiPo or CR2477 |
| Inspection Wand | ESP32-S3 | RGB/thermal guided inspection, QR asset registration, NFC service record | Wi-Fi + BLE | USB-C LiPo |
| Utility Sentinel ×M | STM32G0B1 + ESP32-C6 | pressure/flow/current monitoring; dry-contact output with physical key | Sub-GHz TDMA | isolated 5 V / 12 V |
| Filter & Drain Dock ×M | ESP32-C6 | differential-pressure filter state, condensate/drain level, service LED | Sub-GHz TDMA | 12 V |

```text
Condition Tags ─BLE/Sub-GHz─┐
Inspection Wand ─────Wi-Fi──┼─ Maintain Hub ─ MQTT/TLS ─ FastAPI + mobile app
Utility/Filter nodes ─TDMA──┘        │
                                     └─ local rules, optional dry-contact / water-valve interlock
```

## Daily-life workflows

1. A tag on a washing machine learns healthy spin vibration. A new bearing imbalance is scored against that machine’s baseline; the app shows a trend and an inspection checklist rather than asserting a diagnosis.
2. A Filter Dock measures fan-filter differential pressure and condensate tray level. The hub estimates days until restriction and warns before loss of heating/cooling efficiency or overflow.
3. The guided wand pairs a thermal image with a visible image, labels the asset through QR/NFC, and flags a temperature anomaly relative to nearby pixels and the asset baseline. Images remain local unless exported.
4. For a configured leak path, two independent inputs (leak contact plus abnormal flow/pressure) are required before the hardware-keyed valve output can close. The controller still cannot override a physical manual valve.
5. The hub creates explainable maintenance cards: evidence, confidence, safety class, manual checks, and a service-history record.

## Electrical design and pin assignments

### Maintain Hub
- CM4 runs Mosquitto, FastAPI, SQLite/SQLCipher and model inference. ESP32-S3 is the always-on radio/display co-processor and watchdog.
- 5 V USB-C PD sink → TPS2115A power mux (adapter/6.4 V LiFePO4 UPS) → 5 V; CM4 carrier has a 3 A eFuse. ATECC608B shares I²C with SHTC3.
- SX1262: S3 GPIO11 MOSI, GPIO13 MISO, GPIO12 SCK, GPIO10 NSS, GPIO4 DIO1, GPIO5 BUSY, GPIO6 RESET. E-paper CS/DC/RST/BUSY: GPIO15/16/17/18. E-stop/ack button GPIO21.

### Condition Tag
- nRF52840; BMI270 IMU on I²C P0.26/P0.27 (INT P0.11), TMP117 on same bus, piezo vibration preamp to SAADC AIN0, leak/contact loop P0.13, LIS3DH-compatible motion wake input P0.14.
- MCP73831 charges a protected LiPo; TPS62743 supplies 3.0 V. MAX17048 fuel gauge. Optional SX1262 uses SPI P0.20/22/24, NSS P0.25, DIO1 P0.15.

### Inspection Wand
- ESP32-S3-WROOM-1; MLX90640 thermal array and VL53L5CX ToF on I²C GPIO8/9, OV5640 camera DVP, PN532 NFC UART GPIO17/18, QR trigger GPIO0. 1.3-inch OLED uses I²C; 1200 mAh LiPo uses BQ24074 + MAX17048.
- Thermal imagery is for comparative inspection only; use non-contact measurement limits and never inspect exposed live conductors.

### Utility Sentinel
- STM32G0B1 controls only an opto-isolated, normally-open dry contact: PA8 output through TLP291. Key switch PB0 and local STOP PB1 are hard prerequisites. Output drops on reset/watchdog.
- 0–10 V pressure transducer to PA0 via divider, Hall flow pulse PB6, SCT-013 CT conditioned to PA1, two leak loops PB7/PB8. SX1262 SPI1 PA5/6/7, NSS PA4, DIO1 PB5.
- 12 V input has 1 A fuse, reverse-polarity MOSFET and TPS5430 5 V buck. Isolated mains measurement/installation is for qualified personnel only.

### Filter & Drain Dock
- ESP32-C6; SDP810 differential pressure I²C GPIO6/7, two float inputs GPIO2/3, SHT45 GPIO6/7, WS2812 status GPIO8, service button GPIO9. 12 V → MP1584 5 V → AP2112K 3.3 V. SX1262 uses GPIO10–13 and DIO1 GPIO4.

## Protocol and security

Fixed nodes use SX1262 868 MHz (EU) or 915 MHz (US) TDMA star: a 10-second superframe has hub beacon slot 0, telemetry slots 1–7, and acknowledged command slots 8–9. Tags may use BLE GATT when near the hub and fall back to radio. Frames contain version, type, 32-bit node ID, monotonic sequence, epoch, length, payload ≤96 bytes, CRC-32C, and a 16-byte HMAC-SHA256 tag. Keys are provisioned per device and never committed. Commands have expiry, nonce, policy version, and actuator feedback requirement. Details: `docs/protocol.md`.

## ML pipeline

| Model | Input | Output | Guardrail |
|---|---|---|---|
| AssetBaseline | vibration/temperature windows | normal vs novelty score | per-asset baseline; no universal failure claim |
| FilterLife | pressure, runtime, humidity | days-to-service interval | requires 7 days of data |
| ThermalSpot | thermal image + reference region | relative hotspot candidates | wand asks user to verify asset/conditions |
| LeakFusion | flow, pressure, contact events | leak likelihood | two-sensor rule before any interlock request |
| MaintenancePlan | event history and task completion | ranked, explainable task cards | never dispatches a repair automatically |

## Build and commissioning

1. Read `docs/architecture.md` and `docs/protocol.md`; have licensed tradespeople install anything connected to building services.
2. Assemble low-voltage nodes and run bench validation before attachment. Flash each C target with its vendor toolchain.
3. Provision a unique node ID and 32-byte key; do not reuse development keys.
4. Start `software/dashboard` using `docker compose up --build`, set `MAINTAINSYNC_API_TOKEN` and `MAINTAINSYNC_MQTT_URL`.
5. Register assets with the wand; collect 7–14 days of normal baseline before enabling anomaly notifications. Test every STOP/key/interlock path with live actuation disconnected.

Repository contents: C firmware and shared protocol, KiCad source placeholders with connection notes, per-node manufacturer BOMs, FastAPI/MQTT service, training baseline, React Native app starter, commissioning script, and detailed API/protocol/architecture documentation.

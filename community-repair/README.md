# CommunityRepair

CommunityRepair helps library and neighborhood volunteers keep public bicycle-repair stands usable. A rider should not arrive to find the pump hose cut or the shared hex keys missing; staff need evidence to prioritize inspection without cameras, user accounts, or claims that a bike is roadworthy. The system observes equipment presence and pump pressure response, then lets a human confirm repairs.

## Distinction

SpokeSense observes a bicycle and its maintenance; ToolShare coordinates tools in a lending library; CommunityRepair is fixed public infrastructure and reports the condition of a self-service repair stand. It neither tracks riders nor diagnoses bicycles.

## Topology and operation

```text
Tool Dock (nRF52840 + reed switches) --BLE--> Site Gateway (ESP32-C6)
Pump Monitor (nRF52840 + pressure + motion) --BLE--> Site Gateway --Wi-Fi/MQTT TLS--> optional FastAPI
                                                        | local SQLite queue / status page
```

Two battery sensor units are spatially mounted at a repair stand, while the mains-USB gateway bridges BLE and provides local buffering. Nodes advertise compact observations; gateway timestamps, deduplicates, and stores them. Internet or broker loss never disables sensing. The system has no actuators. Staff inspection and acknowledgement remain authoritative; sensor silence is UNKNOWN, never AVAILABLE.

## Hardware contract

Reference prototype components and logical pin maps; these are not fabrication-ready schematics. Verify the chosen module revision and datasheets before PCB work.

| Unit | Controller / radio | Interfaces | Supply |
|---|---|---|---|
| Gateway | ESP32-C6-WROOM-1, Wi-Fi + BLE 5 | USB-C 5 V; internal 3.3 V; I2C status OLED SDA GPIO6/SCL GPIO7; LED GPIO15 | certified 5 V USB supply |
| Tool Dock | Seeed XIAO nRF52840, BLE | Tool-present switches on D0-D5 with pull-ups; service button D6; battery ADC D7 | 1S protected 1000 mAh LiPo, USB charging module |
| Pump Monitor | Seeed XIAO nRF52840, BLE | 0-1.2 MPa analog pressure transducer via divider to A0; pump lever reed D1; accelerometer LIS3DH I2C D4/D5 | 1S protected 1000 mAh LiPo |

Use normally-open switches to ground with pull-ups, input debounce and connector strain relief. Pressure sensor must be a rated, sealed component compatible with the pump head and its maximum pressure; do not connect an unverified transducer to a pressurized line. Prototype sensing through an isolated test fixture first. Battery protection and charge management must be independently rated; no unattended charging in deployment. Gateway and sensor enclosures require weather ingress testing. BLE is selected for low payload and room-scale range; Wi-Fi on battery nodes would increase standby load. Test coverage at actual metal stands.

### Estimated power

Tool Dock target 15 uA sleep, 8 mA for 20 ms per event, 20 events/day; 1000 mAh / (0.015 mA + 0.0037 mAh/day) gives roughly 2.7 years ideal, derated to a 12-month service target for self-discharge, cold, and retries. Pump Monitor target 25 uA average with periodic 1-second checks at 2 mA every 10 seconds: about 0.225 mA average, yielding 185 days ideal / 90-day service target. Measure these values on assembled hardware; they are design budgets, not test results. Gateway is continuously USB powered.

## Protocol

BLE service UUID `8a2f0001-0b7a-4d9e-a3c1-5ad1b2c00001`; characteristic payload is canonical UTF-8 JSON, max 160 bytes: `{"v":1,"node":"tool-dock-01","seq":42,"kind":"tool_state","tool":"hex5","present":true,"battery_mv":3980}`. Kinds are `tool_state`, `pump_test`, `heartbeat`, `service_ack`; pump readings use integer `pressure_kpa`. Sequence numbers are monotonically increasing uint32 and wrap modulo 2^32; gateway ignores exact duplicate `(node,seq)` and flags gaps. Sensor advertisements are unauthenticated prototype telemetry: production deployment must bond devices, rotate per-install keys, protect the gateway API, and use TLS with authenticated MQTT. No remote commands/OTA. Gateway retains 30 days of observations locally and exports only site-level status, not rider data.

MQTT topics: `communityrepair/v1/{site_id}/{node_id}/telemetry`; API is read-only `GET /health`, `GET /sites/{site_id}/status`, `GET /sites/{site_id}/events`. MQTT credentials are supplied through environment variables, never committed. API access must be restricted to trusted local network until authentication is implemented.

## Contents and maturity

- `firmware/common/` portable C frame validation and host tests.
- `firmware/tool-dock/` and `firmware/pump-monitor/` illustrative nRF52 sampling loops; target SDK integration is not yet implemented.
- `schematic/` pin and wiring intent only; no KiCad fabrication files are claimed.
- `hardware/bom/` per-node estimated BOMs.
- `software/dashboard/` runnable FastAPI local prototype with SQLite event store.
- `software/ml-pipeline/` synthetic example for inspection prioritization, not a validated predictive model.
- `software/mobile-app/` React Native concept stub; not installation-ready.
- `docs/` architecture, API, protocol and safety notes.

No physical build, RF range test, weather test, or field study has been performed. No sensor data set supports ML accuracy claims. Public deployment requires accessibility review and staff procedures.

## Validation and safety

Run `cc -std=c11 -Wall -Wextra -Werror firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/communityrepair-test && /tmp/communityrepair-test`; dashboard tests with `python -m unittest discover software/dashboard/tests`; ML demo with `python software/ml-pipeline/train.py`. An absent or implausible sensor report becomes UNKNOWN and opens a human inspection task; it does not certify a stand or bicycle as safe. No occupant tracking, audio, images, or health data. Inspectors must physically verify tools and pump function. This is an advisory maintenance aid, not a roadworthiness or safety certification system.

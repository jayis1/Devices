# CoolStop — community cooling-site readiness

Status: concept prototype; no hardware has been fabricated or safety-certified. This system reports observed room conditions and supply availability to site staff. It is not an emergency service, does not assess individual health, and must not be used to decide that a site is safe during a heat emergency.

## User and daily moment

A volunteer coordinator opening a neighborhood cooling room on a hot afternoon needs to know whether the public room is open, whether its occupied-zone temperature and humidity are changing, whether drinking-water containers are running low, and whether an in-person check is overdue. Today those facts are scattered across paper logs and messages. CoolStop puts coarse site-level readiness on a local display and staff dashboard, without tracking visitors.

The climate node measures the occupied zone, the supply node estimates water-container mass, and a gateway joins their distinct measurements with a staff-declared open/closed state. The separate nodes can be placed where people and supplies actually are; the gateway timestamps and displays their reports even when the internet is down. A phone-only or one-box design cannot report both room climate and remote stock status. Offline, the local display continues to show the last readings and their age; missing or stale reports are visibly marked unknown, never assumed ready.

Non-goals: medical advice or heat-risk diagnosis; visitor counting or identity tracking; automatic HVAC or water-system control; claims of potable water quality; automated dispatch or certification of emergency shelter readiness.

Failure behavior: loss of internet leaves the LAN display and buffered measurements available; loss of a sensor link marks that metric stale; low battery produces a maintenance warning, not a fabricated value; sensor disagreement is shown with timestamps and requires staff inspection. Staff can always use the paper checklist and override the displayed status.

## System

Three low-voltage prototype nodes communicate over Wi-Fi/MQTT on a site LAN. No radio range or regulatory certification is claimed; use regionally approved modules and follow local radio rules. The gateway is an ESP32-S3 module for local display and Wi-Fi, with a USB-connected Linux host providing MQTT, FastAPI, SQLite, and the dashboard (the gateway coordinates sensor nodes; it is not the API host). Climate and supply nodes are ESP32-C3-MINI-1 modules. This prototype uses Wi-Fi rather than a custom Sub-GHz link so the integration path can be exercised with ordinary local infrastructure.

```text
Climate node (SHT31) --Wi-Fi/MQTT--> Site gateway (ESP32-S3 + display)
Supply node (load cell/HX711) --Wi-Fi/MQTT-->       | LAN MQTT
                                                    v
                                     Linux host: Mosquitto + FastAPI/SQLite
                                                    |
                                             React Native stub
```

Readings use protocol v1 JSON on `coolstop/v1/<site>/<node>/telemetry`. Each report has `version`, `node_id`, monotonically increasing `seq`, UTC `timestamp`, `kind`, and `values`; temperatures are °C, relative humidity percent, supply mass grams. The backend rejects malformed or unsupported versions and stores the last valid report. Broker access must be restricted to per-node credentials and TLS where infrastructure supports it; this repository contains no credentials or production provisioning implementation. Do not expose the broker publicly.

## Hardware and interfaces

Pin assignments below are prototype module GPIOs; validate the chosen carrier board and electrical limits before assembly. All electronics are SELV USB 5 V input with 3.3 V logic. No mains switching. Use a certified USB supply and enclosed, strain-relieved assemblies; keep the load cell and electronics dry.

| Node | SoC / function | Connections | Power |
|---|---|---|---|
| Gateway | ESP32-S3-WROOM-1-N8; Wi-Fi coordinator, local status display | I²C GPIO8 SDA/GPIO9 SCL to SSD1306 OLED (0x3C); USB serial to Linux host | 5 V USB; display/MCU 3.3 V module rail |
| Climate | ESP32-C3-MINI-1; room condition | SHT31-DIS-B (I²C 0x44), GPIO4 SDA/GPIO5 SCL | 5 V USB; module 3.3 V rail |
| Supply | ESP32-C3-MINI-1; static container-scale estimate | HX711 DOUT GPIO4, SCK GPIO5; 4-wire 1 kg strain gauge bridge | 5 V USB; HX711/module 3.3 V logic |

The scale measures static load only, requires a tare and known-mass calibration, and does not establish drinkability or available volume in irregular containers. The SHT31 is not a calibrated life-safety instrument. Local enclosure, strain relief, condensation protection, and periodic reference checks are required.

BOM files: `hardware/bom/`. Costs are rough USD prototype estimates, not live quotes; module antenna and carrier variants must be checked before ordering. A schematic directory contains pin/net specifications, not KiCad schematics or fabrication-ready PCB layouts.

## Software, build, and verification

- C firmware samples node sensors and publishes protocol-v1 telemetry (ESP-IDF project files are not included; `main.c` is an illustrative integration skeleton).
- `software/dashboard/` is the runnable Python API/MQTT ingestion slice. See `software/dashboard/README.md`.
- `software/ml-pipeline/` trains a seeded synthetic supply-depletion baseline only. It demonstrates a reproducible workflow; it is not validated for operational predictions.
- `software/mobile-app/` is an explicitly non-buildable React Native screen/API stub; it has no dependency manifest or authentication flow.
- `scripts/` has local setup and calibration instructions; never place credentials in source control.

Backend checks: `python3 -m unittest discover -s software/dashboard/tests`. Run API locally with `python3 -m pip install -r software/dashboard/requirements.txt` then `uvicorn app:app --app-dir software/dashboard`; MQTT ingestion is optional and enabled only when `paho-mqtt` is installed and configured. See `docs/` for protocol, API, architecture, and design rationale.

All measurements are advisory and may be wrong or stale. The deployment owner must keep a human opening checklist, verify conditions in person, and follow applicable public-health and accessibility guidance. Do not use this prototype as a substitute for official heat-emergency procedures.
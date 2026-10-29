# SnowShare — community sidewalk clearing coordination

Status: concept prototype. Hardware is unbuilt and unvalidated; this is not a winter-safety certification or guarantee of accessible passage. SnowShare helps neighborhood groups and municipal crews prioritize manual inspection and snow-clearing work at curb ramps and short sidewalk segments after winter weather.

## User need and difference

A fresh snowfall creates many simultaneous sidewalk tasks, while residents using wheelchairs, walkers, strollers, or canes need cleared curb transitions to complete ordinary trips. Phone reports alone are uneven and quickly stale; a single sensor cannot distinguish localized accumulation across a route. SnowShare combines low-power surface-condition nodes at selected locations with a staffed hub that publishes age-stamped observations and accepts human-confirmed work updates. It differs from FrostLine, which reports microclimate/surface-condition advisories for inspection, and CurbCue, which reports blocked curb ramps and sidewalk hazards: SnowShare focuses on post-snow event task coordination and verified manual clearing, not hazard detection or route safety claims.

A day in use: overnight nodes record temperature and precipitation context; a volunteer coordinator reviews the hub's queue after a storm; a crew checks a site, clears it, and records completion from the field; stale or failed sensors are marked unknown and never interpreted as cleared. No system-generated route is represented as safe.

## Why multiple nodes, not an app plus sensor?

Tasks are spatially distributed and need evidence at the curb ramp itself, while a gateway at a community center can continue collecting without internet. A phone-only reporting workflow cannot provide consistent time-series context for dozens of sites or distinguish missing observations from reports of clear ground. The network only supports prioritization; human inspection remains decisive.

## System and data flow

```text
Ramp node A (ESP32-C3 + surface probe) ─┐
Ramp node B (ESP32-C3 + surface probe) ─┼─ Wi-Fi/MQTT → ESP32-S3 site gateway
Weather node (ESP32-C3 + temp/rain) ───┘                         │ LAN MQTT
                                                    Linux host: FastAPI + SQLite
                                                                  └ React Native staff stub
```

Nodes publish JSON v1 to `snowshare/v1/<site>/<node>/telemetry`: `version`, `node_id`, `seq` (uint32), `timestamp` (UTC RFC3339), `kind`, and `values`. Units: surface temperature °C, probe ADC raw counts, precipitation-tip count, battery V. Raw ADC is not a validated snow-depth or ice measurement. Duplicate or older sequence values are rejected; unknown/stale is distinct from clear. MQTT credentials must be unique per node, ACL-restricted, and TLS-protected on shared networks. No secrets are included. There are no actuators or remote control commands.

Local sampling and gateway history continue during internet outages. A local broker/API is optional; if unavailable, nodes retry with bounded backoff and expose lost-data status. On restart, sequence state must be persisted by implementation before deployment. No life-safety or emergency dispatch dependency exists.

## Hardware design specification

All electronics are extra-low-voltage (SELV), 3.3 V logic. Example modules/components are proposed and require datasheet and board-revision review. Surface probes are experimental temperature/thermal-contact measurements, not certified snow depth or ice detection. Install only with property-owner permission and accessible manual inspection. The schematic directory contains pin/net specifications, not fabricated KiCad designs; no fabrication-ready PCB is claimed.

| Node | Main parts | Pin/interface assignment | Power target |
|---|---|---|---|
| Site gateway | ESP32-S3-WROOM-1-N8, SSD1306 128x64 I²C OLED | OLED SDA GPIO8, SCL GPIO9; USB-UART programming; Wi-Fi 2.4 GHz | 5 V USB, design estimate 0.6–1.2 W |
| Ramp node ×N | ESP32-C3-MINI-1, DS18B20 sealed temperature probe, 4.7 kΩ pull-up | 1-Wire GPIO4; status LED GPIO8 through 1 kΩ; Wi-Fi | 3.3 V regulated; estimate 30–100 mA average Wi-Fi duty-cycle dependent |
| Weather node | ESP32-C3-MINI-1, DS18B20, tipping-bucket reed switch | 1-Wire GPIO4; reed GPIO5 interrupt with pull-up; Wi-Fi | 3.3 V regulated, same unvalidated estimate |

GPIO assignments are proposals, not validated against every module carrier; check module datasheets, boot straps, board routing, and peripheral pin matrix before assembly. Provide local decoupling (100 nF at each IC plus 10 µF module bulk), a protected 5 V input and a qualified 3.3 V regulator with at least 500 mA peak capacity per radio node. Battery/solar systems are intentionally not specified; use approved isolated USB SELV supplies for prototype tests. Outdoor deployment needs enclosure, condensation, UV, ESD/surge, connector, cable, and RF validation.

### Indicative BOMs

Prices and suppliers are not verified; exact orderable module suffix and probe cable variant must be confirmed. BOM CSVs are per node and do not imply sourcing approval.

| Node | Approximate prototype parts cost (USD) | BOM |
|---|---:|---|
| Gateway | $27–45 | `hardware/bom/gateway_BOM.csv` |
| Ramp sensor | $13–23 each | `hardware/bom/ramp-node_BOM.csv` |
| Weather | $15–28 | `hardware/bom/weather-node_BOM.csv` |

## Software

- `firmware/common/protocol.c`: bounded portable C frame codec with CRC-16/CCITT-FALSE; CRC detects accidental corruption, it is not authentication.
- Per-node C sources document sampling and the transport integration boundary. Vendor SDK projects and real radio drivers are not supplied; MCU builds are unverified.
- `software/dashboard/`: FastAPI local prototype, SQLite persistence, MQTT integration boundary, and tests. Configure broker TLS and per-device ACL outside the sample; never expose unauthenticated endpoints publicly.
- `software/ml-pipeline/`: synthetic-data demonstration only; no trained model or validated prediction. Labels in real use would be human inspection outcomes; split by site and storm event, evaluate precision/recall and calibration. Rule-based age/inspection queue is the fallback.
- `software/mobile-app/`: React Native staff workflow stub for status, stale-data notice, and human-confirmed task completion; not a production app.

## Safety, privacy, and operations

The service is advisory, not an accessibility guarantee, ice detector, or substitute for inspection. Do not rely on it for emergency access, mobility decisions, or municipal compliance. No automatic routing claims or actuation. Sensor failure, low battery, calibration expiry, or stale timestamp maps to UNKNOWN and a human inspection task. Snow clearing remains under local procedures and worker safety rules.

No audio, images, personal identity, or resident location are collected. Store coarse site IDs and operational readings only; configure retention locally and provide operator deletion through the database. Field reports should not include names or health data. Provide unique credentials, disable debug endpoints in deployment, and update devices only through a reviewed physical maintenance procedure; secure OTA is a future requirement, not implemented.

## Build and verification status

See `docs/architecture.md`, `docs/protocol.md`, and `docs/verification.md`. Run backend tests with `python3 -m unittest discover -s software/dashboard/tests`; compile portable protocol tests with `cc -std=c11 -Wall -Wextra -Werror firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/snowshare-protocol && /tmp/snowshare-protocol`. Hardware, radio, outdoor, and KiCad validation have not been performed. Do not interpret this concept prototype as build-ready or certified.
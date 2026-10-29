# RainGarden — community rain-garden care network

Status: concept prototype. No boards have been fabricated, field-tested, or certified. RainGarden helps parks staff and volunteer stewards maintain planted stormwater basins by showing soil wetness, basin ponding, and recent rainfall at separate locations. It does not control drainage infrastructure, predict floods, or certify water quality.

## Need and product choice

A steward deciding whether a rain garden needs inspection must distinguish a dry planting bed from a saturated basin or a blocked inlet. A single phone app cannot observe subsurface moisture and basin water level at distributed locations, and manual visits miss changes between rounds. Three low-voltage nodes report local measurements to a gateway; the gateway remains useful offline and flags stale data for human inspection. The intended benefit is earlier, evidence-based maintenance and fewer unnecessary site visits, not automated public-safety decisions.

This differs from home flood protection (StormSync), household drain condition (DrainSync), and lawn irrigation (LawnSync): it is an advisory monitoring network for planted municipal/community stormwater assets and never actuates valves or pumps.

## Architecture

```text
Soil probe (ESP32-C3 + capacitive probe) ─┐
Basin gauge (ESP32-C3 + ultrasonic) ──────┼─ Wi-Fi/MQTT → ESP32-S3 gateway/display
Rain node (ESP32-C3 + tipping bucket) ────┘                    │ LAN MQTT
                                                        Linux host: Mosquitto/FastAPI/SQLite
                                                                  └ React Native stub
```

Nodes report JSON v1 to `raingarden/v1/<site>/<node>/telemetry`; fields: `version`, `node_id`, unsigned `seq`, UTC `timestamp`, `kind`, `values`. Units are volumetric-moisture proxy percent (calibrated ADC mapping, not soil laboratory VWC), millimetres for water depth and rainfall, and battery volts. Gateway displays last value and age; disconnected or stale data is UNKNOWN, never READY. Reports are idempotently keyed by node and sequence; duplicate/out-of-order packets are discarded. No actuator commands exist. Provision unique credentials out-of-band; restrict broker ACLs and use TLS on shared networks. No secrets are shipped.

Internet loss does not stop local sampling; the gateway caches a bounded recent history. Gateway or node power loss is indicated as stale. Rain gauge overflow, sensor disconnect, implausible range, and calibration expiry are surfaced as faults. Staff inspection and existing municipal drainage procedures remain authoritative.

## Nodes, pins, and power

Prototype modules use 3.3 V logic and Wi-Fi 2.4 GHz; comply with locally approved module/radio rules. GPIO assignments are proposed for ESP32 modules and must be checked against the selected module revision and board. All power is SELV, weatherproof enclosure design and condensation/drainage are unresolved engineering work. No mains or infrastructure wiring.

| Node | SoC and components | Pins / interface | Supply |
|---|---|---|---|
| Gateway | ESP32-S3-WROOM-1-N8, SSD1306 OLED | I²C SDA GPIO8/SCL GPIO9; Wi-Fi | regulated 5 V USB; design target <1 W |
| Soil ×N | ESP32-C3-MINI-1, capacitive analog probe (DFRobot SEN0193 class) | ADC GPIO0; Wi-Fi; avoid ADC2/Wi-Fi conflict by using ADC1-capable channel per module datasheet | 5 V USB or qualified solar/battery supply, not included |
| Basin | ESP32-C3-MINI-1, JSN-SR04T ultrasonic module | TRIG GPIO4, ECHO GPIO5 through 3.3 V level interface; Wi-Fi | 5 V USB; module rails regulated |
| Rain | ESP32-C3-MINI-1, reed-switch tipping bucket | GPIO4 interrupt with pull-up; Wi-Fi | 5 V USB |

Design estimates only: gateway average 120–250 mA at 5 V depending on Wi-Fi/display; each sensor node 30–100 mA average when Wi-Fi active, requiring mains-free deployment power engineering before field use. Solar/battery runtime is not claimed. Pin level compatibility and ESP32-C3 ADC channel selection require board-level validation.

## BOM and schematic fidelity

BOMs are prototype-level indicative lists without supplier pricing. `schematic/` contains human-readable net/pin plans, not KiCad schematics; PCB layouts, enclosure CAD, surge/lightning protection, environmental qualification, and fabrication files are not provided. Verify datasheets and component revisions before procurement.

## Software and build status

- `firmware/` contains portable C protocol framing and per-node sampling/integration examples; vendor SDK projects and sensor drivers are not yet included, so firmware is not build-tested.
- `software/dashboard/` is a minimal FastAPI ingestion API with SQLite and optional MQTT subscriber; see local requirements.
- `software/ml-pipeline/train.py` demonstrates a deterministic synthetic-data baseline only. It provides no real-site training or predictive claims; rules-based review is the fallback.
- `software/mobile-app/` is a React Native API-consumer stub, not a production app.
- Run backend checks: `python3 -m unittest discover -s software/dashboard/tests`.

## Safety, privacy, and limitations

No imagery, audio, occupant data, or location tracking is collected. Site IDs should be non-identifying; retain only operational telemetry for a locally configured period. This system cannot establish water quality, structural integrity, flood safety, or whether an area is safe to enter. Do not enter flooded/contaminated basins to service sensors. Physical deployment requires municipal approval, qualified electrical/environmental review, calibrated instruments, and accessible human inspection procedures.

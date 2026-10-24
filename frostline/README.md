# FrostLine

FrostLine is a neighborhood-scale, local-first walkway icing advisory network for apartment cooperatives, campuses, and small municipalities. It helps a caretaker decide which pedestrian paths to inspect first; it does not declare a route safe, spread de-icer, or replace an official weather service.

## User, problem, and measurable value

A facilities caretaker currently walks a fixed route before dawn to look for black ice, often checking low-risk areas before shaded bridges and wind-exposed steps. FrostLine ranks instrumented locations by measured surface temperature, moisture, and recent air conditions. Pilot metric: reduce median time from first below-freezing wet-surface observation to caretaker inspection by at least 10 minutes, measured against timestamped manual logs over one winter. This is a proposed evaluation target, not a demonstrated result.

Three ordinary moments: before opening, the hub highlights the cold shaded ramp; during the day, a caretaker marks a site inspected and records treatment; during a network outage, the hub retains recent measurements and displays stale-data status rather than inventing a current risk.

## Build status

| Area | Status |
|---|---|
| Architecture, node interfaces, protocol | Reference design; not field validated |
| Firmware | Portable application logic and protocol compile/test on host; vendor radio/RTC HAL remains to integrate |
| Schematics | Human-readable connection/netlist documents, not KiCad capture |
| BOM | Estimated prototype parts/costs; confirm sourcing and weather ratings |
| Backend | FastAPI local demonstration with SQLite and MQTT adapter boundary; authentication/TLS deployment required |
| ML | Reproducible synthetic-data pipeline smoke test only; not an operational forecast |
| Mobile | React Native screen stub, not a build-verified app |
| Physical/weather validation | Not performed; no safety or regulatory certification claimed |

## Why multiple nodes, and why not just buy a weather station?

Roadside weather stations report regional air conditions, not the distinct shade, drainage, and wind exposure of a particular ramp or stair landing. Two separated microclimate nodes let a caretaker compare sites; a gateway coordinates timestamps, stores data when offline, and forwards an authenticated uplink when configured. The third node is a removable surface probe placed at the most consequential walking surface. This spatial diversity is the reason for the network—not arbitrary board splitting. Unlike a consumer forecast, FrostLine provides site-specific, inspectable evidence and manual inspection workflow; it is not a substitute for municipal road-weather systems or a certified pavement sensor.

## Nodes and parts

| Node | Quantity | Controller / radio | Sensors and role | Power |
|---|---:|---|---|---|
| Site hub | 1 | Raspberry Pi Pico W (RP2040, Wi-Fi) | RTC DS3231, local status LEDs; SX1262 transceiver over SPI | 5 V USB-C; optional protected UPS |
| Air sentinel | 2+ | STM32L072 + SX1262 transceiver IC; region-specific RF matching | SHT45 air temperature/RH, BME280 pressure; shaded radiation shield | 18650 protected cell + solar charge; budget to validate |
| Surface probe | 1+ | STM32L072 + SX1262 transceiver IC; region-specific RF matching | MAX31865/PT1000 surface contact temperature; resistive wetness grid; sealed cable gland | 18650 protected cell + solar charge |

Use 868 MHz only where permitted in the EU and 915 MHz only in appropriate regional allocations; configure legal duty cycle, EIRP, antenna, and certification for deployment. LoRa is a star link, not an ad-hoc mesh. Typical reporting interval is 5 minutes, with event-driven reports on threshold crossing. The hub buffers at most 7 days of records and marks data stale after 15 minutes without a node report.

## Architecture and local decision support

```text
[Air sentinel A] --\
[Air sentinel B] ---- LoRa star ----> [Pico W hub + RTC + SQLite buffer]
[Surface probe] ---/                         | Wi-Fi/MQTT TLS (optional)
                                              v
                                     [FastAPI + SQLite]
                                              |
                                     [Caretaker mobile view]
```

The advisory rank is transparent: wetness detected AND surface temperature at or below 2 °C raises inspection priority; air temperature/RH and dew-point trend add context. It is not an ice detector and must not label untreated pavement safe. Cloud outage does not affect local sampling or local history. Manual inspection and treatment remain human decisions; no actuator or chemical-dosing control exists.

## Pin and bus plan (prototype carrier, 3.3 V logic)

| Node | MCU pin | Interface | Connection |
|---|---|---|---|
| Hub Pico W | GP2/GP3 | UART0 | SX1262 debug/service console only (radio uses SPI) |
| Hub Pico W | GP6/GP7/GP4 | SPI1 SCK/MOSI/MISO | SX1262 SCK/MOSI/MISO; separate CS GP5, BUSY GP8, DIO1 GP9, RESET GP10 |
| Hub Pico W | GP12/GP13 | I²C1 | DS3231 SDA/SCL, 4.7 kΩ pullups to 3.3 V; address 0x68 |
| Air STM32L072 | PB13/PB15/PB14 | SPI2 SCK/MOSI/MISO | SX1262; CS PB12, BUSY PA0, DIO1 PA1, RESET PA2 |
| Air STM32L072 | PB6/PB7 | I²C1 | SHT45 (0x44), BME280 (0x76) |
| Surface STM32L072 | PB13/PB15/PB14 | SPI2 SCK/MOSI/MISO | SX1262; CS PB12, BUSY PA0, DIO1 PA1, RESET PA2 |
| Surface STM32L072 | PA4/PA5/PA6/PA7 | SPI1 | MAX31865 SCK/MISO/MOSI/CS; 3-wire PT1000 option; wetness divider to PA0 ADC |

Pins are a reference allocation for STM32L072KZ-class package and Pico W; validate against the exact chosen package, board routing, CubeMX/Pico SDK, boot/debug needs, and module datasheets before PCB design. Never connect a 5 V signal to MCU GPIO. Add ESD protection at outdoor connectors, TVS on long sensor leads, series resistors, and a sealed, serviceable enclosure. No fabricated KiCad files are included.

## Protocol

LoRa application payload is a compact fixed-layout packet: version u8, message_type u8, node_id u16 LE, sequence u16 LE, unix_time_s u32 LE, surface_temp_c_centi i16 LE, air_temp_c_centi i16 LE, rh_permille u16 LE, wetness_u16 u16 LE, battery_mv u16 LE, flags u8, CRC16-CCITT-FALSE u16 LE. CRC covers all preceding bytes; it detects accidental corruption only, not forgery. Radio link keys and authenticated enrollment are not implemented in this reference firmware; production requires per-device authenticated encryption, replay protection and key rotation. Packets with unsupported version, invalid length, bad CRC, duplicate sequence, or implausible sensor sentinel values are rejected. See docs/protocol.md.

## Power and cost estimate

Air node target average current is 0.9 mA (MCU/sensor sleep 0.5 mA assumed + radio/report average 0.4 mA), with 120 mA radio peak. A 2600 mAh protected cell gives ideal 120 days (2600/0.9/24), before cold derating, regulator quiescent loss, self-discharge, and solar contribution; design for monthly inspection until measured. Surface node target 1.2 mA, same cell: ideal 90 days. Hub draws approximately 120 mA at 5 V with radio receive and Wi-Fi; use a 5 V 1 A supply and protected backup sized separately. These are engineering targets, not measured current.

Estimated small-run BOM (USD, planning estimates): hub $40; each air node $42; surface node $48; one hub + two air nodes + one probe ≈ $172, excluding enclosure machining, assembly, shipping, taxes, test equipment, RF matching/antenna design, and cloud services. BOMs are in hardware/bom/; prices and component availability must be confirmed at purchase.

## Software and ML

FastAPI endpoints: GET /health, POST /api/v1/telemetry, GET /api/v1/sites/{site_id}/latest. The demo stores telemetry in SQLite. MQTT connection is an integration boundary; live broker TLS/client identity must be configured before deployment. ML training consumes generated synthetic examples solely to exercise the pipeline; no accuracy claim or real icing model is provided. The mobile stub shows site priority and freshness; it does not send actuation commands.

## Safety, privacy, and operation

FrostLine is advisory-only, not a life-safety system, pavement certification, or guarantee against ice. Continue routine inspections and follow local winter-maintenance policy. Install probes so they do not create trip hazards, obstruct accessible routes, or compromise drainage. Outdoor batteries require protected cells, fusing, temperature-rated charging, and qualified enclosure design. Collect only sensor readings and site/node identifiers; do not collect names, location histories of people, audio, or images. Configure TLS, unique credentials and access control before exposing APIs. Default retention is 30 days in the demo; operators should set a deletion policy and obtain site-owner consent.

## Build and bring-up

1. Verify radio frequency plan and exact component variants for jurisdiction.
2. Assemble low-voltage prototype boards; first power from a current-limited bench supply.
3. Confirm SPI radio, I²C addresses, RTD calibration with a traceable reference, and wetness sensor dry/wet behavior.
4. Run firmware protocol host tests; integrate vendor SDK HAL and perform RF/regional compliance review.
5. Place nodes at two contrasting microclimates and a representative surface; log alongside manual observations for several freeze/thaw events.
6. Only after calibration and weather sealing, operate as an advisory pilot; never remove existing inspections.

See docs/architecture.md, docs/protocol.md, docs/api.md and schematic/*/README.md. All costs and power values require bench validation. Invented by jayis1.

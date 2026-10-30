# ShadeShare

ShadeShare is a local-first reference system for parks teams and community groups maintaining public shade structures during hot weather. A volunteer can check whether a mapped shade shelter is present and usable before walking to it. The system reports equipment condition and surface heat context; it does not count visitors, recommend medically safe exposure, certify structures, or control equipment.

## Problem and users

Shade directories become stale after storms, maintenance closures, or seasonal changes. Residents—especially people who need a shaded rest stop—may make an unnecessary trip when a listed shelter is unavailable. Site stewards install and maintain the nodes; a municipal or nonprofit operator owns the gateway and reviews maintenance cards. The target workflow is a staff-confirmed daily check with sensor evidence between inspections.

## Differentiation

Heat Relief Network and CoolStop monitor indoor cooling-site climate and supplies. ShadeShare instead observes outdoor shade asset availability and temperature context, with no visitor or occupancy sensing. It is also distinct from GlowRoute, which reports walkway lighting, and FrostLine, which tracks winter walkway conditions. No shared registry or unified-system dependency is required.

## System layout

```text
[Shade Node: panel angle + tamper] --LoRa--> [Site Gateway: RP2040 + RAK3172]
[Surface Node: contact temperature] --LoRa--> [Site Gateway] --HTTPS/MQTT TLS--> optional operator service
                                                               | local web status / CSV export
```

Nodes are low-voltage and observational. LoRa uses a configurable regional plan (EU868 or US915); deployments must obey local duty-cycle/power rules. Store-and-forward works without internet; alerts are advisory. Gateway retains up to 30 days of compact readings on removable/local storage and exports aggregates only. No microphone, camera, identity, or visitor count is collected.

## Nodes and reference pin assignments

| Node | Controller/radio | Sensors and connections | Power |
|---|---|---|---|
| Gateway | RP2040 module + RAK3172 LoRa modem | UART0 GP0/1; modem reset GP2; status LED GP25; USB-C 5 V input | 5 V USB supply; 3.3 V regulated |
| Shade Node | STM32L031K6 + RFM95W (SX1276) | radio SPI1 SCK/PB3, MISO/PB4, MOSI/PB5, NSS/PA4, DIO0/PA1; reed switch/PA0; tilt switch/PA2 | 2×AA primary lithium, deep sleep; target >12 months, needs field validation |
| Surface Node | STM32L031K6 + RFM95W | same SPI mapping; 10 kΩ NTC divider/PA3 ADC; service button/PA0 | 2×AA primary lithium; target >12 months, field validation required |

These are logical reference assignments, not a fabrication-ready schematic. Node radio modules require a regulated 3.3 V rail with local decoupling and manufacturer-recommended antenna clearance. Prototype this pinout against the selected board/module revisions before PCB layout. Inputs use defined pull-ups and debounce; no GPIO drives an actuator. AA cells are not rechargeable; replace as a matched pair.

## Protocol and offline behavior

A compact application record is `{version:u8, node_id:u32, seq:u16, kind:u8, value:int16, battery_mv:u16, crc16:u16}` with network byte order, maximum 16 bytes. Kinds: shade-state, tamper, surface-temp-centiC, heartbeat. Gateway rejects unsupported versions, invalid CRC, duplicate sequence numbers, and stale observations (>15 minutes for current status); retries use bounded exponential backoff with jitter. LoRa link-layer encryption is not assumed: production deployments must add per-node unique keys, authenticated encryption, replay protection and key rotation before collecting operational data. The prototype protocol is not secure for hostile environments. No remote control or OTA is implemented.

## Software and maturity

`firmware/` contains portable C protocol code and illustrative node sampling loops. `software/dashboard/` provides a small FastAPI read-only status/export service; MQTT credentials are environment supplied. `software/ml-pipeline/` contains a synthetic-data baseline for prioritizing inspection, not a heat-safety predictor. `software/mobile-app/` is a React Native interface stub for map/list, site detail and maintenance acknowledgement. The complete sensing-to-gateway concept is specified, but hardware, radio link, MQTT broker, app installation and field performance have not been tested. Schematics are pin-assignment documentation, not KiCad fabrication files.

## Safety, privacy and limits

This is not a medical product, heat emergency warning, accessibility certification, or structural inspection. A human must inspect structures and confirm directory entries. A failed or stale sensor is shown as “unknown”, never as safe/available. No actuators or mains wiring are present. Provide a physical service label and manual inspection workflow. Low-vision access requires high-contrast text and screen-reader labels; the printed site ID and staff-maintained paper directory remain the no-phone fallback. Internet outage does not stop local logging. Synthetic ML data cannot support accuracy claims; retain no raw location or personal data.

## BOM summary

Representative parts only; costs are approximate planning estimates, not live quotes. Regional radio certification and final enclosure/antenna choices affect sourcing.

| Node | Main parts | Approx. prototype subtotal |
|---|---|---:|
| Gateway | RP2040 module, RAK3172 modem, USB supply | $31 |
| Shade | STM32L031K6, RFM95W, switches, holder | $15 |
| Surface | STM32L031K6, RFM95W, NTC, holder | $14 |

## Build and validation

1. Assemble the gateway and two nodes using approved regional radio settings and a dummy test load (there are no actuators).
2. Validate each sensor state by manually opening/closing the test switch and heating the NTC with a controlled reference thermometer.
3. Run the host protocol test with `cc -std=c11 -Wall -Wextra -Werror firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/shadeshare-protocol && /tmp/shadeshare-protocol`.
4. Start the dashboard with a Python environment containing FastAPI and Uvicorn; set broker/database configuration from environment. Keep it on localhost unless TLS and authentication are configured.
5. Walk-test radio coverage and battery life at the intended site; do not infer range from nominal LoRa specifications.

## Limitations / next work

KiCad schematics, production provisioning, authenticated radio protocol, OTA, enclosure weathering, calibrated temperature accuracy, site map UX, field battery measurements, and hardware testing remain future work. The synthetic inspection-ranking model is a pipeline demonstration only. The design should receive engineering and accessibility review before public deployment.

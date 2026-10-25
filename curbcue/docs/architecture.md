# Architecture

CurbCue is an advisory-only community maintenance prototype. Hub aggregates low-resolution condition events from curb sensors, a public button and a crew acknowledgement beacon. It stores locally first; optional WAN sync is secondary. No camera, microphone, personal location, route recommendation, or actuation.

| Role | SoC | Link | Data |
|---|---|---|---|
| Hub | Raspberry Pi Zero 2 W | BLE central, Wi-Fi MQTT | queue, dashboard, timestamps |
| Ramp sensor | XIAO nRF52840 | BLE GATT | load delta, wetness, tilt, sequence |
| Report button | XIAO nRF52840 | BLE GATT | manual unverified report |
| Crew beacon | XIAO nRF52840 | BLE GATT | local inspection acknowledgement |

Data path: sample/debounce → bounded event → BLE → hub dedupe and local queue → optional MQTT TLS → FastAPI validation/storage → mobile stub. WAN loss does not stop local collection. Hub loss causes nodes to retain at most ten events; stale state is never rendered as clear. Manual reports remain unverified until crew inspection.

Proposed electrical interfaces (subject to board revision and bench verification): sensor I2C 3.3V, LIS3DH address 0x18, HX711 digital interface, wetness divider on ADC; hub display SPI0. Confirm pull-ups and ADC input limits; provide ESD, reverse-polarity protection and battery protection. No PCB/DRC/ERC or environmental validation has been performed. Battery charger design requires review.

Radio: BLE 5 LE in 2.4 GHz ISM, conducted power and antenna design per module certification and local rules; measured range not yet available. System is deliberately short-range site telemetry, not a city-wide network. Costs and duty-cycle assumptions are planning values only. Physical pilot, EMC, accessibility and ingress testing remain open.

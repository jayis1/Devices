# UmbrellaLink

Invented and maintained by jayis1. Prototype design; not physically validated.

UmbrellaLink helps a neighborhood, campus, or apartment lobby maintain a small pool of loanable umbrellas. A low-voltage site gateway coordinates two distinct dock nodes: a lending dock reports available umbrella slots and a return dock confirms deposits. A React Native app provides availability and handoff guidance without accounts, cameras, borrower identity, or location histories. The problem is forgotten rain gear and wasted single-use emergency purchases; unlike inventory and tool libraries already listed in Devices, this is a weather-triggered, anonymous public convenience pool.

## Build status and limits

The protocol codec and API validation path are executable host-side prototypes. Firmware files are portable C logic sketches (no target SDK build configured); schematics are pin/interface design notes, not KiCad electrical schematics. App and ML components are clearly scoped stubs. No physical testing, radio certification, weatherproofing, or field trial has occurred. The device never locks a person in/out or controls mains equipment. Human staff replenish and inspect umbrellas. Availability is advisory and can be stale.

## Nodes and architecture

1. Site gateway: Raspberry Pi Zero 2 W, Linux/Python edge service, Wi-Fi uplink, USB-powered 5 V supply. Bridges the local sub-GHz link to MQTT and the API; buffers observations during WAN outages.
2. Lending dock: ESP32-C6 module, 4 presence switches (one per slot), local status LEDs, 5 V USB supply. Reports slot occupancy; users take umbrellas manually.
3. Return dock: ESP32-C6 module, 4 presence switches and a 4-cell return rack, 5 V USB supply. Reports deposits separately so abnormal mismatch can be flagged for staff.

Node radio: 868 MHz FSK module (RFM69HCW, 3.3 V) on each ESP32-C6 board, point-to-point star to gateway, explicit region-specific legal frequency/power configuration required. Gateway uses a matching module over SPI. No mesh or cloud dependency for local observations. Gateway publishes MQTT over TLS; node payload authentication is not implemented in this prototype, so deploy only on a controlled test site until key provisioning/MAC is added.

Data flow: switch debounce → versioned bounded packet → gateway validation and deduplication → MQTT TLS → FastAPI validation → anonymous site summary → mobile availability view. On WAN loss the gateway retains the latest state; app/API mark readings stale after 15 minutes. Nodes retry at most 3 times with randomized backoff; gateway acknowledges packet sequence numbers. No user events are recorded.

## Electrical and pin plan

All logic is 3.3 V. ESP32-C6 module: SPI SCK GPIO6, MOSI GPIO7, MISO GPIO2, radio CS GPIO10, radio DIO0 GPIO3; four active-low slot switches GPIO18-21 with 10 kΩ pull-ups and 100 nF debounce capacitors. LEDs GPIO22-25 through 1 kΩ resistors. UART0 GPIO16/17 reserved for programming/debug. Use ESD protection at external switch wiring, 100 nF at each module supply pin, 10 µF local bulk capacitance. 5 V USB input feeds a 3.3 V regulator rated at least 500 mA; radio transmit peak budget 130 mA, ESP32 peak 300 mA, design rail for 500 mA minimum. Common ground; switches connect GPIO to ground. Verify module pin availability against selected module revision before PCB design. Do not connect mains.

Gateway radio SPI: SCK 11, MOSI 10, MISO 9, CS 8, DIO0 25 (Pi GPIO/BCM numbering), 3.3 V only; provide level-safe wiring and a separate 3.3 V regulator. Raspberry Pi 5 V input rated 2.5 A. Enclosure and connector design remain open.

## Protocol

UTF-8 JSON envelope, max 192 bytes, protocol version 1: `{\"v\":1,\"node\":\"lend-01\",\"seq\":17,\"kind\":\"slots\",\"free\":2,\"total\":4,\"uptime_s\":120}`. `free` and `total` are integer slot counts; `seq` is uint32 and wraps modulo 2^32. Reject unknown versions, malformed JSON, invalid counts, oversized payloads, and unknown node IDs. Gateway suppresses duplicate `(node,seq)` within its bounded cache; stale/out-of-order readings do not replace newer state. Production payload signing and secure enrollment are required before public deployment.

## Software

- `firmware/common/protocol.c`: bounded host-testable message validation.
- `firmware/lending-dock/main.c`, `firmware/return-dock/main.c`: node logic outline; integrate with ESP-IDF GPIO/radio drivers before hardware use.
- `software/dashboard`: FastAPI validation/API prototype. MQTT transport adapter is a deployment stub; configure TLS broker externally.
- `software/ml-pipeline`: optional baseline availability forecasting scaffold. It is not trained; no model is required for operation and no synthetic accuracy claims are made.
- `software/mobile-app`: React Native screen stub; no production build configured.

## BOM (per unit; indicative, verify availability)

| Node | Ref | Qty | Part | Purpose |
|---|---:|---:|---|---|
| Lending/return dock | U1 | 1 | Espressif ESP32-C6-WROOM-1-N8 | MCU/Wi-Fi/BLE |
| Lending/return dock | U2 | 1 | HopeRF RFM69HCW-868S2 | 868 MHz FSK radio |
| Lending/return dock | SW1-SW4 | 4 | Omron D2F-L micro switch (or equivalent) | Slot presence |
| Lending/return dock | D1-D4 | 4 | Generic 3 mm LED | Slot indicator |
| Lending/return dock | R1-R4 | 4 | 1 kΩ 1% resistor | LED current limit |
| Lending/return dock | R5-R8 | 4 | 10 kΩ 1% resistor | Input pull-up |
| Lending/return dock | C1-C4 | 4 | 100 nF ceramic | Switch filtering |
| Lending/return dock | U3 | 1 | AP2112K-3.3 (or equivalent 500 mA LDO) | 3.3 V rail |
| Gateway | U1 | 1 | Raspberry Pi Zero 2 W | Edge gateway |
| Gateway | U2 | 1 | HopeRF RFM69HCW-868S2 | Radio bridge |
| Gateway | PS1 | 1 | Regulated 5 V, 2.5 A USB supply | Gateway power |

## Repository map

See `schematic/`, `firmware/`, `hardware/bom/`, `software/dashboard/`, `software/ml-pipeline/`, `software/mobile-app/`, `docs/`, and `scripts/`. Assembly, data contract, threat limits, and commissioning checklist are in `docs/architecture.md`. Author: jayis1.

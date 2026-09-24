# HygieneGuard architecture

Author: jayis1

## Data flow and failure boundary

```text
Sink Sentinel xN ---- Sub-GHz TDMA ---+
Smart Dispenser xN - Sub-GHz TDMA ----+--> Hygiene Hub --> local MQTT/FastAPI --> mobile scaffold
Door Beacon xN ----- BLE / TDMA -------+
```

The system provides supply and routine guidance only. No node identifies a person, captures audio/video, controls a door, or decides whether hands are clean. A radio, hub, ML, or cloud failure changes every dependent state to `unknown`; no command is inferred from stale telemetry.

## Peripheral and power budget

| Node | Signals and pins | Power | Safe failure |
|---|---|---|---|
| Hub CM4 | UART0 GPIO14/15 to ESP32-C6; Ethernet; USB-C 5 V | 5 V, 3 A | local UI says offline; no automation |
| Sink Sentinel ESP32-C6 | I2C GPIO6/7: SHTC3; pulse GPIO2: YF-S201; ADC GPIO0: leak strip; SPI GPIO10/11/12/13 + DIO1 GPIO5: SX1262 | 5 V, 500 mA isolated adaptor | sends unknown; plumbing remains mechanical |
| Dispenser STM32G0B1 | HX711 PB6/PB7; pump pulse PA0; I2C PB8/PB9: TMP117; SPI1 PA5/6/7, PA4 NSS, PB5 DIO1 | 5 V, 1 A | manual pump remains usable |
| Door Beacon nRF52840 | e-paper SPI P0.13/14/15, CS P0.16; button P0.11; SX1262 SPI P0.20/21/22, NSS P0.23, DIO1 P0.24 | CR2477, 20 mA burst | blank display/no reminder |

Every board needs input fuse, reverse-polarity protection, local 100 nF decoupling, regulator bulk capacitance, labelled test points, and antenna keep-out. Sink electronics must remain outside wet zones and use SELV supplies.

## Commissioning and privacy

Use a unique per-node key provisioned outside source control. TLS protects hub-to-broker/API links; ACLs limit a node to its own topic. The hub retains a bounded event history and rejects duplicate `(node_id, epoch, sequence)` frames. Never present model scores as hygiene, infection, or health conclusions.
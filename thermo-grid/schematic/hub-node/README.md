# ThermoGrid hub-node Schematic

## KiCad Project

Open `hub-node.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB (Signal, GND, Power, Signal)
- Minimum trace width: 0.15mm (signal), 0.5mm (power)
- Via size: 0.3mm drill, 0.6mm pad
- Impedance controlled: 50Ω for RF traces (SX1262)
- Ground plane on layer 2 (unbroken for RF performance)

## Design Rules

- All decoupling capacitors within 2mm of IC power pins
- RF section (SX1262) isolated from digital section with grounded guard vias
- Boiler relay driven by MOSFET — separate power rail from logic (flyback diode!)
- Battery charge: MCP73831 with 500mA charge current for 2500mAh LiPo
- USB-C: 5V 2A minimum for charging + operation + relay
- Power supply sequencing: 3.3V before 1.8V (W25Q256)
- PCF8563 RTC with CR1220 backup — timekeeping during power outage for TOU schedules

## Key Design Considerations

- RP2040 Core 0 runs mesh TDMA + thermal forecast ML + TFT display (hard real-time)
- RP2040 Core 1 runs ESP32-C6 UART bridge + comfort model inference (soft real-time)
- ESP32-C6 handles WiFi MQTT + BLE GATT server + solar inverter API queries
- SX1262 high-power PA (20dBm) for reliable whole-house mesh coverage
- TFT display updates at 2Hz — low priority, never blocks safety checks
- Battery backup ensures mesh + freeze protection continue during power outage (critical)
- Boiler relay is opto-isolated from logic (230VAC switching)

## Pin Assignments (RP2040)

| Pin | Function | Notes |
|-----|----------|-------|
| GP0-1 | UART0 TX/RX | ESP32-C6 bridge (115200 baud) |
| GP2-3 | I2C1 SDA/SCL | PCF8563 RTC |
| GP6-8 | SPI0 | TFT display (ILI9488) |
| GP9 | GPIO | SD card CS |
| GP10-13 | SPI0 + GPIO | TFT CS/DC/RST/BL |
| GP14-16 | GPIO | SX1262 BUSY/IRQ/NRST |
| GP17-20 | SPI1 | SX1262 radio |
| GP22 | GPIO | Boiler relay (via MOSFET + optocoupler) |
| GP23 | GPIO | User button |
| GP24-26 | GPIO | RGB status LED |

## Pin Assignments (ESP32-C6)

| Pin | Function | Notes |
|-----|----------|-------|
| UART0 | RX/TX | Bridge to RP2040 (115200) |
| WiFi | internal | MQTT uplink to cloud, solar inverter API |
| BLE | internal | GATT server for mobile app + comfort tags |

## Schematic Files

- `hub-node.kicad_sch` — Main schematic
- `hub-node.kicad_pcb` — PCB layout (work in progress)
- `hub-node.kicad_pro` — KiCad project file

See BOM in `hardware/bom/hub_node_bom.csv` for component details.
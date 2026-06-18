# PorchGuard hub-node Schematic

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
- Siren driven by MOSFET (for 100 dB volume) — separate power rail from logic
- Battery charge: MCP73831 with 500mA charge current for 2500mAh LiPo
- USB-C: 5V 2A minimum for charging + operation + siren
- Power supply sequencing: 3.3V before 1.8V (W25Q256)

## Key Design Considerations

- RP2040 Core 0 runs mesh TDMA + pirate ML + TFT + siren (hard real-time)
- RP2040 Core 1 runs ESP32-C6 UART bridge (soft real-time)
- ESP32-C6 handles WiFi MQTT + BLE GATT server
- SX1262 high-power PA (20dBm) for reliable whole-house mesh coverage
- TFT display updates at 2Hz — low priority, never blocks safety checks
- Battery backup ensures porch monitoring + siren continue during power outage (critical)
- Siren driven at high volume for audible pirate/tamper deterrent

## Pin Assignments (RP2040)

| Pin | Function | Notes |
|-----|----------|-------|
| GP0-1 | UART0 TX/RX | ESP32-C6 bridge |
| GP6-8 | SPI0 | TFT display (ILI9341) |
| GP9 | GPIO | SD card CS |
| GP10-13 | SPI0 + GPIO | TFT CS/DC/RST/BL |
| GP14-16 | GPIO | SX1262 BUSY/IRQ/NRST |
| GP17-20 | SPI1 | SX1262 radio |
| GP22 | PWM | Siren (via MOSFET) |
| GP23 | GPIO | User button |
| GP24-26 | GPIO | RGB status LED |

## Pin Assignments (ESP32-C6)

| Pin | Function | Notes |
|-----|----------|-------|
| UART0 | RX/TX | Bridge to RP2040 (115200) |
| WiFi | internal | MQTT uplink to cloud |
| BLE | internal | GATT server for mobile app |

## Schematic Files

- `hub-node.kicad_sch` — Main schematic
- `hub-node.kicad_pcb` — PCB layout (work in progress)
- `hub-node.kicad_pro` — KiCad project file

See BOM in `hardware/bom/hub_node_bom.csv` for component details.
# PorchGuard mailbox-node Schematic

## KiCad Project

Open `mailbox-node.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB, 60×40mm (small to fit in/under mailbox)
- Impedance controlled: 50Ω for RF traces (SX1261)
- IP67 enclosure, UV-rated, conformal coated
- Solar panel on top side, antenna on edge

## Design Rules

- Ultra-low-power design — every uA matters (months+ on coin cells)
- HX711 load cell ADC: 24-bit, gain 128, low-power mode between reads
- DS18B20 on 1-wire with 4.7kΩ pullup
- LIS2DH12 on I2C with INT1 wired to wake-capable EXTI pin
- Reed switch on EXTI0 (door open) — primary wake source
- Solar cell (0.5W, 5V) → MCP73831 trickle-charges 2× CR2032 (3V system)
- Battery voltage divider on ADC for monitoring
- SX1261 RF output via SMA edge or PCB trace antenna

## Key Design Considerations

- STM32L011K4 chosen for ultra-low-power STOP mode (<1µA) + small footprint
- STOP mode with RTC wake every 5 min for temp/light poll
- PIR not used (mailbox door open is the trigger — reed switch)
- Load cell physically supports mail tray — mail weight drives classification
- Solar panel sized for average mailbox location (top of mailbox)
- LoRa SF9/SF12 for long-range reach to hub (curb to house, up to 500m)
- Tamper alert uses SF12 (max robustness — must reach hub even at range)

## Pin Assignments (STM32L011K4)

| Pin | Function | Notes |
|-----|----------|-------|
| PA0 | EXTI (wake) | Reed switch (door open) |
| PA1 | GPIO | HX711 SCK |
| PA2 | GPIO | HX711 DOUT |
| PA3 | 1-wire | DS18B20 temperature |
| PA4 | ADC | ALS-PT19 light |
| PA5 | GPIO/SPI CS | SX1261 CS |
| PA6-8 | SPI1 | SX1261 (MISO, MOSI, SCK) |
| PA9 | ADC | Solar voltage |
| PA10 | ADC | Battery voltage |
| PA11 | EXTI (wake) | LIS2DH12 INT1 (tamper) |
| PB0 | GPIO | SX1261 BUSY |
| PB1 | GPIO IRQ | SX1261 IRQ |
| PB2 | GPIO | SX1261 NRST |

## Schematic Files

- `mailbox-node.kicad_sch` — Main schematic
- `mailbox-node.kicad_pcb` — PCB layout (work in progress)
- `mailbox-node.kicad_pro` — KiCad project file

See BOM in `hardware/bom/mailbox_node_bom.csv` for component details.
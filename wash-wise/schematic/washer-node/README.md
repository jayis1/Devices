# WashWise washer-node Schematic

## KiCad Project

Open `washer-node.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB (Signal, GND, Power, Signal)
- Minimum trace width: 0.15mm (signal), 0.5mm (power), 1.0mm (pump power)
- Via size: 0.3mm drill, 0.6mm pad
- 50Ω controlled impedance for SX1261 RF traces
- Ground plane on layer 2

## Design Rules

- All decoupling capacitors within 2mm of IC power pins
- RF section (SX1261) isolated from pump motor drive (noise)
- Peristaltic pump driven via MOSFET with flyback diode
- HX711 load cell ADC: dedicated analog ground, star ground topology
- ACS712 current sensor: routed away from switching noise sources
- Flow sensor: hall-effect, needs 10kΩ pullup + 100nF decoupling
- DS18B20: 4.7kΩ pullup on 1-Wire data line
- Power: 12V for pump (via MP1584 buck to 5V for ESP32-S3), USB-C 5V backup

## Key Design Considerations

- ESP32-S3 handles sensors + pump + mesh (single core, FreeRTOS)
- ADXL313 mounted flat to washer cabinet (magnetic mount)
- Current sensor (ACS712) clamps onto washer power cord (non-invasive)
- Flow sensor splices into washer fill hose (or external non-invasive option)
- Load cell platform under detergent reservoir
- Peristaltic pump output feeds into washer dispenser drawer
- Pump compartment sealed from electronics (splash protection)

## Pin Assignments (ESP32-S3)

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO1 | ADC1_CH0 | ACS712 current sensor |
| GPIO2 | ADC1_CH1 | (spare analog) |
| GPIO3 | ADC1_CH2 | (spare analog) |
| GPIO4 | ADC1_CH3 | Battery voltage |
| GPIO5 | GPIO INT | YF-S201 flow sensor |
| GPIO6 | 1-Wire | DS18B20 water temp |
| GPIO7 | GPIO | HX711 DOUT (load cell) |
| GPIO8 | I2C SDA | ADXL313 + SHT40 |
| GPIO9 | I2C SCL | ADXL313 + SHT40 |
| GPIO10 | GPIO | HX711 SCK (load cell clock) |
| GPIO12-14 | SPI | SX1261 radio |
| GPIO15-18 | GPIO | SX1261 CS/BUSY/IRQ/NRST |
| GPIO4 | PWM (LEDC) | Peristaltic pump motor |

## Schematic Files

- `washer-node.kicad_sch` — Main schematic
- `washer-node.kicad_pcb` — PCB layout
- `washer-node.kicad_pro` — KiCad project file

See BOM in `hardware/bom/washer_node_bom.csv` for component details.
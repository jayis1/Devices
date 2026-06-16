# FreshKeep hub-node Schematic

## KiCad Project

Open `hub-node.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB (Signal, GND, Power, Signal)
- Minimum trace width: 0.15mm (signal), 0.5mm (power)
- Via size: 0.3mm drill, 0.6mm pad
- Impedance controlled: 50Ω for RF traces (SX1261/SX1262)
- Ground plane on layer 2 (unbroken for RF performance)

## Design Rules

- All decoupling capacitors within 2mm of IC power pins
- RF section isolated from digital section
- Analog sensor inputs have RC filters (100Ω + 100nF)
- Power supply sequencing: 3.3V before 1.8V
- Gas valve driver: IRLZ44N MOSFET with 10kΩ gate pull-down

## Schematic Files

- `hub-node.kicad_sch` — Main schematic
- `hub-node.kicad_pcb` — PCB layout (work in progress)
- `hub-node.kicad_pro` — KiCad project file

See BOM in `hardware/bom/hub-node-bom.csv` for component details.


# CradleKeep crib-pad Schematic

## KiCad Project

Open `crib-pad.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 2-layer flex PCB (designed to be ultra-thin: 0.8mm total)
- 200mm × 300mm — sized to fit under standard crib mattress
- FSR sensors positioned at head, chest, left hip, right hip positions
- Conductive wetness traces woven into flex PCB in diaper zone area

## Design Rules

- Flex PCB: 0.1mm polyimide, 0.5oz copper
- Minimum trace width: 0.2mm (flex), 0.15mm (rigid sections)
- Via size: 0.2mm laser drill (flex), 0.3mm mechanical (rigid)
- Bend radius: minimum 5mm (no components in bend zones)
- All components on rigid tail section (outside mattress area)
- FSR connection: conductive adhesive + Zebra connector
- Battery holder on rigid section near edge of pad

## Key Design Considerations

- **Ultra-low power is critical**: CR2450 coin cell must last 18+ months
  - STM32L476 Stop mode: 0.8µA (RTC running)
  - Wake every 5ms for FSR sampling (200Hz), process, sleep
  - SX1261: TX burst every 2s (normal) or 500ms (alert)
  - Total average: <10µA including radio TX
- **FSR placement**: 4 force-sensitive resistors at anatomically meaningful positions
  - FSR1 (head): detects head movement and breathing
  - FSR2 (chest): primary BCG breathing detection
  - FSR3 (left hip): position detection + movement
  - FSR4 (right hip): position detection + movement
- **Wetness detection**: Two interleaved conductive traces (copper + gold plating) in the diaper zone area. When wet, conductivity increases between traces.
- **Safety**: No battery on the mattress surface — CR2450 in rigid section at edge. No sharp components. No heat-generating components.

## Schematic Files

- `crib-pad.kicad_sch` — Main schematic
- `crib-pad.kicad_pcb` — PCB layout (work in progress)
- `crib-pad.kicad_pro` — KiCad project file

See BOM in `hardware/bom/crib-pad-bom.csv` for component details.
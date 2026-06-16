# CradleKeep nursery-monitor Schematic

## KiCad Project

Open `nursery-monitor.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB (Signal, GND, Power, Signal)
- 60mm × 50mm — wall-mount form factor with camera + sensor board
- Camera module connected via parallel interface (ESP32-S3 DVP)
- Dual MEMS microphones for beamforming (5mm separation)

## Design Rules

- All decoupling capacitors within 2mm of IC power pins
- RF section (868MHz + 2.4GHz) isolated from digital section
- Camera interface: 8-bit parallel, 20MHz pixel clock
- I2S audio traces: matched length, 50Ω impedance
- IR LED driver: MOSFET with 100Ω gate resistor, 1kHz PWM
- Ground plane on layer 2 (unbroken for RF performance)

## Key Design Considerations

- ESP32-S3 dual-core: Core 0 runs mesh + sensors, Core 1 runs audio ML
- 940nm IR LEDs invisible to human eye (no red glow) — safe for baby's sleep
- IR cut filter (motorized) switches between day and night modes
- Dual SPH0645 microphones at 5mm separation for basic beamforming
- SCD30 CO2 sensor has built-in auto-calibration — mount away from direct exhalation
- SGP40 VOC sensor requires 200ms measurement time — schedule appropriately
- VEML7700 light sensor has automatic gain — works from 0-120000 lux
- PIR-like baby detection: camera + motion analysis (not actual PIR sensor)

## Schematic Files

- `nursery-monitor.kicad_sch` — Main schematic
- `nursery-monitor.kicad_pcb` — PCB layout (work in progress)
- `nursery-monitor.kicad_pro` — KiCad project file

See BOM in `hardware/bom/nursery-monitor-bom.csv` for component details.
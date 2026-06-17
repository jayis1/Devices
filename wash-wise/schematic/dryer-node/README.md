# WashWise dryer-node Schematic

## KiCad Project

Open `dryer-node.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB (Signal, GND, Power, Signal)
- Minimum trace width: 0.15mm (signal), 0.5mm (power)
- Via size: 0.3mm drill, 0.6mm pad
- 50Ω controlled impedance for SX1261 RF traces
- Ground plane on layer 2
- **Heat-tolerant design:** components rated to 85°C ambient (dryer proximity)

## Design Rules

- All decoupling capacitors within 2mm of IC power pins
- RF section (SX1261) isolated from sensor analog section
- K-type thermocouple: dedicated cold-junction compensation (MAX6675 built-in)
- MAX6675 SPI lines: short, with 100Ω series resistors for noise immunity
- MPXV7002DP: differential pressure — two silicone tubes to exhaust duct
  - Tap 1: pre-lint-trap (warm side)
  - Tap 2: post-lint-trap (ambient side)
  - Pressure difference indicates lint accumulation
- ACS712: clamps onto dryer power cord (non-invasive, 30A rated)
- Battery backup: 500mAh LiPo — runs 2+ hours if USB power lost
- MQ-2 smoke sensor: optional, mount near exhaust vent

## Key Design Considerations

- ⚠️ **SAFETY-CRITICAL NODE** — must always be powered, battery backup essential
- ESP32-S3 runs all sensors + mesh + local fire risk heuristic
- K-type thermocouple wires: use high-temperature PTFE insulation (rated to 260°C)
- Thermocouple junction attached to exhaust duct exterior with high-temp foil tape
- Differential pressure taps: silicone tubing rated to 200°C
- Enclosure: IP54, polycarbonate, rated to 80°C continuous
- Deep sleep when idle + battery < 20% (but NOT if dryer was recently running)
- Fire alert broadcast uses SF10 (long range, robust) — overrides TDMA

## Pin Assignments (ESP32-S3)

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO1 | ADC1_CH0 | MPXV7002DP differential pressure |
| GPIO2 | ADC1_CH1 | ACS712 current sensor (dryer power) |
| GPIO3 | ADC1_CH2 | MQ-2 smoke sensor (optional) |
| GPIO4 | ADC1_CH3 | Battery voltage divider |
| GPIO8 | I2C SDA | SHT40 humidity + ADXL313 vibration |
| GPIO9 | I2C SCL | SHT40 humidity + ADXL313 vibration |
| GPIO9-11 | SPI | MAX6675 thermocouple ADC |
| GPIO12-14 | SPI | SX1261 radio |
| GPIO15-18 | GPIO | SX1261 CS/BUSY/IRQ/NRST |

## Sensor Placement

```
Dryer exhaust duct:

   ┌─────────────────────────────────────────┐
   │ Dryer │──→ [LINT TRAP] ──→ [EXHAUST DUCT] ──→ [VENT]
   └─────────────────────────────────────────┘
                    ↑ tap 1        ↑ tap 2        ↑ thermocouple
                    (pre-lint)     (post-lint)    (exhaust temp)
                    ↑ humidity sensor (in exhaust stream)
```

## Schematic Files

- `dryer-node.kicad_sch` — Main schematic
- `dryer-node.kicad_pcb` — PCB layout
- `dryer-node.kicad_pro` — KiCad project file

See BOM in `hardware/bom/dryer_node_bom.csv` for component details.
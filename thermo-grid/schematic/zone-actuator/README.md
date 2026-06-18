# ThermoGrid zone-actuator Schematic

## KiCad Project

Open `zone-actuator.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB (Signal, GND, Power, Signal), 100×60mm
- Minimum trace width: 0.15mm (signal), 0.8mm (power — motor current)
- Via size: 0.3mm drill, 0.6mm pad (0.4mm for power traces)
- Impedance controlled: 50Ω for RF traces (SX1261)
- Ground plane on layer 2 (unbroken for RF performance)
- **DIN-rail or junction box enclosure** — near radiator / air handler / manifold

## Design Rules

- All decoupling capacitors within 2mm of IC power pins
- RF section (SX1261) isolated from digital section with grounded guard vias
- Motor driver (A4988) needs heatsink pad + thick power traces
- Relay outputs: opto-isolated, with flyback/snubber for inductive loads
- 24VAC input → MP1584 buck → 5V → AP2112-3.3 (logic)
- Motor power from 24VAC directly (for Danfoss RA2) or 12V (for servo damper)
- DS18B20 on OneWire — 4.7kΩ pullup resistor
- Flow sensor (YF-S201) on interrupt pin with pullup

## Key Design Considerations

- ESP32-C3: cost-effective RISC-V with WiFi (optional fallback to cloud)
- SX1261 Sub-GHz for mesh communication with hub
- Three actuator types supported (selectable via jumper or config):
  1. **Motorized radiator valve** (Danfoss RA2): H-bridge (A4988) drives valve motor
     Full stroke ≈ 60s, PWM speed control for precision
  2. **HVAC damper** (MG996R servo): PWM position 0-90°
     1ms = closed, 2ms = fully open (50Hz PWM)
  3. **Relay output** (bang-bang): for radiant zone valves, electric baseboard
     Hysteresis ±0.5°C to prevent relay chatter
- PID control loop: uses room temp from hub (relayed from room sensor) as feedback
- Pipe temp (DS18B20) as secondary feedback + energy accounting
- Optional flow meter (YF-S201) for BTU energy measurement (hydronic systems)
- Failsafe: if hub offline >10min, holds last setpoint or enters frost protection (5°C)
- Power: 24VAC (from boiler transformer) or 4× AA with boost

## Pin Assignments (ESP32-C3)

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO0 | H-bridge A | Valve open direction (radiator mode) |
| GPIO1 | H-bridge B | Valve close direction (radiator mode) |
| GPIO2 | OneWire | DS18B20 pipe/floor temp |
| GPIO3 | LEDC PWM | Valve motor PWM / damper servo PWM |
| GPIO4-6 | SPI | SX1261 radio (SCK/MOSI/MISO) |
| GPIO7 | GPIO | SX1261 CS |
| GPIO8 | GPIO | SX1261 BUSY |
| GPIO9 | GPIO | SX1261 IRQ |
| GPIO10 | GPIO | SX1261 NRST |
| GPIO19 | GPIO EXTI | YF-S201 flow sensor (pulse counter) |
| GPIO20 | GPIO | Relay 1 (zone valve) |
| GPIO21 | GPIO | Relay 2 (boiler / heat-pump) |

## Actuator Type Selection

Configure via jumper or firmware config:

| Jumper | Actuator Type | Notes |
|--------|--------------|-------|
| Open | Radiator valve (PWM H-bridge) | Danfoss RA2 M30×1.5 |
| Short | Damper servo (PWM position) | MG996R, 50Hz |
| 2-3 | Relay (bang-bang) | On/off zone valve |

## Schematic Files

- `zone-actuator.kicad_sch` — Main schematic
- `zone-actuator.kicad_pcb` — PCB layout (work in progress)
- `zone-actuator.kicad_pro` — KiCad project file

See BOM in `hardware/bom/zone_actuator_bom.csv` for component details.
# ThermoGrid room-sensor Schematic

## KiCad Project

Open `room-sensor.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB (Signal, GND, Power, Signal), 80×50mm
- Minimum trace width: 0.15mm (signal), 0.5mm (power)
- Via size: 0.3mm drill, 0.6mm pad
- **Vented enclosure** — sensors must have airflow (critical for humidity + air velocity accuracy)
- MLX90640 thermal IR array needs unobstructed FOV to walls/floor
- Ground plane on layer 2 (unbroken for RF performance)

## Design Rules

- All decoupling capacitors within 2mm of IC power pins
- STM32WL55JC integrated Sub-GHz radio — no external radio chip needed!
  PCB trace antenna or small SMA whip for 915MHz
- MLX90640 on I2C1 (timing-isolated from other sensors — it needs clock stretching)
- SHT45, BMP390, SCD41 on I2C2
- SDP810 (differential pressure) needs a physical port to room air — use gasketed port
- mmWave (HLK-LD2410B) on UART1 (115200 baud)
- PIR (AM612) on GPIO interrupt (wakes from deep sleep)
- Power: 2× AA + 0.5W solar — MCP73831 trickle charges
  Solar panel connects via MCP73831 to a small NiMH or directly to AA (configurable)

## Key Design Considerations

- STM32WL55JC is the key choice: Cortex-M4 + LoRa in one chip, ultra-low-power
- MLX90640 16×12 thermal IR array: measures wall/floor/window surface temps → MRT
  This is the key differentiator — most thermostats only measure air temp
- SDP810 air velocity: detects drafts, HVAC airflow, open windows
- Deep sleep <8µA with RTC wakeup + PIR interrupt wakeup
- Sensor sampling burst: ~20mA for <1s every 30s → average ~0.7µA
- Solar panel extends battery life to 12+ months
- Vented enclosure critical: air must reach SHT45 (humidity), SDP810 (air velocity)
- MLX90640 FOV: mount at 1.5m height facing walls/floor (not ceiling)

## Pin Assignments (STM32WL55JC)

| Pin | Function | Notes |
|-----|----------|-------|
| PA0 | GPIO EXTI | AM612 PIR (edge wakeup) |
| PA1 | ADC | ALS-PT19 light sensor |
| PA2-3 | UART1 TX/RX | HLK-LD2410B mmWave |
| PA4 | ADC | Battery voltage divider |
| PA5 | ADC | Solar panel voltage |
| PA9-10 | I2C1 SDA/SCL | MLX90640 thermal IR |
| PB0 | GPIO | User calibration button |
| PB10-11 | I2C2 SDA/SCL | SHT45, BMP390, SCD41 |
| RF | internal | 915MHz Sub-GHz (integrated in STM32WL) |
| PB3 | GPIO | SDP810 power gate (MOSFET) |

## Sensor Placement (on PCB)

```
┌─────────────────────────────┐
│  [Solar Panel connector]     │
│                              │
│  ┌──────┐  ┌──────┐         │
│  │MLX   │  │SHT45 │  ← vents │
│  │90640 │  │      │         │
│  └──────┘  └──────┘         │
│  ┌──────┐  ┌──────┐         │
│  │SDP   │  │BMP390│         │
│  │810   │  │      │         │
│  └──────┘  └──────┘         │
│  ┌──────┐  ┌──────┐         │
│  │STM32 │  │SCD41 │         │
│  │WL55  │  │(opt) │         │
│  └──────┘  └──────┘         │
│  [PIR]  [mmWave]  [AA bat]   │
│  [PCB trace antenna]         │
└─────────────────────────────┘
```

## Schematic Files

- `room-sensor.kicad_sch` — Main schematic
- `room-sensor.kicad_pcb` — PCB layout (work in progress)
- `room-sensor.kicad_pro` — KiCad project file

See BOM in `hardware/bom/room_sensor_bom.csv` for component details.
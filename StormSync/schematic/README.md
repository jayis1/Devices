# StormSync — Schematics

## Overview

Each hardware node has its own schematic documented in a subfolder:

```
schematic/
├── README.md              # This file
├── hub/                   # Hub/Gateway (ESP32-S3 + SX1262 + SIM7000)
├── sump-sentinel/         # Sump Pit Sentinel (ESP32 + ultrasonic + CT clamp + ADXL355)
├── saturation-probe/      # Soil Saturation Probe (nRF52840 + FDC2214 + 3-depth)
├── weather-sentinel/      # Weather Sentinel (ESP32-S3 + BME280 + rain gauge)
└── flood-actuator/        # Flood Actuator (ESP32 + motorized valve + backup pump relay)
```

## Design Notes

- All Sub-GHz nodes use SX1262 at 868 MHz (EU) / 915 MHz (US)
- 4-layer PCBs: signal / GND / power / signal
- SX1262 ground plane directly below radio, keep SPI traces <50mm
- Solar nodes: MCP73871 charger + LiFePO4 battery
- Sump Sentinel & Flood Actuator: 12V SLA battery backup (critical for power outages)
- Hub: 4G LTE cellular backup for alerts when Wi-Fi is down
- All outdoor enclosures: IP65+ minimum

## KiCad Projects

Each subfolder contains a README with detailed block diagram, pin assignments, power architecture, and PCB layout notes. In a full production build, KiCad `.kicad_sch`, `.kicad_pcb`, and `.kicad_pro` files would be included.
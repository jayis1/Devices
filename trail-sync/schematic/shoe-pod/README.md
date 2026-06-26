# TrailSync — Shoe Pod Schematic

KiCad schematic for the TrailSync Shoe Pod (nRF52833 + IMU + pressure insole).

## Key Components
- nRF52833 (Sub-GHz + on-device gait CNN)
- SX1262 (Sub-GHz 868/915 MHz radio)
- LSM6DSL (3D accel ±16G + 3D gyro ±2000dps)
- 24× FSR pressure insole array
- 2× strain gauge + HX711 (ground reaction force)
- CR2477 coin cell holder

## Power
- CR2477 (1000 mAh) — 30+ day life
- 3.3V LDO (TPS62825)
- Deep sleep ~3 µA between runs
- Auto-wake on IMU motion threshold

## PCB
- 4-layer FR4, 45×28mm
- Fits inside shoe midsole pocket or lace clip
- IP67 polyurethane enclosure

*Full KiCad project files to be added.*
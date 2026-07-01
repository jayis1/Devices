# MigraineSync — Env Sentinel Schematic

## SoC: ESP32-S3-WROOM-1-N8R2

### Power
- USB-C 5V → 3.3V LDO (AMS1117-3.3) → ESP32-S3 + sensors
- Optional 18650 via TP4056 → 3.3V LDO (backup)

### I²C Bus (via TCA9548A Mux at 0x70)
| Signal | GPIO |
|--------|------|
| SCL    | 8    |
| SDA    | 9    |

### TCA9548A Channel Assignments
| Ch | Device     | Addr  | Notes                        |
|----|------------|-------|------------------------------|
| 0  | BMP390     | 0x76  | Barometric pressure          |
| 1  | SPL06-007  | 0x76  | Sound level (same addr!)     |
| 2  | VEML7700   | 0x10  | Ambient light                |
| 2  | SHT45      | 0x44  | Temp + humidity (shared ch)  |
| 3  | BME688     | 0x77  | VOC / IAQ                    |
| 3  | SCD41      | 0x62  | CO₂ NDIR (shared ch)         |

### SPI Bus (VSPI): SX1262
| Signal   | GPIO |
|----------|------|
| SCK      | 14   |
| MOSI     | 16   |
| MISO     | 15   |
| CS       | 10   |
| DIO1 IRQ | 11   |
| BUSY     | 12   |
| RESET    | 13   |

### GPIO
| Function      | GPIO |
|---------------|------|
| STATUS_LED    | 18   |
| BATTERY_ADC   | 17   |

### Sensor Details
- **BMP390**: Pressure ±3 Pa, 0.016 hPa RMS noise, 1 Hz poll
- **VEML7700**: 0.0036 lux resolution, 0.5 Hz poll
- **BME688**: Uses Bosch BSEC2 library for IAQ (requires license-free download)
- **SCD41**: NDIR CO₂, ±40 ppm, periodic measurement mode
- **SHT45**: ±0.1°C / ±1.5% RH (best-in-class)
- **SPL06-007**: 30-120 dB SPL, internal DSP

### KiCad Project
- `env-sentinel.kicad_pro` — KiCad 7+ project
- `env-sentinel.kicad_sch` — main schematic
- `env-sentinel.kicad_pcb` — 4-layer PCB (70×50 mm)

### PCB Notes
- Sensor section separated from RF section with ground plane isolation
- SCD41 requires minimum 2.5V; ensure LDO can supply all sensors simultaneously
- BME688 heater draws ~15 mA peak; budget power accordingly
- Ventilation holes in enclosure for gas + CO₂ sensors
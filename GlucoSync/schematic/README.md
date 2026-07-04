# GlucoSync Schematics

KiCad schematic files for each hardware node:

| Node | File | SoC |
|------|------|-----|
| Metabolic Hub | `hub/glucosync_hub.kicad_sch` | ESP32-S3-WROOM-1-N8R2 |
| Meal Scanner | `meal-scanner/glucosync_scanner.kicad_sch` | ESP32-S3-WROOM-1-N8R2 |
| Activity Band | `activity-band/glucosync_band.kicad_sch` | nRF52840 (Fanstel BT840) |
| Insulin Pen Tag | `pen-tag/glucosync_pen.kicad_sch` | nRF52840 (Fanstel BT840) |

## Key design notes

### Metabolic Hub
- 4-layer PCB (signal / ground / 3.3V / signal)
- ESP32-S3 with 8 MB flash + 2 MB PSRAM
- UC8151D e-ink display via SPI (GPIO4-9)
- MAX98357A I²S amplifier on GPIO39-42
- LSM6DSO IMU on I²C (GPIO37-38)
- 18650 Li-ion UPS with MCP73831 charger

### Meal Scanner
- 4-layer PCB, handheld wand form factor
- ESP32-S3 with 8 MB flash + 2 MB PSRAM for camera buffers
- OV5640 camera via DVP 8-bit parallel (GPIO4-18)
- 5× spectral LEDs on individual MOSFET drivers (GPIO21, 37-40)
- BME280 ambient sensor on I²C (GPIO41-42)
- 500 mAh LiPo with MCP73831 charger, USB-C

### Activity Band
- 4-layer PCB, wrist band form factor (45×25mm)
- nRF52840 (Fanstel BT840 module)
- MAX30101 PPG on I²C (P0.24-26)
- LSM6DSO IMU on I²C #2 (P0.27-29)
- DRV2605L haptic driver + ERM on P0.06
- CR2477 coin cell (1000 mAh, ~90 days)

### Insulin Pen Tag
- 4-layer PCB, clip-on form factor (30×15mm)
- nRF52840 (Fanstel BT840 module)
- LSM6DSO IMU on I²C (P0.24-26)
- CR2477 coin cell (1000 mAh, ~180 days)
- Spring clip mechanism for pen attachment
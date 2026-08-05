# SeizureSync — Seizure Band KiCad Project

KiCad project for the SeizureSync Seizure Band (wrist-worn detector).

## Files
- `seizure-band.kicad_sch` — schematic
- `seizure-band.kicad_pcb` — PCB layout (flex-rigid, 45×35 mm)

## Key ICs
- ESP32-S3-MINI-1-N8R2 (main SoC)
- SX1262IMLTIC (Sub-GHz 868 MHz)
- ICM-42688-P (6-axis IMU, 2000 Hz)
- MAX30102GND+ (PPG/SpO₂)
- AD5940ACPZ-R7 (EDA impedance)
- DRV2605LDGTR (haptic driver)
- ST7789V (1.3″ LCD driver)
- BQ25895RTWR (PMIC/charger)

See `hardware/bom/SeizureBand_BOM.csv`.
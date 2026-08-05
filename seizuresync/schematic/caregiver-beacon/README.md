# SeizureSync — Caregiver Beacon KiCad Project

KiCad project for the SeizureSync Caregiver Beacon (portable alert, 7-day).

## Files
- `caregiver-beacon.kicad_sch` — schematic
- `caregiver-beacon.kicad_pcb` — PCB layout (4-layer FR4, 90×60 mm)

## Key ICs
- ESP32-C3-MINI-1-N4 (RISC-V SoC)
- SX1262IMLTIC (Sub-GHz 868 MHz)
- UC8151d (2.9″ e-ink driver)
- DRV2605LDGTR (haptic driver)
- MAX98357A (I²S audio amp)
- BQ25895RTWR (PMIC/charger)

See `hardware/bom/CaregiverBeacon_BOM.csv`.
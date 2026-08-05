# SeizureSync — Aura Patch KiCad Project

KiCad project for the SeizureSync Aura Patch (chest-worn disposable, 14-day).

## Files
- `aura-patch.kicad_sch` — schematic
- `aura-patch.kicad_pcb` — PCB layout (2-layer FR4 flex, 35×25 mm)

## Key ICs
- nRF52840-QFAA-R (BLE 5.0 SoC, ultra-low power)
- TMP117AIDRVR (medical-grade temp sensor, ±0.1°C)
- AD8232ACPZ-WP (biopotential AFE for EDA)
- MAX30101GND+ (low-power PPG)

See `hardware/bom/AuraPatch_BOM.csv`.
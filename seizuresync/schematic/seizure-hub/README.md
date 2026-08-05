# SeizureSync — Seizure Hub KiCad Project

This directory contains the KiCad project files for the SeizureSync
Seizure Hub (bedside coordinator node).

## Files
- `seizure-hub.kicad_sch` — schematic
- `seizure-hub.kicad_pcb` — PCB layout

## Key ICs
- ESP32-S3-WROOM-1-N16R8 (main SoC)
- SX1262IMLTIC (Sub-GHz 868 MHz radio)
- MLX90640ESF-BCA (32×24 IR thermal array)
- MAX30102GND+ (PPG/SpO₂)
- SCD41-D-R2 (CO₂)
- SIM7600G-C-SE (4G LTE backup)
- UC8151d (2.9″ e-ink driver)
- LTC4040IM (UPS power-path manager)

## Pin assignments
See main README.md pin assignment table.

## BOM
See `hardware/bom/SeizureHub_BOM.csv`.
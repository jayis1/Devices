# VoiceSync — Schematics

## Overview

Each hardware node has its own schematic documented in a subfolder:

| Node | SoC | Key ICs | Schematic |
|------|-----|---------|----------|
| Voice Hub | ESP32-S3 | SX1262, BME280, DS3231 | [hub/README.md](hub/README.md) |
| Vocal Band | nRF52840 | NAU88C22, LSM6DS3, TMP117, MAX30102 | [vocal-band/README.md](vocal-band/README.md) |
| Room Sentinel | ESP32-S3 | SX1262, 4×ICS-43434, SHT40, SGP40 | [room-sentinel/README.md](room-sentinel/README.md) |
| Hydration Tag | nRF52840 | HX711, LIS2DW12, load cell | [hydration-tag/README.md](hydration-tag/README.md) |
| Humidity Node | ESP32 | SX1262, SHT40, HC-SR04, 2×relay | [humidity-node/README.md](humidity-node/README.md) |

## KiCad Projects

In production, each folder contains a full KiCad project with:
- `.kicad_pro` — KiCad project file
- `.kicad_sch` — Schematic file
- `.kicad_pcb` — PCB layout file
- `gerbers/` — Manufacturing files

## Design Notes

- All Sub-GHz nodes use SX1262 at 868 MHz (EU) / 915 MHz (US)
- BLE wearable nodes (Vocal Band, Hydration Tag) use nRF52840 BLE 5.0
- 4-layer PCBs for hub and room sentinel (RF + audio), 2-layer for simpler nodes
- RF sections: 50Ω impedance, keep antenna traces short
- Audio sections: shielded, away from digital noise sources
- Power: USB-C for always-on nodes, LiPo/CR2032 for wearables
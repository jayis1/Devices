# LawnSync Schematics

This directory contains KiCad schematic projects for each hardware node in the LawnSync system.

## Nodes

| Node | SoC | Schematic File | Description |
|------|-----|----------------|-------------|
| Hub / Gateway | ESP32-S3-WROOM-1 | `hub/` | Central coordinator, Wi-Fi/MQTT, Sub-GHz mesh |
| Soil Sensor Node | nRF52840 + SX1262 | `soil-node/` | Solar-powered multi-parameter soil sensor |
| Sprinkler Controller | ESP32-WROOM-32E | `sprinkler/` | 8-zone valve controller with flow/pressure/rain |
| Weather Station | ESP32-S3-WROOM-1 | `weather-node/` | Solar-powered weather monitoring |
| Lawn Scanner | ESP32-S3-WROOM-1 | `scanner-node/` | Multispectral imaging + edge ML |

## Design Notes

- All radio nodes use SX1262/SX1276 at 868 MHz (EU) / 915 MHz (US) — change crystal and matching network for regional bands.
- Solar nodes use MCP73871 charger + LiFePO4 cells (3.2V nominal, safer than LiPo for outdoor deployment).
- Soil nodes use capacitive moisture sensing (FDC2214) — no corrosion, unlike resistive probes.
- Sprinkler controller includes TVS diodes + MOVs on all valve outputs for inductive spike protection.
- All nodes include programming headers (SWD for nRF52, UART for ESP32).

## Opening in KiCad

Each subfolder contains:
- `*.kicad_pro` — KiCad project file
- `*.kicad_sch` — Schematic file
- `*.kicad_pcb` — PCB layout file (when available)

Open the `.kicad_pro` file in KiCad 7+.
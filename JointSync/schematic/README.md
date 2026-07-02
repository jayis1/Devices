# JointSync Schematics

KiCad 7+ schematic projects for each hardware node.

## Nodes

| Node | SoC | File |
|------|-----|------|
| Hub | ESP32-S3-WROOM-1-N8R8 | `hub/` |
| Joint Tag | nRF52840 | `joint-tag/` |
| Compression Sleeve | ESP32-S3-MINI-1 | `compression-sleeve/` |
| Joint Scanner | ESP32-S3-WROOM-1-N8R8 | `joint-scanner/` |

## Opening

```bash
# Open in KiCad
kicad hub/jointsync_hub.kicad_pro

# Open individual schematic
eeschema hub/jointsync_hub.kicad_sch
```

## Notes

- All schematics are 4-layer (Hub, Tag, Sleeve) or 6-layer (Scanner) PCBs
- Impedance-controlled traces for USB-C (90Ω differential) and SPI
- Sub-GHz 868 MHz RF section follows CC1120 reference design
- See `../hardware/bom/` for component details
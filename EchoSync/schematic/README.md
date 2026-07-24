# EchoSync — Schematics

This directory contains KiCad schematic projects for each hardware node.

## Nodes

| Subfolder | Node | SoC | Description |
|-----------|------|-----|-------------|
| `hub/` | Echo Hub | ESP32-S3-WROOM-1-N16R8 | Gateway, mesh coordinator, display, bed-shaker relay |
| `room-sentinel/` | Room Sentinel | ESP32-S3-WROOM-1-N8R2 | 4-mic I²S array, SoundNet CNN, SHT40, SX1262 |
| `wrist-band/` | Wrist Band | nRF52840 QFAA | Haptic motor, OLED, IMU, BLE 5.0, LiPo |
| `door-tag/` | Door Tag | nRF52840 QFAA | Piezo contact sensor, I²S mic, CR2032, BLE 5.0 |

Each subfolder contains:
- `README.md` — Schematic description, block diagram, pin assignments
- KiCad `.kicad_sch` project files (to be created in KiCad)

## Power Architecture

```
USB-C 5V ──→ TPS25940 eFuse ──→ AMS1117-3.3 LDO ──→ 3.3V
                                                    │
              ┌─────────────────────────────────────┤
              │                                     │
    ESP32-S3 SoC      SX1262 Radio     Peripherals (BME280, SHT40, etc.)
```

Battery-powered nodes (Wrist Band, Door Tag) use:
- nRF52840 ultra-low-power modes (System ON, RAM retention)
- Duty-cycled sensors (mic enabled only when needed)
- CR2032 (Door Tag) or LiPo 300mAh (Wrist Band)
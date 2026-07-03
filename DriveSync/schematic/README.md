# DriveSync Schematics

This directory contains KiCad schematic files for each hardware node.

## Nodes

| Node | File | SoC |
|------|------|-----|
| Dash Hub | `hub/drivesync_hub.kicad_sch` | ESP32-S3-WROOM-1-N8R8 |
| Steering Wheel Node | `wheel-node/drivesync_wheel.kicad_sch` | nRF52840 |
| Seat Belt Tag | `belt-tag/drivesync_belt.kicad_sch` | nRF52840 |
| OBD-II Dongle | `obd-dongle/drivesync_obd.kicad_sch` | RP2040 |

Open with KiCad 7+. Each schematic includes:
- MCU and all peripheral ICs
- Power regulation
- Sensor connections with pin assignments
- Communication buses (I²C, SPI, I²S, DVP, UART)
- LED and haptic motor drivers

See the main README for detailed pin assignments.
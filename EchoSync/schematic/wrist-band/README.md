# Wrist Band — Schematic

## Block Diagram

```
┌────────────────────────────────────────────────────────────┐
│                  WRIST BAND (nRF52840)                     │
│                                                            │
│  ┌─────────────┐  ┌──────────┐  ┌──────────────┐         │
│  │ nRF52840    │  │ DRV2605L │  │ SSD1306      │         │
│  │ QFAA        │  │ Haptic   │  │ 0.96" OLED   │         │
│  │ 1MB Flash   │  │ Driver   │  │ 128×64       │         │
│  │ 256KB RAM   │  │ I²C 0x5A │  │ I²C 0x3C     │         │
│  │ BLE 5.0     │  └────┬─────┘  └──────┬───────┘         │
│  └──────┬──────┘       │               │                  │
│         │              │ I²C           │ I²C              │
│         │ I²C          │               │                  │
│  ┌──────┴──────┐  ┌────┴────┐   ┌──────┴───────┐         │
│  │ LSM6DS3TR-C │  │ LRA     │   │ OLED Display │         │
│  │ 6-axis IMU  │  │ Haptic  │   │ Sound icon + │         │
│  │ I²C 0x6A    │  │ Motor   │   │ direction    │         │
│  │ Sleep detect│  │ VM-1207 │   │              │         │
│  └─────────────┘  └─────────┘   └──────────────┘         │
│                                                            │
│  ┌─────────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐ │
│  │ MCP73831    │  │ LiPo     │  │ Buttons  │  │ SK6812 │ │
│  │ Charger     │  │ 300mAh   │  │ A + B    │  │ RGB    │ │
│  │ USB-C 100mA │  │ 3.7V     │  │ GPIO     │  │ LED    │ │
│  └─────────────┘  └──────────┘  └──────────┘  └────────┘ │
│                                                            │
│  ┌──────────────────────────────────────────────────────┐ │
│  │ PCB Trace Antenna (BLE 5.0 chip antenna)            │ │
│  └──────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────┘
```

## Power Architecture

- **Battery:** 3.7V 300 mAh LiPo (rechargeable)
- **Charger:** MCP73831 via USB-C (100 mA charge rate, ~3 hours full charge)
- **Regulation:** nRF52840 internal regulators (3.0V DCDC, 1.8V for peripherals)
- **Battery Life:** ~3 days with continuous BLE + IMU + display updates
- **Power Management:** System ON sleep between BLE events, display off when idle

## Key ICs

| IC | Function | Package | Interface |
|----|----------|---------|-----------|
| nRF52840 QFAA | Main MCU, BLE 5.0 | QFN-73 | — |
| DRV2605L | Haptic driver (123 waveforms) | WLCSP-9 | I²C 0x5A |
| VM-1207 | LRA haptic motor | Can | DRV2605L |
| SSD1306 | 0.96" OLED controller | COB | I²C 0x3C |
| LSM6DS3TR-C | 6-axis IMU | LGA-14 | I²C 0x6A |
| MCP73831 | LiPo charger | SOT-23-5 | GPIO |
| SK6812 | RGB status LED | 5050 | GPIO (NRZ) |

## Pin Assignments

See `firmware/common/config.h` for complete pin assignments.

### I²C buses:
- **I2C_0:** OLED (0x3C) + DRV2605L (0x5A) + IMU (0x6A) — shared bus

### GPIO:
- Buttons: P0.12 (A=dismiss), P0.13 (B=menu)
- Haptic enable: P0.15 (MCP73831 STAT → P0.14)
- OLED RST: P0.16
- LED: P0.09 (SK6812)
- VBAT ADC: P0.10
- USB detect: P0.11

## Enclosure

- **Material:** Silicone sport band (hypoallergenic, IP67 waterproof)
- **PCB:** Rigid-flex, fits inside silicone housing
- **Display window:** 0.96" cutout with clear TPU
- **Haptic motor:** Mounted on PCB underside, contacts wrist
- **USB-C:** Side-mounted, water-resistant cap
- **Size:** 42mm case, fits 18-22mm bands
# Nursing Sensor Schematic

## Overview

The Nursing Sensor is an adhesive breast patch worn during the breastfeeding period. It contains dual TMP117 temperature sensors (one per breast) for bilateral temperature asymmetry detection (mastitis early warning) and an LIS2DW12 accelerometer for nursing position detection.

## SoC: nRF52840 QFAA

- ARM Cortex-M4F @ 64 MHz
- BLE 5.0, 1 MB flash, 256 KB RAM

## Pin Assignments (nRF52840 P0.x)

| Pin | Function | Bus | Notes |
|-----|----------|-----|-------|
| P0.02 | TMP117 Left INT | GPIO | Left breast temp alert |
| P0.03 | TMP117 Right INT | GPIO | Right breast temp alert |
| P0.04 | I²C SDA | I²C0 | Both TMP117 + LIS2DW12 |
| P0.05 | I²C SCL | I²C0 | Shared I²C |
| P0.06 | LIS2DW12 INT1 | GPIO | IMU interrupt |
| P0.07 | LED | GPIO | Status blink |
| P0.08 | Button | GPIO | User button |

## I²C Address Map

| Device | Address | Notes |
|--------|---------|-------|
| TMP117 Left | 0x48 | GND on ADDR pin |
| TMP117 Right | 0x49 | VDD on ADDR pin |
| LIS2DW12 | 0x1E | SA0 = 0 |

## Power

- **Battery:** CR2032 220 mAh (replaceable)
- **LDO:** None — direct 3V from CR2032
- **Battery life:** 14 days (0.1 Hz temp + 12.5 Hz IMU)

## Block Diagram

```
┌──────────────────────────────────────────────────────────┐
│              NURSING SENSOR (nRF52840)                    │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ TMP117   │  │ TMP117   │  │ LIS2DW12 │               │
│  │ Left     │  │ Right    │  │ 3-axis   │               │
│  │ 0x48     │  │ 0x49     │  │ 0x1E     │               │
│  │ I²C      │  │ I²C      │  │ I²C      │               │
│  └──────────┘  └──────────┘  └──────────┘               │
│                                                          │
│  ┌──────────┐  ┌──────────┐                              │
│  │ CR2032   │  │ LED      │                              │
│  │ 220 mAh  │  │ Status   │                              │
│  └──────────┘  └──────────┘                              │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ BLE 5.0 (Coded PHY for range)                    │    │
│  │ PCB Trace Antenna                                 │    │
│  └──────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────┘
```
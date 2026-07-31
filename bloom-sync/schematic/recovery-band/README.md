# Recovery Band Schematic

## Overview

The Recovery Band is a wrist-worn wearable that continuously monitors maternal vital signs during the 6-week postpartum period. It contains a MAX30101 PPG sensor for heart rate/HRV/SpO₂, an LSM6DSO IMU for activity and sleep tracking, and a TMP117 skin temperature sensor.

## SoC: nRF52840 QFAA

- ARM Cortex-M4F @ 64 MHz
- BLE 5.0, 1 MB flash, 256 KB RAM
- Ultra-low-power for coin-cell/LiPo wearables

## Pin Assignments (nRF52840 P0.x)

| Pin | Function | Bus | Notes |
|-----|----------|-----|-------|
| P0.02 | MAX30101 INT | GPIO | PPG interrupt |
| P0.03 | I²C SDA | I²C0 | MAX30101 + TMP117 + MAX17048 |
| P0.04 | I²C SCL | I²C0 | Shared I²C |
| P0.05 | LSM6DSO INT1 | GPIO | IMU interrupt |
| P0.06 | LSM6DSO CS | SPI0 CS | IMU |
| P0.07 | SPI SCK | SPI0 | Shared SPI |
| P0.08 | SPI MISO | SPI0 | Shared SPI |
| P0.09 | SPI MOSI | SPI0 | Shared SPI |
| P0.10 | TMP117 INT | GPIO | Temp alert |
| P0.11 | Fuel Alert | GPIO | MAX17048 |
| P0.12 | LED | SK6812 | Status |
| P0.13 | Button | GPIO | User button |
| P0.14 | Charge Stat | GPIO | MCP73871 status |
| P0.15 | VBAT Sense | ADC | Battery voltage |

## Power

- **Battery:** 200 mAh LiPo (3.7V nominal)
- **Charger:** MCP73871 (USB-C, 100 mA)
- **LDO:** AP2112K-3.3 (3.7V → 3.3V)
- **Battery life:** 7 days (1 Hz vitals + 50 Hz IMU)

## Block Diagram

```
┌──────────────────────────────────────────────────────────┐
│               RECOVERY BAND (nRF52840)                    │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │
│  │ MAX30101 │  │ LSM6DSO  │  │ TMP117   │  │ MAX17048 │ │
│  │ PPG HR/  │  │ 6-axis   │  │ Skin Temp│  │ Fuel     │ │
│  │ SpO2/HRV │  │ IMU      │  │ ±0.1°C   │  │ Gauge    │ │
│  │ I²C      │  │ SPI      │  │ I²C      │  │ I²C      │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘ │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ USB-C    │  │ MCP73871 │  │ SK6812   │               │
│  │ Charging │  │ Charger  │  │ LED      │               │
│  └──────────┘  └──────────┘  └──────────┘               │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ BLE 5.0 (2M PHY + Coded PHY)                     │    │
│  │ PCB Trace Antenna                                 │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ Power: 200 mAh LiPo + AP2112K-3.3               │    │
│  └──────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────┘
```
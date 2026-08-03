# Wrist Band Schematic

## Overview

The Wrist Band is a wrist-worn wearable that continuously monitors physiological signals during menopause. It contains a MAX30101 PPG sensor for heart rate/HRV/SpO₂, a TMP117 skin temperature sensor for hot flash detection, an ADS1292 EDA sensor for stress/sympathetic arousal, and an LSM6DSO IMU for activity and sleep tracking.

## SoC: nRF52840 QFAA

- ARM Cortex-M4F @ 64 MHz
- BLE 5.0, 1 MB flash, 256 KB RAM
- Ultra-low-power for LiPo wearables

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
| P0.11 | ADS1292 DRDY | GPIO | EDA data ready |
| P0.12 | ADS1292 CS | SPI0 CS2 | EDA sensor |
| P0.13 | Fuel Alert | GPIO | MAX17048 |
| P0.14 | LED | SK6812 | Status |
| P0.15 | Button | GPIO | User button |
| P0.16 | Charge Stat | GPIO | MCP73871 status |
| P0.17 | VBAT Sense | ADC | Battery voltage |
| P0.20 | EDA Electrode A | GPIO/ADC | Skin conductance electrode |
| P0.21 | EDA Electrode B | GPIO/ADC | Skin conductance electrode |

## EDA Sensor Details

### ADS1292 — Electrodermal Activity
- 24-bit biopotential analog front-end
- Measures skin conductance between two stainless steel electrodes
- Excitation: 0.5V constant voltage across electrodes
- Skin conductance range: 0.5-50 µS (microsiemens)
- Sample rate: 4 Hz
- Tonic (baseline) + phasic (event-related) decomposition
- EDA spike precedes hot flash by 10-20 min — key predictive feature

## Power

- **Battery:** 200 mAh LiPo (3.7V nominal)
- **Charger:** MCP73871 (USB-C, 100 mA)
- **LDO:** AP2112K-3.3 (3.7V → 3.3V)
- **Battery life:** 7 days (1 Hz vitals + 4 Hz EDA + 50 Hz IMU)

## Block Diagram

```
┌──────────────────────────────────────────────────────────┐
│               WRIST BAND (nRF52840)                       │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │
│  │ MAX30101 │  │ TMP117   │  │ ADS1292  │  │ LSM6DSO  │ │
│  │ PPG HR/  │  │ Skin Temp│  │ EDA      │  │ 6-axis   │ │
│  │ SpO2/HRV │  │ ±0.1°C   │  │ Stress   │  │ IMU      │ │
│  │ I²C      │  │ I²C      │  │ SPI      │  │ SPI      │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘ │
│                                                          │
│  ┌──────────┐  ┌──────────┐                              │
│  │ EDA      │  │ MAX17048 │                              │
│  │ Electrode│  │ Fuel     │                              │
│  │ A+B      │  │ Gauge    │                              │
│  └──────────┘  └──────────┘                              │
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
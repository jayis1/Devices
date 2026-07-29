# Smart Band Schematic

## Overview

The Smart Band is a resistance band with an embedded handle unit containing a 50 kg load cell + HX711 24-bit ADC for force measurement, an LSM6DSO IMU for orientation/tempo tracking, and a BLE 5.0 radio for streaming data to the Hub.

## SoC: nRF52840 QFAA

- 1 MB flash, 256 KB RAM
- ARM Cortex-M4F @ 64 MHz
- BLE 5.0

## Pin Assignments

| Pin | Function | Bus | Notes |
|-----|----------|-----|-------|
| P0.02 | HX711 SCK | GPIO | Load cell clock (bit-banged) |
| P0.03 | HX711 DOUT | GPIO | Load cell data (bit-banged) |
| P0.04 | LSM6DSO CS | SPI0 CS | IMU chip select |
| P0.05 | SPI0 SCK | SPI0 | SPI clock |
| P0.06 | SPI0 MISO | SPI0 | SPI MISO |
| P0.07 | SPI0 MOSI | SPI0 | SPI MOSI |
| P0.08 | LSM6DSO INT1 | GPIO | IMU interrupt |
| P0.09 | I²C SDA | I²C | MAX17048 fuel gauge |
| P0.10 | I²C SCL | I²C | MAX17048 fuel gauge |
| P0.11 | Status LED | SK6812 | RGB LED |
| P0.12 | USB-C detect | GPIO | Charging status |
| P0.13 | Button | GPIO | Power / pairing |
| P0.15 | BLE Antenna | RF | PCB trace antenna |

## Power

- **Battery:** 3.7V LiPo 300 mAh (rechargeable)
- **Charger:** MCP73831 (USB-C, 100 mA charge current)
- **Boost:** TPS61023 (3.7V → 3.3V)
- **Battery life:** ~7 days at 1h/day exercise
- **Charge time:** ~2 hours via USB-C

## Load Cell Interface

- **Sensor:** YZZC CZL601 50 kg load cell (strain gauge, 2 mV/V)
- **ADC:** HX711 24-bit, gain=128, 5V excitation
- **Resolution:** 50 kg / 2^24 = 0.003 g → ~0.01 N
- **Sample rate:** 80 Hz (HX711 max)
- **Calibration:** 2-point (tare + known weight)
- **Temperature compensation:** via HX711 internal reference

## Block Diagram

```
┌──────────────────────────────────────────────────┐
│              SMART BAND (nRF52840)                │
│                                                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│  │ Load Cell│  │ HX711    │  │ LSM6DSO  │       │
│  │ 50 kg    │──│ 24-bit   │  │ IMU      │       │
│  │ Strain   │  │ ADC      │  │ Accel    │       │
│  │ Gauge    │  │ Gain 128 │  │ Gyro     │       │
│  └──────────┘  └──────────┘  └──────────┘       │
│                     │ GPIO         │ SPI0         │
│  ┌──────────────────────────────────────────┐   │
│  │           nRF52840 QFAA                  │   │
│  │  BLE 5.0 + Cortex-M4F @ 64 MHz         │   │
│  └──────────────────────────────────────────┘   │
│                     │                            │
│  ┌──────────┐  ┌────┴──────┐  ┌──────────┐     │
│  │ MAX17048 │  │ BLE Ant   │  │ MCP73831 │     │
│  │ Fuel Gauge│  │ PCB Trace│  │ Charger  │     │
│  │ I²C      │  │          │  │ USB-C    │     │
│  └──────────┘  └──────────┘  └──────────┘     │
│                                                  │
│  ┌──────────────────────────────────────────┐   │
│  │  LiPo 300 mAh 3.7V + TPS61023 Boost     │   │
│  └──────────────────────────────────────────┘   │
│                                                  │
│  Handle: 80mm × 40mm × 20mm                    │
│  Band: Fabric resistance band with load cell    │
└──────────────────────────────────────────────────┘
```
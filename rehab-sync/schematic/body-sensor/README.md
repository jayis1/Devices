# Body Sensor Schematic

## Overview

The Body Sensor is a wearable IMU node that attaches to body segments (thigh, shin, foot, upper arm, forearm, torso) via a silicone strap. It samples a 9-DoF IMU at 100 Hz, runs Madgwick AHRS quaternion computation, and streams orientation data to the Hub via BLE 5.0.

## SoC: nRF52840 QFAA

- 1 MB flash, 256 KB RAM
- ARM Cortex-M4F @ 64 MHz
- BLE 5.0 (coded PHY for range, 2M PHY for throughput)
- NFC-A (tap-to-pair)
- Ultra-low power: 1.7V-3.6V supply

## Pin Assignments

| Pin | Function | Bus | Notes |
|-----|----------|-----|-------|
| P0.02 | LSM6DSO CS | SPI0 CS | IMU chip select |
| P0.03 | LIS3MDL CS | SPI0 CS | Magnetometer CS |
| P0.04 | SPI0 SCK | SPI0 | Shared SPI clock |
| P0.05 | SPI0 MISO | SPI0 | Shared SPI MISO |
| P0.06 | SPI0 MOSI | SPI0 | Shared SPI MOSI |
| P0.07 | LSM6DSO INT1 | GPIO | IMU data-ready interrupt |
| P0.08 | LIS3MDL INT | GPIO | Mag data-ready interrupt |
| P0.09 | Status LED | SK6812 | Single RGB LED |
| P0.10 | BLE Antenna | RF | PCB trace antenna |
| P0.11 | Button | GPIO | Power / pairing |
| P0.13 | NFC | NFC-A | Tap-to-pair (optional) |

## Power

- **Battery:** CR2032 220 mAh (3V coin cell)
- **Direct 3V rail:** nRF52840 (1.7-3.6V), LSM6DSO (1.7-3.6V), LIS3MDL (1.7-3.6V)
- **Battery life:** ~30 days at 1h/day exercise (5.5 mA active, 2 μA sleep)
- **No regulator needed** — CR2032 directly powers all components

## Block Diagram

```
┌─────────────────────────────────────────────────┐
│              BODY SENSOR (nRF52840)              │
│                                                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐      │
│  │ LSM6DSO  │  │ LIS3MDL  │  │ CR2032   │      │
│  │ Accel    │  │ Mag      │  │ 220 mAh  │      │
│  │ Gyro     │  │ ±8 gauss │  │ 3V       │      │
│  │ ±8g      │  │ 100 Hz   │  │          │      │
│  │ ±2000dps │  │          │  │          │      │
│  │ 100 Hz   │  │          │  │          │      │
│  └────┬─────┘  └────┬─────┘  └──────────┘      │
│       │ SPI0        │ SPI0                      │
│  ┌────┴─────────────┴──────────────┐           │
│  │        nRF52840 QFAA            │           │
│  │  1MB flash, 256KB RAM          │           │
│  │  Cortex-M4F @ 64 MHz          │           │
│  │  BLE 5.0 + NFC-A              │           │
│  └────────────────────────────────┘           │
│       │ BLE 5.0 (2M PHY)                       │
│  ┌────┴──────────┐                            │
│  │ PCB Trace Ant │                            │
│  └───────────────┘                            │
│                                                 │
│  ┌──────────┐  ┌──────────┐                   │
│  │ SK6812   │  │ Button   │                   │
│  │ LED      │  │ Power    │                   │
│  └──────────┘  └──────────┘                   │
│                                                 │
│  Enclosure: 35mm × 25mm × 8mm                  │
│  Mount: Silicone hook-and-loop strap            │
└─────────────────────────────────────────────────┘
```
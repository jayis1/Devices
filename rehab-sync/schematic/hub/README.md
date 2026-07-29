# Rehab Hub Schematic

## Overview

The Rehab Hub is the central gateway of the RehabSync system. It coordinates the BLE body-area network (BAN), bridges to the cloud via Wi-Fi/MQTT, runs local edge ExerciseNet + FormNet + RepCount inference, drives the TFT display, I²S speaker, and haptic driver, and manages OTA firmware distribution.

## SoC: ESP32-S3-WROOM-1-N16R8

- 16 MB flash, 8 MB PSRAM
- Dual-core Xtensa LX7 @ 240 MHz
- Vector instructions for CNN acceleration
- Wi-Fi 2.4 GHz + BLE 5.0

## Pin Assignments

| GPIO | Function | Bus | Notes |
|------|----------|-----|-------|
| GPIO4 | SX1262 DIO1 | Radio IRQ | Interrupt |
| GPIO5 | SX1262 BUSY | Radio | Busy signal |
| GPIO6 | SX1262 NSS | SPI2 CS | Radio CS |
| GPIO7 | SX1262 RST | GPIO | Radio reset |
| GPIO8 | SX1262 SCK | SPI2 | Radio clock |
| GPIO9 | SX1262 MISO | SPI2 | Radio MISO |
| GPIO10 | SX1262 MOSI | SPI2 | Radio MOSI |
| GPIO11 | I²C SDA | I²C0 | BME280, DS3231, DRV2605L, MAX17048 |
| GPIO12 | I²C SCL | I²C0 | Shared I²C bus |
| GPIO13 | Hub IMU INT1 | GPIO | LSM6DSO interrupt |
| GPIO14 | Hub IMU CS | SPI3 CS | LSM6DSO |
| GPIO15 | Hub IMU SCK | SPI3 | LSM6DSO clock |
| GPIO16 | Hub IMU MISO | SPI3 | LSM6DSO MISO |
| GPIO17 | Hub IMU MOSI | SPI3 | LSM6DSO MOSI |
| GPIO18 | SD card CS | SPI3 CS | Logging |
| GPIO19 | SD card SCK | SPI3 | Shared SPI3 |
| GPIO20 | SD card MOSI | SPI3 | Shared SPI3 |
| GPIO21 | SD card MISO | SPI3 | Shared SPI3 |
| GPIO35 | TFT SCK | SPI4 | Display |
| GPIO36 | TFT MOSI | SPI4 | Display |
| GPIO37 | TFT CS | SPI4 CS | Display |
| GPIO38 | TFT DC | GPIO | Display data/command |
| GPIO39 | TFT RST | GPIO | Display reset |
| GPIO40 | TFT BL | PWM | Backlight |
| GPIO41 | I²S BCLK | I²S | Audio (MAX98357A) |
| GPIO42 | I²S LRCK | I²S | Audio |
| GPIO45 | I²S DIN | I²S | Audio data |
| GPIO46 | Status LED | SK6812 | BLE/WiFi/Cloud/Session |
| GPIO43 | USB TX | UART0 | Debug |
| GPIO44 | USB RX | UART0 | Debug |

## Power

- **Input:** USB-C 5V
- **Charger:** MCP73871 (LiPo charger, 500 mA)
- **Battery:** 2000 mAh LiPo (3.7V nominal)
- **Boost:** TPS61023 (3.7V → 5V, 2A)
- **LDO:** AMS1117-3.3 (5V → 3.3V)
- **Battery life:** ~8 hours continuous, ~72 hours standby

## Block Diagram

```
┌──────────────────────────────────────────────────────────┐
│                    REHAB HUB (ESP32-S3)                   │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │
│  │ SX1262   │  │ LSM6DSO  │  │ microSD  │  │ ILI9488  │ │
│  │ Sub-GHz  │  │ Hub IMU  │  │ Logger   │  │ 3.5" TFT │ │
│  │ SPI2     │  │ SPI3     │  │ SPI3     │  │ SPI4     │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘ │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ I²C0: BME280 + DS3231 + DRV2605L + MAX17048     │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ MAX98357 │  │ OV5640   │  │ USB-C    │               │
│  │ I²S Amp  │  │ Camera   │  │ Power+Dbg│               │
│  └──────────┘  └──────────┘  └──────────┘               │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ Wi-Fi 2.4 GHz + BLE 5.0 (BAN Coordinator)       │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ Power: MCP73871 + TPS61023 + AMS1117-3.3        │    │
│  │ Battery: 2000 mAh LiPo                           │    │
│  └──────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────┘
```
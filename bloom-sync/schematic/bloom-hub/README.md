# Bloom Hub Schematic

## Overview

The Bloom Hub is the central gateway of the BloomSync postpartum maternal health system. It coordinates the BLE wide-area network (WAN), bridges to the cloud via Wi-Fi/MQTT, runs local edge inference (hemorrhage risk screening, wound infection screening, mastitis detection), captures voice samples for PPD screening, drives the TFT display + speaker + haptic, and manages OTA firmware distribution.

## SoC: ESP32-S3-WROOM-1-N16R8

- 16 MB flash, 8 MB PSRAM
- Dual-core Xtensa LX7 @ 240 MHz
- Vector instructions for CNN acceleration
- Wi-Fi 2.4 GHz + BLE 5.0

## Pin Assignments

| GPIO | Function | Bus | Notes |
|------|----------|-----|-------|
| GPIO8 | I²C SDA | I²C0 | BME280, DS3231, DRV2605L, MAX17048 |
| GPIO9 | I²C SCL | I²C0 | Shared I²C bus |
| GPIO10 | SD card CS | SPI2 CS | Logging |
| GPIO11 | SD card MOSI | SPI2 | Shared SPI2 |
| GPIO12 | SD card SCK | SPI2 | Shared SPI2 |
| GPIO13 | SD card MISO | SPI2 | Shared SPI2 |
| GPIO36 | TFT SCK | SPI3 | Display |
| GPIO37 | TFT MOSI | SPI3 | Display |
| GPIO38 | TFT CS | SPI3 CS | Display |
| GPIO39 | TFT DC | GPIO | Display data/command |
| GPIO40 | TFT RST | GPIO | Display reset |
| GPIO41 | TFT BL | PWM | Backlight |
| GPIO42 | I²S BCLK | I²S | Audio (mic + amp) |
| GPIO43 | I²S LRCK | I²S | Audio |
| GPIO44 | I²S DIN | I²S | Mic data in (ICS-43434) |
| GPIO45 | I²S DOUT | I²S | Amp data out (MAX98357A) |
| GPIO46 | Status LED | SK6812 | BLE/WiFi/Cloud/Alert |
| GPIO47 | Haptic EN | GPIO | DRV2605L enable |
| GPIO0 | Button | GPIO | User button |
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
│                    BLOOM HUB (ESP32-S3)                   │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │
│  │ microSD  │  │ ILI9488  │  │ ICS-43434│  │ MAX98357 │ │
│  │ Logger   │  │ 3.5" TFT │  │ I²S Mic  │  │ I²S Amp  │ │
│  │ SPI2     │  │ SPI3     │  │ Voice    │  │ Speaker  │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘ │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ I²C0: BME280 + DS3231 + DRV2605L + MAX17048     │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ DRV2605L │  │ USB-C    │  │ SK6812   │               │
│  │ Haptic   │  │ Power+Dbg│  │ RGB LEDs │               │
│  └──────────┘  └──────────┘  └──────────┘               │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ Wi-Fi 2.4 GHz + BLE 5.0 (WAN Coordinator)       │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ Power: MCP73871 + TPS61023 + AMS1117-3.3        │    │
│  │ Battery: 2000 mAh LiPo                           │    │
│  └──────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────┘
```
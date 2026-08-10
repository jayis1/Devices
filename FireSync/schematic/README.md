# FireSync — Schematics

## Overview

FireSync has 5 hardware node types, each with its own schematic:

| Node | MCU | Key ICs | File |
|------|-----|---------|------|
| FireSync Hub | ESP32-S3-WROOM-1-N16R8 | SX1262, SIM7000A, BME280, DS3231 | `hub/` |
| Room Sentinel | ESP32-S3-WROOM-1-N8R2 | SX1262, PMSA003, ZE07-CO, MLX90640, DS18B20 | `room-sentinel/` |
| Stove Guard | ESP32-S3-WROOM-1-N8R2 | SX1262, MLX90640, AS5600×4, L298N | `stove-guard/` |
| Panel Monitor | STM32G431CBU6 | SX1262, SCT-013×2, DS18B20×4 | `panel-monitor/` |
| Escape Controller | ESP32-S3-WROOM-1-N8R2 | SX1262, W25Q128, MAX98357A, WS2812B×4 | `escape-controller/` |

Open `.kicad_pro` files in KiCad 7+.
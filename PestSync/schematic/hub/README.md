PestSync Hub — KiCad Project
==============================

This directory contains the KiCad project files for the PestSync Hub node.

Files:
- hub.kicad_pro    — KiCad project file
- hub.kicad_sch    — Schematic (ESP32-WROOM-32E, SX1262, SSD1306, BME280, microSD, power)
- hub.kicad_pcb    — PCB layout (2-layer FR4)

Schematic Overview:
- ESP32-WROOM-32E MCU
- SX1262 Sub-GHz radio (SPI: MOSI=14, MISO=12, SCK=13, NSS=15, RST=2, DIO1=4, BUSY=5)
- SSD1306 OLED 0.96" (I2C: SDA=21, SCL=22)
- BME280 ambient sensor (I2C shared)
- microSD card slot (SPI2: MISO=19, MOSI=23, SCK=18, CS=25)
- WS2812B status LED (GPIO26)
- TP4056 + DW01 battery protection + 18650 holder
- USB-C power input + AP2112 3.3V LDO
- 5V 1W solar panel input

See README.md in repo root for full pin assignments.
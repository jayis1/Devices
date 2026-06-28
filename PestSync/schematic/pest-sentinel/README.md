PestSync Pest Sentinel — KiCad Project
======================================

This directory contains the KiCad project files for the Pest Sentinel node.

Files:
- pest-sentinel.kicad_pro    — KiCad project file
- pest-sentinel.kicad_sch    — Schematic (ESP32-S3, OV2640, MLX90640, PIR, SX1262, IR LED)
- pest-sentinel.kicad_pcb    — PCB layout (4-layer FR4, impedance-controlled camera bus)

Schematic Overview:
- ESP32-S3-N8R2 MCU (8 MB PSRAM for YOLOv8-nano model + frame buffers)
- OV2640 camera (DVP parallel bus, 20 MHz XCLK, IR-cut filter)
- MLX90640 thermal array (32×24 IR, I2C #2 at 400 kHz)
- AM312 PIR motion sensor (GPIO interrupt)
- 2× 850 nm IR LEDs (MOSFET-driven, night illumination)
- SX1262 Sub-GHz radio (SPI)
- WS2812B status LED
- TP4056 + DW01 + 18650 holder
- USB-C power + programming

See README.md in repo root for full pin assignments.
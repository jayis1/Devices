PestSync Smart Trap — KiCad Project
====================================

Files:
- smart-trap.kicad_pro    — KiCad project
- smart-trap.kicad_sch    — Schematic (ESP32-C3, SX1262, HX711, reed, ADXL362, bait sensor)
- smart-trap.kicad_pcb    — PCB layout (2-layer FR4)

Schematic Overview:
- ESP32-C3 MCU (RISC-V, ultra-low power)
- SX1262 Sub-GHz radio (SPI: MOSI=2, MISO=3, SCK=4, NSS=5, RST=6, DIO1=7, BUSY=8)
- HX711 load cell amplifier (DOUT=GPIO1, SCK=GPIO10) + 50g load cell
- Reed switch (GPIO0, interrupt on trap fire)
- ADXL362 accelerometer (SPI: CS=21, INT=20) for tamper detection
- Capacitive bait level sensor (ADC1_CH0, GPIO9)
- Bicolor LED (GPIO18: green=armed, red=triggered)
- TPS61099 boost converter (2× AA → 3.3V)
- USB-C programming connector

See README.md in repo root for full pin assignments.
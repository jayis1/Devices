PestSync Deterrent Node — KiCad Project
========================================

Files:
- deterrent-node.kicad_pro    — KiCad project
- deterrent-node.kicad_sch    — Schematic (ESP32-C3, SX1262, ultrasonic, strobe, diffuser)
- deterrent-node.kicad_pcb    — PCB layout (2-layer FR4)

Schematic Overview:
- ESP32-C3 MCU
- SX1262 Sub-GHz radio (SPI: MOSI=2, MISO=3, SCK=4, NSS=5, RST=6, DIO1=7, BUSY=8)
- 2× 40 kHz piezo ultrasonic transducers (GPIO9, LEDC PWM, MOSFET driven)
- High-intensity white strobe LED (GPIO10, MOSFET driven)
- Piezo essential-oil diffuser (GPIO1, MOSFET driven) + reservoir
- Capacitive oil level sensor (ADC1_CH0, GPIO0)
- WS2812B status LED (GPIO18)
- TP4056 + DW01 + 18650 + USB-C power

See README.md in repo root for full pin assignments.
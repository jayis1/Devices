# ambient-controller conceptual schematic connection notes

Author: jayis1

STM32G0B1 PA8 LED PWM, PB0 USB fan MOSFET enable; both outputs are SELV only. SX1262 SPI1 PA5 SCK/PA6 MISO/PA7 MOSI/PA4 NSS/PB5 DIO1. USB-C 5 V -> AP2112K 3.3 V.

Power: input fuse/polyfuse, reverse-polarity protection, 100 nF decoupling at each IC, 10 uF regulator bulk capacitance, test points, and RF antenna keep-out. This map needs KiCad ERC, thermal, and RF review before fabrication.

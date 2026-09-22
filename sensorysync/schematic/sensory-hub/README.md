# sensory-hub conceptual schematic connection notes

Author: jayis1

ESP32-S3 to SX1262: GPIO11 MOSI, GPIO13 MISO, GPIO12 SCK, GPIO10 NSS, GPIO4 DIO1, GPIO5 BUSY, GPIO6 RESET. CM4 links by UART at 3.3 V.

Power: input fuse/polyfuse, reverse-polarity protection, 100 nF decoupling at each IC, 10 uF regulator bulk capacitance, test points, and RF antenna keep-out. This map needs KiCad ERC, thermal, and RF review before fabrication.

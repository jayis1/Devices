# quiet-pod conceptual schematic connection notes

Author: jayis1

RP2040 GPIO10 BCLK/GPIO11 LRCLK/GPIO12 DIN: MAX98357A. GPIO15 haptic PWM, GPIO2 local stop button. SX1262 SPI0 GPIO19 MOSI/GPIO16 MISO/GPIO18 SCK/GPIO17 NSS/GPIO20 DIO1.

Power: input fuse/polyfuse, reverse-polarity protection, 100 nF decoupling at each IC, 10 uF regulator bulk capacitance, test points, and RF antenna keep-out. This map needs KiCad ERC, thermal, and RF review before fabrication.

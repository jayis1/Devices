# comfort-band conceptual schematic connection notes

Author: jayis1

nRF52840 P0.26 SDA/P0.27 SCL: BMI270. P0.02: protected EDA ADC. P0.13: wearer button. P0.14: haptic enable. CR2477 -> TPS62743 3.0 V.

Power: input fuse/polyfuse, reverse-polarity protection, 100 nF decoupling at each IC, 10 uF regulator bulk capacitance, test points, and RF antenna keep-out. This map needs KiCad ERC, thermal, and RF review before fabrication.

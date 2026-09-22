# room-beacon conceptual schematic connection notes

Author: jayis1

ESP32-C6 I2C GPIO6 SDA/GPIO7 SCL: SCD41, VEML7700, SHTC3. I2S GPIO2 BCLK/GPIO3 WS/GPIO4 DOUT: INMP441. SX1262 GPIO10 NSS/GPIO11 MOSI/GPIO12 MISO/GPIO13 SCK/GPIO5 DIO1.

Power: input fuse/polyfuse, reverse-polarity protection, 100 nF decoupling at each IC, 10 uF regulator bulk capacitance, test points, and RF antenna keep-out. This map needs KiCad ERC, thermal, and RF review before fabrication.

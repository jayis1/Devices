# Preliminary schematic notes (not KiCad/ERC-checked)

GL-2: J1 protected 3V3/GND; U1 ESP32-C3 module; U2 BH1750FVI breakout (I2C 0x23); R1/R2 4.7k pull-ups SDA/SCL; C1 100nF; D1 status LED via 1k to GPIO8; J2 USB-UART/program pads (GND,3V3,TX,RX,EN,BOOT). GPIO4 SDA, GPIO5 SCL.

GL-3 substitutes VEML7700 (I2C 0x10) and same host. Check module GPIO/strap behavior, battery protection/regulator, connector polarity, and sensor optical orientation against selected manufacturer documentation before PCB design.

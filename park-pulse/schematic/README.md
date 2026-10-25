# Schematic status

No KiCad files are represented as completed schematics. The node pin assignments below are conceptual and require datasheet review and KiCad capture before fabrication.

Fountain Sentinel: ESP32-C6-MINI-1 3V3; GPIO4 I2C SDA (VL53L1X optional), GPIO5 SCL, GPIO6 pulse input (open-drain flow sensor, external 10k pull-up), GPIO7 leak switch (active-low, 10k pull-up), GPIO8 SX1262 NSS, GPIO9 SCK, GPIO10 MOSI, GPIO11 MISO, GPIO12 RESET, GPIO13 BUSY.

Bin Sentinel: same radio wiring; GPIO4/5 I2C to VL53L1X (address 0x29); GPIO6 HX711 DOUT and GPIO7 SCK for load cell. Check C6 module pin mapping and strapping constraints against chosen module datasheet.

Gateway: Pi Zero 2 W USB to certified-region SX1262 USB concentrator; no mains wiring.

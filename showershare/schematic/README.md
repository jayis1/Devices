# Prototype schematic notes

These pin-level connection notes are not KiCad schematic files and have not undergone ERC. Stall Sentinel connections: ESP32-C3 module GPIO4 to door reed with 10 kΩ pull-up; GPIO5/GPIO6 to HX711 DOUT/SCK; common ground; regulated 3.3 V logic. Water Sentinel: GPIO4 to DS18B20 data with 4.7 kΩ pull-up; GPIO5 pulse input with 10 kΩ pull-up. Gateway: ESP32-C6 powered by regulated 5 V USB module. Verify module pinouts and logic levels against selected manufacturer datasheets before wiring.

# Schematic notes (not KiCad)

## Shelf node
Candidate ESP32-C6-WROOM-1 module; HX711 breakout DOUT=GPIO4, SCK=GPIO5; tare switch GPIO9 to ground with pull-up; LED GPIO8 through series resistor. USB 5V power; 3.3V domain. Verify selected HX711 board IO/output voltage and ESP32-C6 module pin availability before connection. This note is not a schematic or fabrication-ready design.

## Intake node
ESP32-C6-WROOM-1; PN532 I²C SDA GPIO4/SCL GPIO5; buttons GPIO8/9 with pull-ups; LED GPIO10 through resistor. Use only a PN532 breakout with documented 3.3V logic or add level shifting. This note is not a schematic or fabrication-ready design.

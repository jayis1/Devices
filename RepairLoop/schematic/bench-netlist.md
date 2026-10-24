# Conceptual bench sensor netlist (not KiCad)

- USB 5V input → protected 5V rail; regulator selected for ESP32-C6 module peak current.
- ESP32-C6-WROOM-1-N8 module; UART programming/debug pads; local decoupling per module datasheet.
- Occupancy sensor output to GPIO4; I2C peripheral SDA GPIO6/SCL GPIO7, pullups to compatible logic voltage.
- Status LED GPIO8 through current-limiting resistor.
- Confirm board/module pin availability and antenna keepout before layout.

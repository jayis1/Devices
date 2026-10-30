# Pin and schematic intent

Documentation only; no KiCad schematic or fabrication files are supplied.

- Gateway: ESP32-C6 module; USB-C 5 V input to module supply; I2C OLED GPIO6 SDA / GPIO7 SCL; status LED GPIO15. Do not route USB VBUS directly to 3.3 V.
- Tool Dock: XIAO nRF52840 module; D0-D5 GPIO input pull-ups to normally-open switches returning to ground; D6 service input; D7 battery monitor through divider.
- Pump Monitor: XIAO nRF52840; LIS3DH on I2C D4/D5; reed input D1; pressure sensor analog output must be scaled within ADC limits and have defined ground/reference. Confirm transducer pressure rating and output range before connection.

Board-specific pin aliases, power paths, protection, antenna keep-out, decoupling, and connector protection require design review against the exact module revision before layout.

# LineWise connection records

These are design records, not KiCad schematic files or ERC-checked circuits. Before PCB work, verify exact dev-board pin mapping and datasheet limits.

- gateway.md: ESP32-C6 module, USB-C 5 V input → protected 3.3 V rail; I2C header GPIO4 SDA/GPIO5 SCL, SWD/UART programming access and antenna keepout.
- line-sentinel.md: nRF52840 module, 2×AA protected supply → 3.3 V; HX711 DOUT P0.26, SCK P0.27, load cell E+/E-/A+/A-. No mains connection.
- weather-sentinel.md: ESP32-C3 module, regulated 5 V input; GPIO2 rain reed and GPIO3 wind reed to ground, 10 kΩ pull-ups to 3.3 V; external low-capacitance ESD protection at cable entry.

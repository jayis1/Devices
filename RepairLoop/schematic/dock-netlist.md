# Conceptual dock netlist (not KiCad)

- Protected USB 5V input; regulated 3.3V rail and module decoupling per selected board datasheet.
- nRF52840 module, SWD programming pads.
- HX711 DOUT P0.11, SCK P0.12; load cell excitation and input range must be verified.
- I2C SDA P0.26/SCL P0.27 for optional dock peripherals; verify NFC-pin configuration and board routing.

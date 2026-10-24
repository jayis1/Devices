# Conceptual air sentinel netlist (not KiCad)

- Protected USB 5V input and 3.3V regulator sized for selected radio module.
- ESP32-C6-WROOM-1-N8; programming/debug UART.
- I2C SDA GPIO6/SCL GPIO7 to selected environmental module; GPIO4 reads fan tach only, no fan control.
- GPIO8 status LED through resistor; sensor interface voltage must be verified.

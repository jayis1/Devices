# Conceptual netlist — Site Hub

Not a KiCad schematic or fabrication document. Module-level prototype wiring only.

| Ref | Part | Connections |
|---|---|---|
| U1 | ESP32-S3-WROOM-1 | 3V3, GND; UART0 TX GPIO43/RX GPIO44 for debug; I2C SDA GPIO8/SCL GPIO9 reserved; SPI display TBD |
| J1 | USB-C receptacle | VBUS to certified 5 V input; CC1/CC2 each 5.1 kΩ Rd; GND; ESD protection required |
| D1 | Status LED | GPIO2 -> series resistor -> LED -> GND |
| RF1 | Module antenna | Keep-out per module datasheet; do not add external antenna unless exact module variant permits |

Pin map is an initial proposal; verify against exact module revision and SDK before PCB capture. 5 V must not reach MCU GPIO.

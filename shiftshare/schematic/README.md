# Prototype schematic intent

No native KiCad schematic files are supplied. This document is a pin/net planning aid only and has not passed ERC/DRC. Board-level pin aliases must be checked against the exact purchased module revision before wiring.

- gateway: Pico W, I2C GP4/GP5 to display; button inputs GP10-GP12; LED GP13 through current-limiting resistor.
- room sentinel: nRF52840 XIAO, analog envelope output to ADC-capable pin; BLE data link.
- supply dock: nRF52840 XIAO, NAU7802 I2C, load-cell bridge to NAU7802, reed GPIO.

Power is SELV only. No external mains, actuators, or charging circuit design is specified.

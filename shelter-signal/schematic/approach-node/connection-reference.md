# Approach node connection reference (not a KiCad schematic)

| Ref | Part | Connections |
|---|---|---|
| U1 | MDBT50Q nRF52840 module | VDD/GND; clear/block inputs P0.11/P0.12; tilt P0.13; LED P0.15 |
| SW1/SW2 | momentary buttons | pull-ups to 3V3; contacts to GND; debounce in firmware |
| S1 | tilt switch | pull-up to 3V3; switch to GND |
| D1 | LED | series resistor to P0.15 |
| BT1/U2 | 2xAA and regulator | validate cell range and cold performance |

Not a schematic, netlist, or verified module-carrier design.

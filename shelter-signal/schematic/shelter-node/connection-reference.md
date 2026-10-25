# Shelter node connection reference (not a KiCad schematic)

| Ref | Part | Connections |
|---|---|---|
| U1 | MDBT50Q nRF52840 module | VDD/GND; I2C SDA P0.26, SCL P0.27; light ADC P0.04; contact P0.11 |
| U2 | BME280 | I2C address 0x76 (SDO low), 3V3/GND; local decoupling |
| Q1 | PT15-21C/TR8 | divider to P0.04; select resistor after calibration |
| SW1 | reed switch | pull-up to 3V3, contact to GND |
| BT1/U3 | 2xAA and regulator | validate input voltage range against fresh-cell voltage; add reverse protection |

Verify exact module/carrier pin mapping before any board design.

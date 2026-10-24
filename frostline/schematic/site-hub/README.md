# Site hub connections (not a KiCad schematic)

Netlist reference: Pico W GP6=SCK, GP7=MOSI, GP4=MISO; GP5=SX1262_NSS, GP8=BUSY, GP9=DIO1, GP10=RESET; all logic 3.3 V. GP12/13=I²C SDA/SCL to DS3231 at 0x68 with pullups. Provide USB-C power, reset access, SWD/UART service pads, decoupling, and RF keepout/antenna per module datasheet. Use local certified 5 V adapter; no battery charger included in current BOM. Diagram must be captured and ERC-checked in KiCad before PCB fabrication.

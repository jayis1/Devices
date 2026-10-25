# Schematic capture status

No KiCad schematic files are supplied. The following connectivity outline is not a schematic or fabrication deliverable; power protection, connector pinout, ESD, decoupling and PCB layout must be designed and ERC/DRC checked by a qualified engineer.

Curb node: XIAO nRF52840 3V3/GND; LIS3DH SDA/SCL on board D4/D5; HX711 DOUT/SCK on D2/D3; wetness divider output to A0; load cell to HX711 bridge inputs. Confirm pin mapping for the purchased board revision.
Button and crew beacon: XIAO GPIO input to momentary switch with pull-up; LED/sounder drivers require current-limiting/transistor stage and are not finalized. Hub: Pi USB BLE adapter and SPI display module; verify display voltage and connector before assembly.

# Reference pin/net list (not a KiCad schematic)

MCU: Ebyte E73-2G4M08S1C nRF52840 module, 3.3 V. LIS3DH VDD/VDDIO=3.3 V, I2C address 0x18 (SA0 low), SDA=P0.26, SCL=P0.27, pull-ups 4.7k to 3.3 V. INT1=P0.11. Reed input P0.13 to GND, internal pull-up; debounce 30 ms. SWDIO/SWDCLK exposed on test pads; RESET exposed. Battery divider to ADC P0.04 (100k/100k, enable only for measurement). Radio antenna keepout per module datasheet. Add decoupling and protected battery/charger per power doc.

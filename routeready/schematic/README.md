Reference pin/net ledgers only; not KiCad projects or fabrication-ready schematics.

Section beacon: 5V_IN -> module VIN; module 3V3 -> switch pullups and LED logic; GND common. Switch common to GND; open/setting/closed contacts to GPIO4/GPIO5 via defined truth table and internal pullups. GPIO6 -> 1k series -> LED anode; LED cathode -> GND. Verify current limiting with selected LED.
Setter dock: PN532 powered at 3V3; I2C SDA GPIO4/SCL GPIO5 with 4.7k pullups to 3V3; confirm button GPIO6 to GND with pullup. Check PN532 module voltage variant before wiring.
Gateway Pico W powered by certified USB 5V; do not connect unisolated venue mains.

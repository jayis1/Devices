# Reference net plans (not KiCad schematics)

Gateway: Pico W; 5V USB to board input; GP0/GP1 UART0 reserved; GP4/GP5 I2C0 optional; onboard Wi-Fi. Confirm Pico W board pin map and supply design before fabrication.

Bag node: ESP32-C3-MINI-1 module; battery input must use a regulator compatible with cell voltage and radio peaks; I2C GPIO4/5 to VL53L0X (3V3); pullups 4.7k to 3V3; sensor shutdown controlled by a verified free GPIO. No charging circuit specified.

Bin node: same sensor bus; reed switch from GPIO6 to ground, internal/external pull-up to 3V3. Enclosure and sensor placement must avoid contact with waste.

Beacon: nRF52810 module button P0.11 to ground and LED P0.13 through current-limiting resistor; confirm module pin mapping and CR2032 pulse capability.

These are conceptual connections; verify against selected module schematics and datasheets. No ERC, PCB, or electrical review has been performed.

# Tactile Hub schematic reference

- J1 USB-C receptacle -> 5 V input, CC1/CC2 each 5.1 kΩ to GND.
- F1 2.5 A polyfuse; TVS1 SMBJ5.0A to GND.
- U1 Raspberry Pi CM4 on carrier; U2 ESP32-C6-WROOM-1 powered by 3V3.
- U3 TPS62130: 5 V to 3.3 V, 2 A; 22 µH inductor, 2x22 µF output.
- U2 GPIO16/17 cross to CM4 UART RX/TX through 0 Ω links.
- J2 exposes I²C and 3V3 for a DW3000 UWB gateway module.
- SW1 is a physical commissioning button to GND on CM4 GPIO23.

Keep antenna clear zones and USB-C ESD layout per vendor reference designs.

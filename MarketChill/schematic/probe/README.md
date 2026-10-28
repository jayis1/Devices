# Schematic design note — Probe
Not a KiCad schematic or fabrication-ready design. Interfaces: ESP32-C3-MINI-1 GPIO6/7 I²C to TMP117AIDRVR at 0x48; 4.7 kΩ pull-ups to 3V3; 100 nF sensor decoupling. Add a regulated 3.3 V supply, battery reverse protection, programming connector, and antenna keepout in a reviewed schematic. Validate exact module pinout and rail range before PCB layout.

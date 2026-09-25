# Door Navigator schematic reference

- J1 USB-C 5 V input, 1 A fuse and ESD diode.
- U1 ESP32-C6-WROOM-1 powered by AP63203 3.3 V buck.
- U2 DRV2605L: SDA=GPIO6, SCL=GPIO7, EN=GPIO5; J2 to 10 mm LRA.
- J3 reed switch to GPIO4 with 10 kΩ pull-up and 100 nF debounce capacitor.
- SW1 commissioning switch to GPIO9; status LED GPIO8.

This node senses only the doorway's local reed contact. It is not an access-control or emergency egress device.

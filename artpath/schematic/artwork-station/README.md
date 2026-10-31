# Artwork station proposed net schedule (not a KiCad schematic)

- ESP32-C3-MINI-1 module: 3V3, GND, EN, BOOT and UART debug; antenna keepout per module datasheet.
- PN532 module: I2C SDA/SCL to GPIO4/GPIO5; power and logic levels must be confirmed for the exact breakout; NFC antenna placement is mechanical-critical.
- MAX98357A: I2S BCLK/LRCLK/DIN from GPIO6/7/8; 3V3 logic, speaker output to SP1 only; do not ground either speaker output.
- Button SW1: GPIO2 to GND, pull-up enabled, debounce in software.
- USB-C 5V input: ESD protection, resettable fuse, reverse protection; regulator and bulk capacitance sized for radio/audio peak load.

This is a functional connection proposal only. No native KiCad schematic/PCB, footprint review, ERC, assembly drawing, enclosure or compliance assessment exists. Verify module pinout and datasheets before implementation.

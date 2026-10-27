# Schematic status

These are textual wiring specifications, not KiCad schematics. No native `.kicad_sch` is supplied; electrical design must be reviewed and captured in KiCad before PCB manufacture. Never treat these notes as validated schematics.

## Hub wiring

ESP32-C6 module U1: 3V3, GND; OLED DS1 VCC=3V3/GND, SDA=GPIO7, SCL=GPIO6, address 0x3C; buttons SW1-SW3 GPIO10/11/18 to GND with internal pull-ups (verify module pin availability). USB-C J1 VBUS=5V, GND=GND; protected 3.3V regulator input 5V/output module 3V3. Add 100nF at module/OLED and bulk capacitor sized after transient measurements. Provide reset/boot and programming access. Do not use USB-C CC pins without 5.1k Rd resistors on CC1/CC2.

## Chute wiring

ESP32-C3 module U2: sensor output to GPIO4, 10k pull-up to 3V3; sensor GND common; sensor VCC only per vendor rating. Verify open-collector output before connecting; do not connect a 5V push-pull output to GPIO. USB-C J2 VBUS feeds sensor and 3.3V regulator; common ground. Add input RC/filter after measuring sensor pulse shape, ESD protection at exposed sensor connector, decoupling, reset and programming pads.

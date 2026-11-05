# Prototype schematic ledger

Connection-level notes only, not native KiCad files or ERC-checked schematics. Do not fabricate PCBs without reviewing exact module reference designs.

Room beacon: protected cell/USB source → 3.3 V regulator → ESP32-C3-MINI-1. GPIO8/9 I²C for optional SHTC3, GPIO3 LED via 1 kΩ, GPIO4 button to GND, UART0 GPIO21/20 programming. Add decoupling per module datasheet and preserve antenna keep-out. Gateway: Raspberry Pi Zero 2 W, certified regulated 5 V/2 A USB supply. Scan dock: ESP32-C3; QR engine UART, OLED I²C, buttons to ground. Confirm QR-engine UART voltage before connecting.
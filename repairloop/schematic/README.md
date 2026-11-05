# Reference wiring ledgers (not native KiCad schematics)

This directory documents connections for prototype planning only. Netlists, symbols, footprints, PCB layouts, and ERC results are absent. Convert to reviewed KiCad projects only after confirming the selected board/module reference designs.

- bench-beacon: ESP32-C3 GPIO4/5 buttons to GND, GPIO6 LED data via 330 Ω, UART0 GPIO20/21 service; 3V3/GND.
- ticket-dock: ESP32-C3 GPIO4-6 buttons, GPIO8/9 I²C to 3.3 V OLED with pull-ups, UART0 GPIO20/21.
- gateway: Pi Zero 2 W powered by 5 V USB; local network interface to beacon coordinator; storage on microSD.

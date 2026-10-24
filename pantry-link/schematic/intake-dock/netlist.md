# Conceptual netlist — Intake Dock

Not a KiCad schematic; module-level prototype wiring only.

| Signal | ESP32-C3-MINI-1 GPIO proposal | Connection |
|---|---|---|
| HX711 DOUT/SCK | GPIO4 / GPIO5 | 3V3 interface; bridge load cell |
| LCD SPI SCK/MOSI/CS/DC | GPIO6 / GPIO7 / GPIO10 / GPIO3 | Confirm board/module pin availability; avoid strapping conflicts |
| Volunteer confirm button | GPIO1 | Pull-up, debounced; physical confirmation required |
| USB-C VBUS | 5 V input | Certified external SELV supply, fuse and ESD protection |
| 3V3 rail | Module supply | Regulator sized for peak radio/display load |

Wi-Fi remains disabled in normal operation; BLE to the hub is the intended link. Exact pin and power design awaits board-level review.

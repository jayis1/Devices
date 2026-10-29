# Schematic status: pin/net specification only; not KiCad, not ERC-checked.

ESP32-C3-MINI-1 regulated 3.3 V, USB 5 V input. Reed contacts GPIO3/GPIO4 to GND with pull-ups; buttons GPIO5/GPIO6 to GND with pull-ups; GPIO7 -> 1 kΩ -> LED -> GND. Debounce in firmware. Add ESD protection for exposed contacts, connector labeling, bypass capacitors and programming access. Confirm pins against exact module/board before fabrication.

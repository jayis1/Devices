# Candidate electrical net schedules, not KiCad captures

- Rack: USB5V→3V3 carrier; load-cell bridge excitation from module-rated supply; differential A+/A−→HX711; HX711 SCK→GPIO6, DOUT→GPIO7; common ground; UART debug. Keep analog/load-cell wiring twisted and away from radio antenna.
- Dock: USB5V→3V3 carrier; VL53L0X SDA/SCL→GPIO4/5; XSHUT→GPIO6 if supported; module interrupt optional; I²C pull-ups to 3V3 only. Confirm the breakout's onboard regulator and level shifting.
- Inspection beacon: USB5V→3V3 carrier; normally-open momentary switch GPIO2→GND with pull-up; GPIO8→resistor→RGB LED driver; common ground; UART debug.

No KiCad-native files, symbols, footprints, or PCB are included. Nets are provisional; verify carrier/module revisions and electrical ratings before implementation.
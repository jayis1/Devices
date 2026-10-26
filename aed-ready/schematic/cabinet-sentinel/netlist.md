# Cabinet sentinel conceptual netlist — not KiCad / not ERC-checked

- ESP32-C3-MINI-1 module, 3V3/GND, USB/UART programming access.
- TMP117 V+ to 3V3, GND to GND, SDA=GPIO4, SCL=GPIO5; 4.7 kΩ pull-ups to 3V3; address 0x48.
- Normally-closed reed contact between GPIO6 and GND; pull-up to 3V3, debounce in firmware.
- Test button GPIO7 to GND, pull-up; LED GPIO3 through 1 kΩ to GND.
- Battery ADC GPIO0 through a high-impedance divider; divider values and ADC calibration TBD.
- 2×AA holder -> reverse-polarity protection -> low-IQ 3.3 V regulator -> MCU/sensor.

Validate module pin availability, ADC attenuation, RF layout/antenna keep-out, regulator startup/peak current, ESD and enclosure before fabrication. This low-voltage add-on must never connect to AED internals.
# Conceptual netlist — Fridge Sentinel

Not a KiCad schematic; no electrical rule check or PCB capture performed.

| Signal | SoC pin proposal | Connection |
|---|---|---|
| DS18B20 DQ | P0.11 | 1-Wire bus, 4.7 kΩ pull-up to 3V3 |
| Door reed | P0.12 | Switch to GND, internal pull-up; debounce in firmware |
| Buzzer | P0.13 through NPN + base resistor | Never drive buzzer current directly from GPIO |
| Battery | ADC P0.04 divider | Verify ADC limits and divider leakage |
| Supply | 2×AA -> regulator -> 3V3 | No charging circuit |

Use a sealed, food-safe probe jacket and route cable through an approved pass-through without compromising the refrigerator seal. Probe calibration and placement require periodic review.

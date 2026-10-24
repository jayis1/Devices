# Conceptual netlist — Shelf Scale

Not a KiCad schematic. Verify exact module pinout and load-cell wiring before assembly.

| Signal | SoC pin proposal | Connection |
|---|---|---|
| HX711 DOUT | P0.11 | Input with no external 5 V pull-up |
| HX711 SCK | P0.12 | GPIO output, clock pulses bounded |
| BLE status LED | P0.13 | LED+resistor to ground |
| Load cell E+/E-/A+/A- | HX711 screw terminal | Bridge wiring per load-cell datasheet |
| Battery | ADC P0.04 via resistor divider | Protect ADC; divider values and sleep leakage TBD |
| Supply | 2×AA -> regulator -> 3V3 | Confirm regulator dropout and radio peak margin |

Use HX711 at 3.3 V; bridge excitation and ADC common-mode limits must be verified. Mechanical load bypass and overload stops are required.

# Schematic status: pin/net specification only; not KiCad, not ERC-checked.

ESP32-C3-MINI-1 powered from regulated 3.3 V; SPS30 powered from 5 V USB rail; UART sensor TX -> GPIO20 RX, sensor RX <- GPIO21 TX (verify module logic levels); common GND. GPIO8 -> 1 kΩ -> LED -> GND. Add sensor connector keyed, test pads for UART, local bypass capacitors, ESD protection at external connector, and antenna keepout. Final electrical design and pin compatibility must be reviewed against selected module revision and datasheets.

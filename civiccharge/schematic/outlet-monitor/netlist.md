# Conceptual monitor interface netlist — not a mains wiring diagram

- U1: Raytac MDBT50Q-1M nRF52840 module, 3V3/GND, SWDIO/SWDCLK exposed.
- U2: TMP117 temperature sensor, I2C address 0x48; SDA P0.26, SCL P0.27; local 3V3 pull-ups.
- J1: isolated AC measurement module output only (selection/open approval gate), 0–3.0 V SELV output -> P0.02/AIN0. Do not attach mains to this drawing.
- J2: isolated current-sensor output (selection/open approval gate), 0–3.0 V SELV output -> P0.03/AIN1.
- SW1: service request to P0.11, active-low.
- SW2: enclosure switch to P0.15, active-low.
- D1 + R1: status LED to P0.13.
- J3: SWD header; J4: certified isolated 5V input -> separately engineered 3V3 regulator.

Unresolved: exact metering/current front-end part, analog transfer function, reference/bias, protection network, regulator, EMC, isolation ratings and enclosure. Not fabrication-ready.
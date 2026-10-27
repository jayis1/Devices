# Rack wiring specification (not KiCad)

nRF52840 module U1: 3V3/GND; SW1 and SW2 connect P0.11/P0.12 to GND with internal pull-ups (confirm pins on exact chosen module). D1 via 1k resistor to available GPIO and GND. BT1 two AA cells -> reverse-polarity protection -> low-Iq 3.3V regulator -> U1; add 100nF local decoupling and bulk per module datasheet. SWDIO, SWCLK, RESET and GND pads required. A 2×AA alkaline pack is not rechargeable; do not connect a charger. Provide battery-low measurement through a resistor divider only after pin/budget review. Exact module, regulator, protection, and resistor MPNs are unresolved; no PCB release until selected and reviewed.

This text is a wiring note, not a KiCad schematic or validated circuit.

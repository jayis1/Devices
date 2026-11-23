# Design notes and bring-up

Prototype only; no PCB or physical validation. Each node uses the pin map and voltage limits in the project README. Keep external switch leads short or add ESD protection and stronger filtering after site testing. Confirm the exact ESP32-C6 module pin table, radio module footprint, antenna keep-out, and regulator stability against vendor datasheets before laying out a board. No mains wiring. Use a fused, certified USB supply.

Commissioning: assemble one node; verify 3.3 V rail under radio transmit; confirm switch states and debounce; test all four slots; verify packet length and sequence handling; simulate loss of gateway/WAN; compare displayed availability with physical stock. Mark the site offline if observations are older than 15 minutes. Human staff resolve mismatches.

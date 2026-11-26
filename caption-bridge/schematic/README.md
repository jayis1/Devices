# Schematic design notes

No KiCad schematic is supplied; this folder is not a fabricated design. Proposed prototype wiring only:

- Hub: Raspberry Pi 4 Model B, USB-C 5 V supply, USB host and Ethernet/Wi-Fi; use official power supply and enclosure/thermal guidance.
- Each microphone sentinel: Pico W + candidate SPH0645LM4H-B I²S MEMS mic breakout. Proposed nets: Pico GP10=BCLK, GP11=WS/LRCLK, GP12=SD; 3V3 and GND as required by the exact breakout. Leave unused SELECT/SEL strap as specified by breakout vendor; do not assume breakout is 5 V tolerant.
- No battery, charging circuit, speaker, or actuator is designed. USB power only.

Before wiring, verify the exact Pico W revision, RP2040 I²S/PWM/PIO implementation, breakout schematic, microphone voltage limits, clock timing, and pin mux. These assignments have not been checked in KiCad ERC and are not production-ready. Firmware in this repo does not drive these pins.

# Bench Mat conceptual schematic

Author: jayis1. Conceptual connection notes, not fabrication-ready KiCad.

USB-C 5 V → polyfuse/TVS → AP2112K 3.3 V. RP2040 ADC0–ADC3 read buffered FSR dividers. GPIO2 reads an opto-isolated external e-stop contact state; it does not connect to or control an e-stop circuit. SX1262 SPI0 GPIO19/16/18, NSS GPIO17, DIO1 GPIO20. Include a calibration button and connectors with clear polarity marking.
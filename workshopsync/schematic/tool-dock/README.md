# Tool Dock conceptual schematic

Author: jayis1. Conceptual connection notes, not fabrication-ready KiCad.

12 V input: 1 A fuse → reverse-polarity MOSFET → 5 V buck → AP2112K 3.3 V. STM32G0B1 PA0 receives isolated SCT-013 conditioner; PA9/PA10 I²C ADXL355; PB7/PB8 are opto-isolated dry-contact inputs. SX1262 SPI1 PA5/PA6/PA7, NSS PA4, DIO1 PB5. No conductor, relay, MOSFET, or connector may switch tool mains power.
# PPE Tag conceptual schematic

Author: jayis1. Conceptual connection notes, not fabrication-ready KiCad.

CR2477 holder → reverse protection → TPS62743 3.0 V. nRF52840 P0.26/P0.27 connect BMI270 I²C; P0.13 has debounced acknowledgement button; P0.14 enables DRV2605L haptic driver. Provide SWD pads and a user-visible low-battery indication. No microphone, GNSS, or location hardware is populated.
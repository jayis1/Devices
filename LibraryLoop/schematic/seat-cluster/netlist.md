# Seat cluster pin-level netlist (conceptual, not KiCad)

Pico/module I²C GP4/GP5 → ADS1115 at 0x48. Four independent 10 kΩ/FSR dividers to A0–A3, with orientation and range confirmed on bench. GP0/GP1 UART to MAX3485 DI/RO; GP2 controls DE+/RE. Node address uses DIP switch GPIOs (not assigned pending module choice). Add bus TVS, selectable 120 Ω end termination, decoupling and strain relief. Validate analog threshold and no ADC input exceeds 3.3 V. Not fabrication-ready.

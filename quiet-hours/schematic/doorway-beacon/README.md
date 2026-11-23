# Pin-level wiring notes (not a KiCad schematic)

Controller: nRF52840 module. Buttons use GPIO P0.11 and P0.12, active-low to GND with pull-ups. Common-anode RGB LED uses P0.13/P0.14/P0.15 through 1 kΩ resistors (verify LED current and module GPIO limits). USB 5 V feeds a regulated development board; LiPo connector only with a protected cell and a suitable charger/power-path module. Expose SWDIO/SWDCLK/GND/3V3 programming pads. Confirm pin map against exact module revision before assembly.

No KiCad project is provided; this proposal has not passed ERC or physical validation.

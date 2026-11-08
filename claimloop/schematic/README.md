# Schematic notes (not KiCad)

These are connection plans, not editable KiCad schematics. Fabrication is explicitly out of scope until an engineer selects exact carrier boards, checks module reference designs, and performs ERC/PCB review.

## Gateway (Pico W)
Use a certified 5 V USB supply into the development board's documented input. RP2040 onboard Wi-Fi carries gateway API traffic. Current firmware has no BLE radio implementation; node-to-gateway BLE remains a planned adapter and must not be considered implemented.

## Intake tray (ESP32-C3 module)
USB 5 V feeds a 3.3 V regulator sized for radio peaks. GPIO4=HX711 DOUT, GPIO5=HX711 SCK, GPIO9=confirmation switch to ground, GPIO8=LED via 1 kΩ. HX711 excitation supply and logic levels must be verified on the chosen breakout; tie analog ground to controller ground at one point. Add local decoupling per module datasheet. Never connect load-cell bridge directly to MCU pins.

## Shelf node (ESP32-C3 module)
GPIO4 reads an NC reed switch to ground with internal or external 10 kΩ pull-up; GPIO8 drives LED through 1 kΩ. USB 5 V to verified 3.3 V regulator; no lock/actuator outputs.

No ERC, PCB layout, RF, thermal, EMC, or physical testing has been performed. Verify exact chip/module pinout, boot straps, USB protection, decoupling, antenna keep-out, and regional radio rules before fabrication.

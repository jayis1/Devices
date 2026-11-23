# Pin-level wiring notes (not a KiCad schematic)

Board: ESP32-C3-MINI-1 module. INMP441: VDD 3V3, GND common, SCK GPIO4, WS GPIO5, SD GPIO6, L/R GND. SSD1306 I2C: SDA GPIO8, SCL GPIO9, address 0x3C, VCC 3V3. Button: GPIO3 to GND with internal pull-up. USB 5 V to 3V3 regulator input; use module development board with regulator and USB-UART for prototype. Add 100 nF local decoupling at microphone and 10 uF bulk at 3V3 rail. GPIO assignments must be checked against exact module/board strapping before PCB design.

No KiCad project is provided; this netlist is a design proposal, not ERC-checked or fabrication-ready.

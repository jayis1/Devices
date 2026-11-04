# DryCycle prototype schematic notes

These pin/net notes are not native KiCad schematics and are not fabrication-ready. Rack scale: four half-bridge load cells wired as a Wheatstone bridge to HX711 E+/E-/A+/A−; HX711 DOUT→ESP32-C3 GPIO5, SCK→GPIO6, common ground. Confirm excitation voltage, cell polarity, module logic level, decoupling and strain relief against exact parts before assembly. Room node: SHT31 VDD 3V3, GND, SDA GPIO4, SCL GPIO5, pull-ups to 3V3 if absent on breakout. Gateway: Pico W powered only from compliant isolated 5 V USB supply. No mains circuitry.

# Schematic specification — capture pending

Not a KiCad schematic and not fabrication validated. Proposed circuit: ESP32-C6 module at 3.3 V; HX711 differential input J2 for 20 kg load cell; confirm switch GPIO6 to ground with internal pull-up; status LED GPIO7 through 1 kΩ; protected battery input J1, regulated 3.3 V rail, 100 nF local decoupling and 10 µF bulk. Add programming UART/reset/boot pads, reviewed charging/protection and reverse polarity protection before PCB. Verify module-specific GPIO mapping against datasheet.

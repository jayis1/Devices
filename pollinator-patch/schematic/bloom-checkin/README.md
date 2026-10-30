# Bloom check-in reference wiring

Not a KiCad schematic. ESP32-C3 GPIO2/3 each connect to a normally-open button to ground using internal pull-up. Optional display I²C uses GPIO6/7 only after verifying controller, address and logic level. Exact display module is unresolved; manual API entry is a valid alternative.

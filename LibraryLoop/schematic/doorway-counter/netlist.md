# Doorway counter pin-level netlist (conceptual, not KiCad)

ESP32-C6-MINI-1 GPIO6/7 I²C to VL53L1X; confirm exact module pin availability and avoid boot strapping pins after module variant selected. GPIO16/17 UART to MAX3485 DI/RO; GPIO18 DE+/RE. Sensor faces floor, not people; aggregate only direction events in volatile memory. USB 5 V input, regulated 3.3 V rail per module spec, local bypassing, connector TVS and bus termination at end. Mounting and sensing geometry must be validated; no emergency use.

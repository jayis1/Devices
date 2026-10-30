# SpokeSense design notes

Prototype netlist, not KiCad capture. Hub: ESP32-S3-DevKitC-1 USB5V; Wi-Fi + BLE, no external sensors. Wheel: ESP32-C3-DevKitM-1 USB5V; BMI270 3V3 I2C SDA6/SCL7, pull-up reed GPIO4 to ground. Stand: ESP32-C3-DevKitM-1 USB5V; BMP280 and NAU7802 I2C SDA6/SCL7, distinct addresses; pressure and load-cell transducers require a separately engineered fixture. Confirm module pin exposure, breakout pull-ups, sensor supply limits, and bus capacitance before wiring. No battery-runtime claim: reference uses USB, and a battery design has not been selected.

Power: USB SELV supplies only; estimate 0.5A available per controller during radio peaks, validate with actual board and radio load. Sensors are low-current, but actual board regulator and peak current are unmeasured. No mains or actuators.

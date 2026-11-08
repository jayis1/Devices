# Dock observer nets

ESP32-C3-MINI-1 GPIO4 SDA and GPIO5 SCL connect to one VL53L0X breakout. GPIO7 connects to a momentary button to ground with pull-up. 2×AA holder → reverse-protection diode → low-quiescent 3.3 V regulator → module and sensor. Add 100 nF + 10 µF bypass and verify BLE burst current against regulator droop. No electrical connection to charger. Confirm selected module's GPIO and sensor breakout pull-ups before layout.

# Tray node nets

ESP32-C3-MINI-1 GPIO4 SDA and GPIO5 SCL connect to NAU7802 SDA/SCL and VL53L0X SDA/SCL; pull-ups to 3V3 on only one module. GPIO6 drives VL53L0X XSHUT. Four matched bar-cell bridge elements connect as a full bridge to NAU7802 differential inputs; confirm excitation voltage and input common-mode against the selected breakout. 3V3 regulator supplies ESP module and logic; USB 5V input, reverse protection, 100 nF and 10 µF local bypass. Connector polarity and strain relief required. Verify module pin constraints and breakout circuitry before layout.

# Hub netlist (design notes, not KiCad)
ESP32-S3 module 3V3/GND; USB-C 5V into protected 3.3V regulator; I2C SDA GPIO8 / SCL GPIO9 with 4.7k pullups to 3V3 to SSD1306 OLED; UART0 GPIO43 TX/GPIO44 RX debug; BLE via onboard radio/antenna. Add local decoupling and ESD protection after board selection.
